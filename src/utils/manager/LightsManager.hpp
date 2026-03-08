#pragma once

#include "S72Loader.hpp"
#include "SceneTree.hpp"
#include "A3CommonData.hpp"
#include "CameraManager.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <array>
#include <memory>

class LightsManager {
public:
	static constexpr uint32_t SphereShadowFaceCount = 6;

	struct alignas(16) SunLight {
        float cascadeSplits[4];
        glm::mat4 orthographic[4];
        glm::vec3 direction;
        float angle;
        glm::vec3 tint;
        int32_t shadow;
    };
    static_assert(sizeof(SunLight) == 304, "SunLight must match std430 layout.");

    struct alignas(16) SphereLight {
        glm::vec3 position;
        float radius;
        glm::vec3 tint;
        float limit;
		int32_t shadow;
		int32_t _pad_[3];
    };
	static_assert(sizeof(SphereLight) == 48, "SphereLight must match std430 layout.");

    struct alignas(16) SpotLight {
        glm::mat4 perspective;
        glm::vec3 position;
        float radius;
        glm::vec3 direction;
        float fov;
        glm::vec3 tint;
        float blend;
        float limit;
        int32_t shadow;
        int32_t _pad_[2];
    };
    static_assert(sizeof(SpotLight) == 128, "SpotLight must match std430 layout.");

    struct alignas(16) LightsHeader {
        uint32_t count;
        uint32_t _pad_[3];
    };
    static_assert(sizeof(LightsHeader) == 16, "LightsHeader must match std430 layout.");

	struct alignas(16) SphereShadowMatrices {
		std::array<glm::mat4, SphereShadowFaceCount> face_pv;
	};
	static_assert(sizeof(SphereShadowMatrices) == 384, "SphereShadowMatrices must match std430 layout.");

    inline VkDeviceSize sun_lights_buffer_size(uint32_t count) {
        return sizeof(LightsHeader) + sizeof(SunLight) * count;
    }

    inline VkDeviceSize sphere_lights_buffer_size(uint32_t count) {
        return sizeof(LightsHeader) + sizeof(SphereLight) * count;
    }

    inline VkDeviceSize spot_lights_buffer_size(uint32_t count) {
        return sizeof(LightsHeader) + sizeof(SpotLight) * count;
    }

	inline VkDeviceSize sphere_shadow_matrices_buffer_size(uint32_t count) {
		return sizeof(LightsHeader) + sizeof(SphereShadowMatrices) * count;
	}

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

	const std::vector<SunLight>& get_sun_lights() const { return sun_lights; }
	const std::vector<SphereLight>& get_sphere_lights() const { return sphere_lights; }
	const std::vector<SpotLight>& get_spot_lights() const { return spot_lights; }
	const std::vector<SunLight>& get_shadow_sun_lights() const { return shadow_sun_lights; }
	const std::vector<SphereLight>& get_shadow_sphere_lights() const { return shadow_sphere_lights; }
	const std::vector<SpotLight>& get_shadow_spot_lights() const { return shadow_spot_lights; }
	const std::vector<SphereShadowMatrices>& get_shadow_sphere_matrices() const { return shadow_sphere_matrices; }

	const std::vector<uint8_t>& get_sun_lights_bytes() const { return sun_lights_bytes; }
	const std::vector<uint8_t>& get_sphere_lights_bytes() const { return sphere_lights_bytes; }
	const std::vector<uint8_t>& get_spot_lights_bytes() const { return spot_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_sun_lights_bytes() const { return shadow_sun_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_sphere_lights_bytes() const { return shadow_sphere_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_spot_lights_bytes() const { return shadow_spot_lights_bytes; }
	const std::vector<uint8_t>& get_shadow_sphere_matrices_bytes() const { return shadow_sphere_matrices_bytes; }

	VkDeviceSize get_sun_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(sun_lights_bytes.size()); }
	VkDeviceSize get_sphere_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(sphere_lights_bytes.size()); }
	VkDeviceSize get_spot_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(spot_lights_bytes.size()); }
	VkDeviceSize get_shadow_sun_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_sun_lights_bytes.size()); }
	VkDeviceSize get_shadow_sphere_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_sphere_lights_bytes.size()); }
	VkDeviceSize get_shadow_spot_lights_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_spot_lights_bytes.size()); }
	VkDeviceSize get_shadow_sphere_matrices_buffer_capacity() const { return static_cast<VkDeviceSize>(shadow_sphere_matrices_bytes.size()); }

private:
	std::vector<SunLight> sun_lights;
	std::vector<SphereLight> sphere_lights;
	std::vector<SpotLight> spot_lights;
	std::vector<SunLight> shadow_sun_lights;
	std::vector<SphereLight> shadow_sphere_lights;
	std::vector<SpotLight> shadow_spot_lights;
	std::vector<SphereShadowMatrices> shadow_sphere_matrices;

	std::vector<uint8_t> sun_lights_bytes;
	std::vector<uint8_t> sphere_lights_bytes;
	std::vector<uint8_t> spot_lights_bytes;
	std::vector<uint8_t> shadow_sun_lights_bytes;
	std::vector<uint8_t> shadow_sphere_lights_bytes;
	std::vector<uint8_t> shadow_spot_lights_bytes;
	std::vector<uint8_t> shadow_sphere_matrices_bytes;
};
