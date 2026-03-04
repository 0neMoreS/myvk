#include "LightsManager.hpp"

#include "RTG.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

	void compute_cascade_world_corners(
		const glm::vec3& camera_position,
		const glm::vec3& camera_forward,
		const glm::vec3& camera_up,
		float camera_fov,
		float camera_aspect,
		float cascade_near,
		float cascade_far,
		std::array<glm::vec3, 8>& out_corners
	) {
		const glm::vec3 forward = glm::normalize(camera_forward);
		const glm::vec3 right = glm::normalize(glm::cross(forward, camera_up));
		const glm::vec3 up = glm::normalize(glm::cross(right, forward));

		const float tan_half_fov = std::tan(0.5f * camera_fov);
		const float near_h = tan_half_fov * cascade_near;
		const float near_w = near_h * camera_aspect;
		const float far_h = tan_half_fov * cascade_far;
		const float far_w = far_h * camera_aspect;

		const glm::vec3 near_center = camera_position + forward * cascade_near;
		const glm::vec3 far_center = camera_position + forward * cascade_far;

		out_corners[0] = near_center + up * near_h - right * near_w;
		out_corners[1] = near_center + up * near_h + right * near_w;
		out_corners[2] = near_center - up * near_h - right * near_w;
		out_corners[3] = near_center - up * near_h + right * near_w;
		out_corners[4] = far_center + up * far_h - right * far_w;
		out_corners[5] = far_center + up * far_h + right * far_w;
		out_corners[6] = far_center - up * far_h - right * far_w;
		out_corners[7] = far_center - up * far_h + right * far_w;
	}

	glm::mat4 compute_sun_cascade_matrix(
		const glm::vec3& light_direction,
		const std::array<glm::vec3, 8>& corners
	) {
		glm::vec3 centroid(0.0f);
		for (const auto& c : corners) centroid += c;
		centroid /= float(corners.size());

		const glm::vec3 light_dir = glm::normalize(light_direction);
		const glm::vec3 world_up = (std::abs(glm::dot(light_dir, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f)
			? glm::vec3(1.0f, 0.0f, 0.0f)
			: glm::vec3(0.0f, 1.0f, 0.0f);

		float radius = 0.0f;
		for (const auto& c : corners) {
			radius = std::max(radius, glm::length(c - centroid));
		}

		const glm::vec3 light_pos = centroid - light_dir * (radius + 50.0f);
		const glm::mat4 light_view = glm::lookAtRH(light_pos, centroid, world_up);

		glm::vec3 min_v(std::numeric_limits<float>::max());
		glm::vec3 max_v(std::numeric_limits<float>::lowest());
		for (const auto& c : corners) {
			glm::vec3 ls = glm::vec3(light_view * glm::vec4(c, 1.0f));
			min_v = glm::min(min_v, ls);
			max_v = glm::max(max_v, ls);
		}

		const float z_pad = 100.0f;
		const glm::mat4 light_proj = glm::orthoRH_ZO(
			min_v.x,
			max_v.x,
			min_v.y,
			max_v.y,
			std::max(0.01f, -max_v.z - z_pad),
			std::max(0.02f, -min_v.z + z_pad)
		);

		glm::mat4 result = light_proj * light_view;
		result[1][1] *= -1.0f;
		return result;
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
	const CameraManager::Camera& camera
) {
	const std::array<float, SunCascadeCount> splits = compute_sun_cascade_splits(camera.camera_near, camera.camera_far);

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
				for (uint32_t i = 0; i < SunCascadeCount; ++i) {
					dst.cascadeSplits[i] = -splits[SunCascadeCount - 1 - i];
				}

				float cascade_near = std::max(0.01f, camera.camera_near);
				for (uint32_t cascade = 0; cascade < SunCascadeCount; ++cascade) {
					const float cascade_far = splits[cascade];
					std::array<glm::vec3, 8> frustum_corners{};
					compute_cascade_world_corners(
						camera.camera_position,
						camera.camera_forward,
						camera.camera_up,
						camera.camera_fov,
						camera.aspect,
						cascade_near,
						cascade_far,
						frustum_corners
					);

					const uint32_t shader_cascade_index = SunCascadeCount - 1 - cascade;
					dst.orthographic[shader_cascade_index] = compute_sun_cascade_matrix(direction, frustum_corners);
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
