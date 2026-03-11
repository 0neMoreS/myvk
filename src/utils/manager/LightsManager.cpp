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
	constexpr uint32_t SphereShadowFaceCount = 6;

	template< typename LightsT >
	void init_lights_bytes(const LightsT& lights, std::vector<uint8_t>& bytes) {
		using LightT = typename std::decay_t<LightsT>::value_type;
		const size_t header_size = sizeof(LightsManager::LightsHeader);
		const size_t payload_size = sizeof(LightT) * lights.size();
		assert(bytes.size() == header_size + payload_size);

		LightsManager::LightsHeader header{};
		header.count = static_cast<uint32_t>(lights.size());
		std::memcpy(bytes.data(), &header, header_size);
		if (!lights.empty()) {
			std::memcpy(bytes.data() + header_size, lights.data(), payload_size);
		}
	}

	template< typename LightsT >
	void overwrite_lights_payload(const LightsT& lights, std::vector<uint8_t>& bytes) {
		using LightT = typename std::decay_t<LightsT>::value_type;
		const size_t header_size = sizeof(LightsManager::LightsHeader);
		const size_t payload_size = sizeof(LightT) * lights.size();
		assert(bytes.size() == header_size + payload_size);
		if (!lights.empty()) {
			std::memcpy(bytes.data() + header_size, lights.data(), payload_size);
		}
	}

	std::array<float, SunCascadeCount> compute_sun_cascade_splits(float near_plane, float far_plane) {
		std::array<float, SunCascadeCount> splits{};
		for (uint32_t i = 0; i < SunCascadeCount; ++i) {
			const float p = float(i + 1) / float(SunCascadeCount);
			const float log_split = near_plane * std::pow(far_plane / near_plane, p);
			const float uni_split = near_plane + (far_plane - near_plane) * p;
			const float split = SunCascadeLambda * log_split + (1.0f - SunCascadeLambda) * uni_split;
			splits[i] = split;
		}
		return splits;
	}

	std::array<glm::mat4, SphereShadowFaceCount> compute_sphere_shadow_face_pv(
		const glm::vec3& position,
		float near_plane,
		float far_plane
	) {
		const glm::mat4 proj = [&]() {
			glm::mat4 p = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, near_plane, far_plane);
			// p[1][1] *= -1.0f; // Y-flip twic in CPU and GPU, so it cancels out and we get correct orientation without flipping here

			// reverse-z, same remap used by sun/spot shadow matrices
			p[0][2] = p[0][3] - p[0][2];
			p[1][2] = p[1][3] - p[1][2];
			p[2][2] = p[2][3] - p[2][2];
			p[3][2] = p[3][3] - p[3][2];
			return p;
		}();

		const std::array<glm::vec3, SphereShadowFaceCount> face_dirs = {
			glm::vec3(  1.0f, 0.0f, 0.0f ), // Back
			glm::vec3( -1.0f, 0.0f, 0.0f ), // Front
			glm::vec3( 0.0f, 1.0f, 0.0f ), // Bottom
			glm::vec3( 0.0f, -1.0f, 0.0f ), // Top
			glm::vec3( 0.0f,  0.0f,  1.0f), // Left
			glm::vec3( 0.0f,  0.0f, -1.0f), // Right
		};

		const std::array<glm::vec3, SphereShadowFaceCount> face_ups = {
			glm::vec3(0.0f, -1.0f, 0.0f), // Back
			glm::vec3(0.0f, -1.0f, 0.0f), // Front
			glm::vec3(0.0f, 0.0f, 1.0f), // Bottom
			glm::vec3(0.0f, 0.0f, -1.0f), // Top
			glm::vec3(0.0f, -1.0f,  0.0f), // Left
			glm::vec3(0.0f, -1.0f,  0.0f), // Right
		};

		std::array<glm::mat4, SphereShadowFaceCount> face_pv{};
		for (uint32_t face = 0; face < SphereShadowFaceCount; ++face) {
			const glm::mat4 view = glm::lookAtRH(position, position + face_dirs[face], face_ups[face]);
			face_pv[face] = proj * view;
		}

		return face_pv;
	}

	// glm::mat4 lightspace_PV(const CameraManager& camera_manager, const float near_plane, const float far_plane, const glm::vec3& light_dir) {
		
	// 	// ------------------------------------------------------------------
	// 	// 1. Get the world-space corners of the camera's view frustum
	// 	// ------------------------------------------------------------------
	// 	// glm::mat4 camProj = camera_manager.get_perspective();
	// 	CameraManager::Camera active_camera = camera_manager.get_active_camera();
	// 	glm::mat4 camProj = glm::perspectiveRH_ZO(active_camera.camera_fov, active_camera.aspect, near_plane, far_plane);
    // 	camProj[1][1] *= -1.0f;
	// 	glm::mat4 camView = camera_manager.get_view();

	// 	// inverse of (Projection * View) transforms from NDC space back to world space
	// 	glm::mat4 invCamVP = glm::inverse(camProj * camView);

	// 	// Vulkan NDC space has X: [-1, 1], Y: [1, -1], Z: [0, 1]
	// 	std::vector<glm::vec4> ndcCorners = {
	// 		{-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, // Near plane
	// 		{-1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}  // Far plane
	// 	};

	// 	std::vector<glm::vec3> cornersWorld(8);
	// 	glm::vec3 frustumCenter(0.0f);

	// 	for (int i = 0; i < 8; ++i) {
	// 		glm::vec4 pt = invCamVP * ndcCorners[i];
	// 		cornersWorld[i] = glm::vec3(pt) / pt.w; // projective divide
	// 		frustumCenter += cornersWorld[i];
	// 	}
	// 	frustumCenter /= 8.0f;

	// 	// 2. Light view matrix
	// 	glm::vec3 lightPos = frustumCenter - light_dir; 
	// 	glm::vec3 up = camera_manager.get_active_camera().world_up;

	// 	glm::mat4 lightView = glm::lookAtRH(lightPos, frustumCenter, up);

	// 	// 3. Compute the axis-aligned bounding box (AABB) of the frustum in light space
	// 	float minX = std::numeric_limits<float>::max();
	// 	float maxX = std::numeric_limits<float>::lowest();
	// 	float minY = std::numeric_limits<float>::max();
	// 	float maxY = std::numeric_limits<float>::lowest();
	// 	float minZ = std::numeric_limits<float>::max();
	// 	float maxZ = std::numeric_limits<float>::lowest();

	// 	for (const auto& corner : cornersWorld) {
	// 		glm::vec4 trf = lightView * glm::vec4(corner, 1.0f);
	// 		minX = std::min(minX, trf.x);
	// 		maxX = std::max(maxX, trf.x);
	// 		minY = std::min(minY, trf.y);
	// 		maxY = std::max(maxY, trf.y);
	// 		minZ = std::min(minZ, trf.z);
	// 		maxZ = std::max(maxZ, trf.z);
	// 	}

	// 	// 4. Create the orthographic projection matrix for the light
	// 	float range = maxZ - minZ;
	// 	maxZ += range * 2.0f;
	// 	minZ -= range * 2.0f;

	// 	// minY += range * 0.5f;
	// 	// maxY -= range * 0.5f;
	// 	// minX += range * 0.5f;
	// 	// maxX -= range * 0.5f;

	// 	glm::mat4 lightProj = glm::orthoRH_ZO(minX, maxX, minY, maxY, minZ, maxZ);
	// 	lightProj[1][1] *= -1.0f;

	// 	return lightProj * lightView;
	// }

	glm::mat4 lightspace_PV(const CameraManager& camera_manager, const float near_plane, const float far_plane, const glm::vec3& light_dir, uint32_t shadow_map_resolution) {
		// ------------------------------------------------------------------
		// 1. Get the 8 world-space corners of the camera frustum and compute its center
		// ------------------------------------------------------------------
		CameraManager::Camera active_camera = camera_manager.get_active_camera();
		glm::mat4 camProj = glm::perspectiveRH_ZO(active_camera.camera_fov, active_camera.aspect, near_plane, far_plane);
		camProj[1][1] *= -1.0f; // Vulkan Y-flip
		glm::mat4 camView = camera_manager.get_view();

		glm::mat4 invCamVP = glm::inverse(camProj * camView);

		// Vulkan NDC space
		std::vector<glm::vec4> ndcCorners = {
			{-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f},
			{-1.0f, -1.0f, 1.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}
		};

		std::vector<glm::vec3> cornersWorld(8);
		glm::vec3 frustumCenter(0.0f);

		for (int i = 0; i < 8; ++i) {
			glm::vec4 pt = invCamVP * ndcCorners[i];
			cornersWorld[i] = glm::vec3(pt) / pt.w; 
			frustumCenter += cornersWorld[i];
		}
		frustumCenter /= 8.0f;

		// ------------------------------------------------------------------
		// 2. Compute a stable bounding sphere
		// ------------------------------------------------------------------
		// Use the farthest corner distance from the center as the sphere radius
		float radius = 0.0f;
		for (int i = 0; i < 8; ++i) {
			float distance = glm::length(cornersWorld[i] - frustumCenter);
			radius = std::max(radius, distance);
		}
		
		// Slightly round up to reduce edge precision issues (optional refinement)
		radius = std::ceil(radius * 16.0f) / 16.0f;

		// ------------------------------------------------------------------
		// 3. Build the base light view matrix
		// ------------------------------------------------------------------
		glm::vec3 L = glm::normalize(light_dir);
		// Choose a safe up vector to avoid NaN in glm::lookAt when light points nearly vertical
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

		glm::vec3 lightPos = frustumCenter - L * radius; 
		glm::mat4 lightView = glm::lookAtRH(lightPos, frustumCenter, up);

		// ------------------------------------------------------------------
		// 4. Perform texel snapping, the key step to reduce translational shadow shimmering
		// ------------------------------------------------------------------
		// Convert one shadow texel to a world-space distance
		float texelsPerUnit = static_cast<float>(shadow_map_resolution) / (radius * 2.0f);

		// Scale into texel space
		glm::mat4 scaleToTexelSpace = glm::scale(glm::mat4(1.0f), glm::vec3(texelsPerUnit));
		lightView = scaleToTexelSpace * lightView;

		// Snap translation to integer texel steps
		lightView[3][0] = std::floor(lightView[3][0]);
		lightView[3][1] = std::floor(lightView[3][1]);
		// Keep Z (depth) continuous for stable depth comparisons

		// Scale back to world space
		glm::mat4 scaleBackToWorld = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / texelsPerUnit));
		lightView = scaleBackToWorld * lightView;

		// ------------------------------------------------------------------
		// 5. Build a fixed orthographic projection
		// ------------------------------------------------------------------
		// Orthographic width/height are fixed to the sphere diameter (radius * 2), independent of camera rotation
		// Extend Z range (near/far) to include casters outside the camera frustum but still affecting visible shadows
		float zMultiplier = 5.0f; // 10x radius usually covers most scenes

		glm::mat4 lightProj = glm::orthoRH_ZO(
			-radius, radius,          // Left, Right
			-radius, radius,          // Bottom, Top
			-radius * zMultiplier,    // Near (extends far behind the light)
			radius * zMultiplier      // Far  (extends far in front of the light)
		);
		
		// Vulkan Y-flip
		lightProj[1][1] *= -1.0f;

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
	shadow_sphere_matrices.clear();

	sun_lights.reserve(light_tree_data.size());
	sphere_lights.reserve(light_tree_data.size());
	spot_lights.reserve(light_tree_data.size());
	shadow_sun_lights.reserve(light_tree_data.size());
	shadow_sphere_lights.reserve(light_tree_data.size());
	shadow_spot_lights.reserve(light_tree_data.size());
	shadow_sphere_matrices.reserve(light_tree_data.size());

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
			SunLight dst{};
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
			SphereLight dst{};
			dst.position = position;
			dst.radius = src_light.sphere->radius;
			dst.tint = src_light.tint * src_light.sphere->power;
			dst.limit = src_light.sphere->limit.value_or(2.0f * std::sqrt(src_light.sphere->power / (4.0f * std::numbers::pi_v<float>) * 256.0f));
			dst.shadow = static_cast<int32_t>(src_light.shadow);
			if (has_shadow) {
				shadow_sphere_lights.emplace_back(std::move(dst));
			}
			else sphere_lights.emplace_back(std::move(dst));
		}

		if (src_light.spot) {
			SpotLight dst{};
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

	sun_lights_bytes.assign(static_cast<size_t>(sun_lights_buffer_size(static_cast<uint32_t>(sun_lights.size()))), 0);
	sphere_lights_bytes.assign(static_cast<size_t>(sphere_lights_buffer_size(static_cast<uint32_t>(sphere_lights.size()))), 0);
	spot_lights_bytes.assign(static_cast<size_t>(spot_lights_buffer_size(static_cast<uint32_t>(spot_lights.size()))), 0);
	shadow_sun_lights_bytes.assign(static_cast<size_t>(sun_lights_buffer_size(static_cast<uint32_t>(shadow_sun_lights.size()))), 0);
	shadow_sphere_lights_bytes.assign(static_cast<size_t>(sphere_lights_buffer_size(static_cast<uint32_t>(shadow_sphere_lights.size()))), 0);
	shadow_spot_lights_bytes.assign(static_cast<size_t>(spot_lights_buffer_size(static_cast<uint32_t>(shadow_spot_lights.size()))), 0);

	init_lights_bytes(sun_lights, sun_lights_bytes);
	init_lights_bytes(sphere_lights, sphere_lights_bytes);
	init_lights_bytes(spot_lights, spot_lights_bytes);
	init_lights_bytes(shadow_sun_lights, shadow_sun_lights_bytes);
	init_lights_bytes(shadow_sphere_lights, shadow_sphere_lights_bytes);
	init_lights_bytes(shadow_spot_lights, shadow_spot_lights_bytes);

	shadow_sphere_matrices.resize(shadow_sphere_lights.size());
	shadow_sphere_matrices_bytes.assign(static_cast<size_t>(sphere_shadow_matrices_buffer_size(static_cast<uint32_t>(shadow_sphere_matrices.size()))), 0);
	init_lights_bytes(shadow_sphere_matrices, shadow_sphere_matrices_bytes);
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
				float cascade_near = camera_manager.get_active_camera().camera_near;

				for (uint32_t cascade = 0; cascade < SunCascadeCount; ++cascade) {
					dst.cascadeSplits[cascade] = splits[cascade];
					const float cascade_far = splits[cascade];
					dst.orthographic[cascade] = lightspace_PV(camera_manager, cascade_near, cascade_far, direction, src_light.shadow);
					
					// revers-z
					dst.orthographic[cascade][0][2] = dst.orthographic[cascade][0][3] - dst.orthographic[cascade][0][2];
					dst.orthographic[cascade][1][2] = dst.orthographic[cascade][1][3] - dst.orthographic[cascade][1][2];
					dst.orthographic[cascade][2][2] = dst.orthographic[cascade][2][3] - dst.orthographic[cascade][2][2];
					dst.orthographic[cascade][3][2] = dst.orthographic[cascade][3][3] - dst.orthographic[cascade][3][2];

					cascade_near = cascade_far;
				}
			}
		}

		if (src_light.sphere) {
			auto& dst = has_shadow ? shadow_sphere_lights.at(shadow_sphere_idx++) : sphere_lights.at(sphere_idx++);
			dst.position = position;
			dst.shadow = static_cast<int32_t>(src_light.shadow);

			if (has_shadow) {
				// Get this parameter from LearnOpenGL
				const float near_plane = 1.0f;
				const float far_plane = 25.0f; // hard coded for now
				auto& sphere_shadow = shadow_sphere_matrices.at(shadow_sphere_idx - 1);
				sphere_shadow.face_pv = compute_sphere_shadow_face_pv(dst.position, near_plane, far_plane);
			}
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

			// revers-z
			proj[0][2] = proj[0][3] - proj[0][2];
			proj[1][2] = proj[1][3] - proj[1][2];
			proj[2][2] = proj[2][3] - proj[2][2];
			proj[3][2] = proj[3][3] - proj[3][2];
			
			dst.perspective = proj * view;
		}
	}

	overwrite_lights_payload(sun_lights, sun_lights_bytes);
	overwrite_lights_payload(sphere_lights, sphere_lights_bytes);
	overwrite_lights_payload(spot_lights, spot_lights_bytes);
	overwrite_lights_payload(shadow_sun_lights, shadow_sun_lights_bytes);
	overwrite_lights_payload(shadow_sphere_lights, shadow_sphere_lights_bytes);
	overwrite_lights_payload(shadow_spot_lights, shadow_spot_lights_bytes);
	overwrite_lights_payload(shadow_sphere_matrices, shadow_sphere_matrices_bytes);
}
