#pragma once

#include "S72Loader.hpp"
#include "InputEvent.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>
#include <vector>
#include <cstddef>

class CameraManager {
public:
	// Camera data structure
	struct Camera {
		glm::vec3 camera_position;
        glm::vec3 camera_forward;
		glm::vec3 camera_up;
		float camera_fov;
        uint32_t camera_height;
        uint32_t camera_width;
        float camera_near;
        float camera_far;
	};

	CameraManager() = default;
	~CameraManager() = default;

	// Initialize cameras from S72 document
	void initialize(const std::shared_ptr<S72Loader::Document> doc, const uint32_t swapchain_width, const uint32_t swapchain_height);

	// Update camera state (called every frame)
	void update(float dt, uint32_t swapchain_width, uint32_t swapchain_height);

	// Handle input events
	void on_input(const InputEvent& event);

	// Get current camera matrices
	glm::mat4 get_perspective() const;
	glm::mat4 get_view() const;

	// Camera access
	size_t get_active_camera_index() const { return active_camera_index; }
	void set_active_camera_index(size_t index) { if (index < cameras.size()) active_camera_index = index; }
	
	Camera& get_active_camera() { return cameras[active_camera_index]; }
	const Camera& get_active_camera() const { return cameras[active_camera_index]; }

	const std::vector<Camera>& get_all_cameras() const { return cameras; }
	size_t get_camera_count() const { return cameras.size(); }

private:
	// Cameras loaded from scene
	std::vector<Camera> cameras;
	size_t active_camera_index = 0;

	// Input state
	bool keys_down[GLFW_KEY_LAST + 1] = {};
	float last_mouse_x = 0.0f;
	float last_mouse_y = 0.0f;

	// Camera parameters
	const float move_speed = 4.0f;
	const float fov_speed = 1.0f;
	const float rotate_speed = 1.0f;
};
