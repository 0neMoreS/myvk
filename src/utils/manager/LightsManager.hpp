#pragma once

#include "S72Loader.hpp"
#include "SceneTree.hpp"
#include "A3CommonData.hpp"
#include "CameraManager.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <memory>

class LightsManager {
public:
	LightsManager() = default;
	~LightsManager() = default;

	void create(
		const std::shared_ptr<S72Loader::Document>& doc,
		const std::vector<SceneTree::LightTreeData>& light_tree_data
	);

	void update(
		const std::shared_ptr<S72Loader::Document>& doc,
		const std::vector<SceneTree::LightTreeData>& light_tree_data,
		const CameraManager& camera_manager
	);

	const std::vector<A3CommonData::SunLight>& get_sun_lights() const { return sun_lights; }
	const std::vector<A3CommonData::SphereLight>& get_sphere_lights() const { return sphere_lights; }
	const std::vector<A3CommonData::SpotLight>& get_spot_lights() const { return spot_lights; }
	const std::vector<A3CommonData::SunLight>& get_shadow_sun_lights() const { return shadow_sun_lights; }
	const std::vector<A3CommonData::SphereLight>& get_shadow_sphere_lights() const { return shadow_sphere_lights; }
	const std::vector<A3CommonData::SpotLight>& get_shadow_spot_lights() const { return shadow_spot_lights; }

	const std::vector<uint8_t>& get_sun_lights_bytes() const { return sun_lights_bytes; }
	const std::vector<uint8_t>& get_sphere_lights_bytes() const { return sphere_lights_bytes; }
	const std::vector<uint8_t>& get_spot_lights_bytes() const { return spot_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_sun_lights_bytes() const { return shadow_sun_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_sphere_lights_bytes() const { return shadow_sphere_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_spot_lights_bytes() const { return shadow_spot_lights_bytes; }

	VkDeviceSize get_sun_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(sun_lights_bytes.size()); }
	VkDeviceSize get_sphere_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(sphere_lights_bytes.size()); }
	VkDeviceSize get_spot_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(spot_lights_bytes.size()); }
	VkDeviceSize get_shadow_sun_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_sun_lights_bytes.size()); }
	VkDeviceSize get_shadow_sphere_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_sphere_lights_bytes.size()); }
	VkDeviceSize get_shadow_spot_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_spot_lights_bytes.size()); }

private:
	std::vector<A3CommonData::SunLight> sun_lights;
	std::vector<A3CommonData::SphereLight> sphere_lights;
	std::vector<A3CommonData::SpotLight> spot_lights;
	std::vector<A3CommonData::SunLight> shadow_sun_lights;
	std::vector<A3CommonData::SphereLight> shadow_sphere_lights;
	std::vector<A3CommonData::SpotLight> shadow_spot_lights;

	std::vector<uint8_t> sun_lights_bytes;
	std::vector<uint8_t> sphere_lights_bytes;
	std::vector<uint8_t> spot_lights_bytes;
	std::vector<uint8_t> shadow_sun_lights_bytes;
	std::vector<uint8_t> shadow_sphere_lights_bytes;
	std::vector<uint8_t> shadow_spot_lights_bytes;
};
