#include "LightsManager.hpp"

#include "RTG.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <numbers>
#include <type_traits>

namespace {
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
	const std::vector<SceneTree::LightTreeData>& light_tree_data
) {
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
