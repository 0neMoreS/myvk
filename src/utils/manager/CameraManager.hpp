#pragma once

#include "S72Loader.hpp"
#include "InputEvent.hpp"
#include "SceneTree.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>
#include <vector>
#include <cstddef>
#include <array>

class CameraManager {
public:
	// Frustum plane structure
	struct FrustumPlane {
		glm::vec3 normal;
		float distance;
	};
	
	// Frustum structure with 6 planes
	struct Frustum {
		std::array<FrustumPlane, 6> planes; // left, right, bottom, top, near, far
		
		bool is_box_visible(const glm::vec3& min, const glm::vec3& max) const;
	};

	// Camera data structure
	struct Camera {
		glm::vec3 camera_position;
        glm::vec3 camera_forward;
		glm::vec3 camera_up;
		glm::vec3 world_up;
		float camera_fov;
        float camera_near;
        float camera_far;
		float aspect;
	};

	CameraManager() = default;
	~CameraManager() = default;

	// create cameras from S72 document
	void create(const std::shared_ptr<S72Loader::Document> doc, 
				const uint32_t swapchain_width, const uint32_t swapchain_height, 
				const std::vector<SceneTree::CameraTreeData>& camera_tree_data, 
				std::string init_camera_name
			);

	// Update camera state (called every frame)
	void update(float dt, const std::vector<SceneTree::CameraTreeData>& camera_tree_data, bool open_debug_camera);

	// Handle input events
	void on_input(const InputEvent& event);

	float get_aspect_ratio(bool open_debug_camera, VkExtent2D swapchain_extent);

	// Get current camera matrices
	glm::mat4 get_perspective() const;
	glm::mat4 get_view() const;
	glm::mat4 get_debug_perspective() const;
	glm::mat4 get_debug_view() const;
	
	// Get current frustum for culling
	Frustum get_frustum() const;

	// Camera access
	size_t get_active_camera_index() const { return active_camera_index; }
	void set_active_camera_index(size_t index) { if (index < cameras.size()) active_camera_index = index; }
	
	Camera& get_active_camera() { return cameras[active_camera_index]; }
	const Camera& get_active_camera() const { return cameras[active_camera_index]; }

	const std::vector<Camera>& get_all_cameras() const { return cameras; }
	size_t get_camera_count() const { return cameras.size(); }

	Camera& get_debug_camera() { return debug_camera; }
	const Camera& get_debug_camera() const { return debug_camera; }

	void change_active_camera() { active_camera_index = (active_camera_index + 1) % cameras.size();	}

private:
	// Cameras loaded from scene
	std::vector<Camera> cameras; // 0 is for user camera
	size_t active_camera_index = 0;
	Camera debug_camera;

	void update_scene_camera(size_t index, const SceneTree::CameraTreeData &ctd);
	void update_user_camera(float dt, Camera &active_camera);

	// Input state
	bool keys_down[GLFW_KEY_LAST + 1] = {};
	float last_mouse_x = 0.0f;
	float last_mouse_y = 0.0f;
	bool has_last_mouse_pos = false;

	// Right-mouse drag to control camera orientation:
	bool mouse_look_enabled = true;
	bool mouse_look_held = false;
	float pending_mouse_dx = 0.0f;
	float pending_mouse_dy = 0.0f;
	float mouse_sensitivity = 0.0025f; // radians per pixel

	// Camera parameters
	const float move_speed = 10.0f;
	const float fov_speed = 1.0f;
	const float rotate_speed = 1.0f;
};
