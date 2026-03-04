#include "LightsManager.hpp"

#include "RTG.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <numbers>
#include <type_traits>

namespace {
	constexpr uint32_t SunCascadeCount = 4;
	constexpr float SunCascadeLambda = 0.75f;

	template< typename LightsT >
	void init_lights_bytes(const LightsT& lights, std::vector<uint8_t>& bytes) {
		using LightT = typename std::decay_t<LightsT>::value_type;
		const size_t header_size = sizeof(A3CommonData::LightsHeader);
		const size_t payload_size = sizeof(LightT) * lights.size();
		assert(bytes.size() == header_size + payload_size);

		A3CommonData::LightsHeader header{};
		header.count = static_cast<uint32_t>(lights.size());
		std::memcpy(bytes.data(), &header, header_size);
		if (!lights.empty()) {
			std::memcpy(bytes.data() + header_size, lights.data(), payload_size);
		}
	}

	template< typename LightsT >
	void overwrite_lights_payload(const LightsT& lights, std::vector<uint8_t>& bytes) {
		using LightT = typename std::decay_t<LightsT>::value_type;
		const size_t header_size = sizeof(A3CommonData::LightsHeader);
		const size_t payload_size = sizeof(LightT) * lights.size();
		assert(bytes.size() == header_size + payload_size);
		if (!lights.empty()) {
			std::memcpy(bytes.data() + header_size, lights.data(), payload_size);
		}
	}

	std::array<float, SunCascadeCount> compute_sun_cascade_splits(float near_plane, float far_plane) {
		std::array<float, SunCascadeCount> splits{};
		const float n = std::max(0.01f, near_plane);
		const float f = std::max(n + 0.01f, far_plane);
		for (uint32_t i = 0; i < SunCascadeCount; ++i) {
			const float p = float(i + 1) / float(SunCascadeCount);
			const float log_split = n * std::pow(f / n, p);
			const float uni_split = n + (f - n) * p;
			const float split = SunCascadeLambda * log_split + (1.0f - SunCascadeLambda) * uni_split;
			splits[i] = split;
		}
		return splits;
	}

	glm::mat4 lightspace_PV(const CameraManager& camera_manager, const glm::vec3& light_dir) {
		
		// ------------------------------------------------------------------
		// 1. Get the world-space corners of the camera's view frustum
		// ------------------------------------------------------------------
		glm::mat4 camProj = camera_manager.get_perspective();
		glm::mat4 camView = camera_manager.get_view();

		// inverse of (Projection * View) transforms from NDC space back to world space
		glm::mat4 invCamVP = glm::inverse(camProj * camView);

		// Vulkan NDC space has X: [-1, 1], Y: [1, -1], Z: [0, 1]
		std::vector<glm::vec4> ndcCorners = {
			{-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, // Near plane
			{-1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}  // Far plane
		};

		std::vector<glm::vec3> cornersWorld(8);
		glm::vec3 frustumCenter(0.0f);

		for (int i = 0; i < 8; ++i) {
			glm::vec4 pt = invCamVP * ndcCorners[i];
			cornersWorld[i] = glm::vec3(pt) / pt.w; // projective divide
			frustumCenter += cornersWorld[i];
		}
		frustumCenter /= 8.0f; // frustum center in world space

		// 2. Light view matrix
		glm::vec3 lightPos = frustumCenter - light_dir; 
		
		// avoiding singularity when light direction is parallel to camera's world up vector
		glm::vec3 up = camera_manager.get_active_camera().world_up;

		glm::mat4 lightView = glm::lookAtRH(lightPos, frustumCenter, up);

		// 3. Compute the axis-aligned bounding box (AABB) of the frustum in light space
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();

		for (const auto& corner : cornersWorld) {
			glm::vec4 trf = lightView * glm::vec4(corner, 1.0f);
			minX = std::min(minX, trf.x);
			maxX = std::max(maxX, trf.x);
			minY = std::min(minY, trf.y);
			maxY = std::max(maxY, trf.y);
			minZ = std::min(minZ, trf.z);
			maxZ = std::max(maxZ, trf.z);
		}

		// 4. Create the orthographic projection matrix for the light
		float zNearDistance = -maxZ;
		float zFarDistance = -minZ;

		float zExtension = 150.0f; // huristic extension to ensure the shadow map covers objects slightly beyond the frustum
		zNearDistance -= zExtension;

		glm::mat4 lightProj = glm::orthoRH_ZO(minX, maxX, minY, maxY, zNearDistance, zFarDistance);
		lightProj[1][1] *= -1.0f;

		// return light space matrix
		return lightProj * lightView;
	}
}

void LightsManager::create(
	const std::shared_ptr<S72Loader::Document>& doc,
	const std::vector<SceneTree::LightTreeData>& light_tree_data
) {
	sun_lights.clear();
	sphere_lights.clear();
	spot_lights.clear();
	shadow_sun_lights.clear();
	shadow_sphere_lights.clear();
	shadow_spot_lights.clear();

	sun_lights.reserve(light_tree_data.size());
	sphere_lights.reserve(light_tree_data.size());
	spot_lights.reserve(light_tree_data.size());
	shadow_sun_lights.reserve(light_tree_data.size());
	shadow_sphere_lights.reserve(light_tree_data.size());
	shadow_spot_lights.reserve(light_tree_data.size());

	for (const auto& ltd : light_tree_data) {
		if (ltd.light_index >= doc->lights.size()) continue;

		const auto& src_light = doc->lights[ltd.light_index];
		const bool has_shadow = (src_light.shadow != 0);
		const glm::mat4 transform = ltd.model_matrix;
		const glm::mat3 blender_rotation = glm::mat3(transform);
		const glm::vec3 blender_forward = blender_rotation * glm::vec3{0.0f, 0.0f, -1.0f};
		const glm::vec3 position = BLENDER_TO_VULKAN_3 * glm::vec3{transform[3][0], transform[3][1], transform[3][2]};
		const glm::vec3 direction = glm::normalize(BLENDER_TO_VULKAN_3 * blender_forward);

		if (src_light.sun) {
			A3CommonData::SunLight dst{};
			for (int i = 0; i < 4; ++i) dst.cascadeSplits[i] = 0.0f;
			for (int i = 0; i < 4; ++i) dst.orthographic[i] = glm::mat4(1.0f);
			dst.direction = direction;
			dst.angle = src_light.sun->angle;
			dst.tint = src_light.tint * src_light.sun->strength;
			dst.shadow = static_cast<int32_t>(src_light.shadow);
			if (has_shadow) shadow_sun_lights.emplace_back(std::move(dst));
			else sun_lights.emplace_back(std::move(dst));
		}

		if (src_light.sphere) {
			A3CommonData::SphereLight dst{};
			dst.position = position;
			dst.radius = src_light.sphere->radius;
			dst.tint = src_light.tint * src_light.sphere->power;
			dst.limit = src_light.sphere->limit.value_or(2.0f * std::sqrt(src_light.sphere->power / (4.0f * std::numbers::pi_v<float>) * 256.0f));
			if (has_shadow) shadow_sphere_lights.emplace_back(std::move(dst));
			else sphere_lights.emplace_back(std::move(dst));
		}

		if (src_light.spot) {
			A3CommonData::SpotLight dst{};
			dst.perspective = glm::mat4(1.0f);
			dst.position = position;
			dst.radius = src_light.spot->radius;
			dst.direction = direction;
			dst.fov = src_light.spot->fov;
			dst.tint = src_light.tint * src_light.spot->power;
			dst.blend = src_light.spot->blend;
			dst.limit = src_light.spot->limit.value_or(2.0f * std::sqrt(src_light.spot->power / (4.0f * std::numbers::pi_v<float>) * 256.0f));
			dst.shadow = static_cast<int32_t>(src_light.shadow);
			if (has_shadow) shadow_spot_lights.emplace_back(std::move(dst));
			else spot_lights.emplace_back(std::move(dst));
		}
	}

	sun_lights_bytes.assign(static_cast<size_t>(A3CommonData::sun_lights_buffer_size(static_cast<uint32_t>(sun_lights.size()))), 0);
	sphere_lights_bytes.assign(static_cast<size_t>(A3CommonData::sphere_lights_buffer_size(static_cast<uint32_t>(sphere_lights.size()))), 0);
	spot_lights_bytes.assign(static_cast<size_t>(A3CommonData::spot_lights_buffer_size(static_cast<uint32_t>(spot_lights.size()))), 0);
	shadow_sun_lights_bytes.assign(static_cast<size_t>(A3CommonData::sun_lights_buffer_size(static_cast<uint32_t>(shadow_sun_lights.size()))), 0);
	shadow_sphere_lights_bytes.assign(static_cast<size_t>(A3CommonData::sphere_lights_buffer_size(static_cast<uint32_t>(shadow_sphere_lights.size()))), 0);
	shadow_spot_lights_bytes.assign(static_cast<size_t>(A3CommonData::spot_lights_buffer_size(static_cast<uint32_t>(shadow_spot_lights.size()))), 0);

	init_lights_bytes(sun_lights, sun_lights_bytes);
	init_lights_bytes(sphere_lights, sphere_lights_bytes);
	init_lights_bytes(spot_lights, spot_lights_bytes);
	init_lights_bytes(shadow_sun_lights, shadow_sun_lights_bytes);
	init_lights_bytes(shadow_sphere_lights, shadow_sphere_lights_bytes);
	init_lights_bytes(shadow_spot_lights, shadow_spot_lights_bytes);
}

void LightsManager::update(
	const std::shared_ptr<S72Loader::Document>& doc,
	const std::vector<SceneTree::LightTreeData>& light_tree_data,
	const CameraManager& camera_manager
) {
	const std::array<float, SunCascadeCount> splits = compute_sun_cascade_splits(camera_manager.get_active_camera().camera_near, camera_manager.get_active_camera().camera_far);

	size_t sun_idx = 0;
	size_t sphere_idx = 0;
	size_t spot_idx = 0;
	size_t shadow_sun_idx = 0;
	size_t shadow_sphere_idx = 0;
	size_t shadow_spot_idx = 0;

	for (const auto& ltd : light_tree_data) {
		if (ltd.light_index >= doc->lights.size()) continue;

		const auto& src_light = doc->lights[ltd.light_index];
		const bool has_shadow = (src_light.shadow != 0);
		const glm::mat4 transform = ltd.model_matrix;
		const glm::mat3 blender_rotation = glm::mat3(transform);
		const glm::vec3 blender_forward = blender_rotation * glm::vec3{0.0f, 0.0f, -1.0f};
		const glm::vec3 position = BLENDER_TO_VULKAN_3 * glm::vec3{transform[3][0], transform[3][1], transform[3][2]};
		const glm::vec3 direction = glm::normalize(BLENDER_TO_VULKAN_3 * blender_forward);
		const glm::vec3 up = BLENDER_TO_VULKAN_3 * blender_rotation * glm::vec3{0.0f, 1.0f, 0.0f};

		if (src_light.sun) {
			auto& dst = has_shadow ? shadow_sun_lights.at(shadow_sun_idx++) : sun_lights.at(sun_idx++);
			dst.direction = direction;
			if (has_shadow) {
				float cascade_near = std::max(0.01f, camera_manager.get_active_camera().camera_near);

				for (uint32_t i = 0; i < SunCascadeCount; ++i) {
					dst.cascadeSplits[i] = -splits[SunCascadeCount - 1 - i];
				}

				for (uint32_t cascade = 0; cascade < SunCascadeCount; ++cascade) {
					const float cascade_far = splits[cascade];

					const uint32_t shader_cascade_index = SunCascadeCount - 1 - cascade;
					dst.orthographic[shader_cascade_index] = lightspace_PV(camera_manager, direction);
					cascade_near = cascade_far;
				}
			}
		}

		if (src_light.sphere) {
			auto& dst = has_shadow ? shadow_sphere_lights.at(shadow_sphere_idx++) : sphere_lights.at(sphere_idx++);
			dst.position = position;
		}

		if (src_light.spot) {
			auto& dst = has_shadow ? shadow_spot_lights.at(shadow_spot_idx++) : spot_lights.at(spot_idx++);
			dst.position = position;
			dst.direction = direction;
			const float near_plane = 0.1f;
			const float far_plane = dst.limit;
			glm::mat4 view = glm::lookAtRH(position, position + direction, up);
			glm::mat4 proj = glm::perspectiveRH_ZO(dst.fov, 1.0f, near_plane, far_plane);
			proj[1][1] *= -1.0f;
			dst.perspective = proj * view;
		}
	}

	overwrite_lights_payload(sun_lights, sun_lights_bytes);
	overwrite_lights_payload(sphere_lights, sphere_lights_bytes);
	overwrite_lights_payload(spot_lights, spot_lights_bytes);
	overwrite_lights_payload(shadow_sun_lights, shadow_sun_lights_bytes);
	overwrite_lights_payload(shadow_sphere_lights, shadow_sphere_lights_bytes);
	overwrite_lights_payload(shadow_spot_lights, shadow_spot_lights_bytes);
}
