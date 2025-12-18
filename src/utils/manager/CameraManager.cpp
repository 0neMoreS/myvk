#include "CameraManager.hpp"

#include "RTG.hpp"

#include <cmath>

void CameraManager::create(const std::shared_ptr<S72Loader::Document> doc, const uint32_t swapchain_width, const uint32_t swapchain_height) {
    cameras.clear();
	for(auto &camera : doc->cameras) {
        for(auto &transform : camera.transforms) {
            glm::mat3 blender_rotation = glm::mat3(transform);
            glm::vec3 blender_forward = blender_rotation * glm::vec3{0.0f, 0.0f, -1.0f};

            cameras.emplace_back(CameraManager::Camera {
                .camera_position = BLENDER_TO_VULKAN_3 * glm::vec3{transform[3][0], transform[3][1], transform[3][2]},
                .camera_forward = BLENDER_TO_VULKAN_3 * blender_forward,
                .camera_up = BLENDER_TO_VULKAN_3 * blender_rotation * glm::vec3{0.0f, 1.0f, 0.0f},
                .camera_fov = camera.perspective.has_value() ? camera.perspective.value().vfov : 90.0f,
                .camera_height = swapchain_height,
                .camera_width = swapchain_width,
                .camera_near = camera.perspective.has_value() ? camera.perspective.value().near : 0.1f,
                .camera_far = camera.perspective.has_value() ? (camera.perspective.value().far.has_value() ? camera.perspective.value().far.value() : 1000.0f) : 1000.0f,
            });
        }
    }
}

void CameraManager::update(float dt, const uint32_t swapchain_width, const uint32_t swapchain_height) {
	if (cameras.empty()) return;

	Camera& active_camera = cameras[active_camera_index];
	float theta = std::acos(-active_camera.camera_forward.y);
    float phi = std::atan2(active_camera.camera_forward.z, active_camera.camera_forward.x);

	// Keyboard rotation
	if (keys_down[GLFW_KEY_J]) {
		phi += rotate_speed * dt;
	}
	if (keys_down[GLFW_KEY_L]) {
		phi -= rotate_speed * dt;
	}
	if (keys_down[GLFW_KEY_I]) {
		theta -= rotate_speed * dt;
	}
	if (keys_down[GLFW_KEY_K]) {
		theta += rotate_speed * dt;
	}

	// Calculate forward direction
	active_camera.camera_forward = glm::vec3(
		std::sin(theta) * std::cos(phi),
		-std::cos(theta),
		std::sin(theta) * std::sin(phi)
	);

	glm::vec3 right = glm::normalize(glm::cross(active_camera.camera_forward, active_camera.camera_up));

	// Keyboard movement
	if (keys_down[GLFW_KEY_W]) {
		active_camera.camera_position += active_camera.camera_forward * move_speed * dt;
	}
	if (keys_down[GLFW_KEY_S]) {
		active_camera.camera_position -= active_camera.camera_forward * move_speed * dt;
	}
	if (keys_down[GLFW_KEY_A]) {
		active_camera.camera_position -= right * move_speed * dt;
	}
	if (keys_down[GLFW_KEY_D]) {
		active_camera.camera_position += right * move_speed * dt;
	}
	if (keys_down[GLFW_KEY_Q]) {
		active_camera.camera_position += active_camera.camera_up * move_speed * dt;
	}
	if (keys_down[GLFW_KEY_E]) {
		active_camera.camera_position -= active_camera.camera_up * move_speed * dt;
	}

	// Keyboard FOV adjustment
	if (keys_down[GLFW_KEY_R]) {
		active_camera.camera_fov += fov_speed * dt;
	}
	if (keys_down[GLFW_KEY_F]) {
		active_camera.camera_fov -= fov_speed * dt;
	}

	// Clamp FOV
	active_camera.camera_fov = glm::clamp(active_camera.camera_fov, 0.0f, glm::radians(120.0f));

	// Update camera dimensions
	// active_camera.camera_width = swapchain_width;
	// active_camera.camera_height = swapchain_height;
}

void CameraManager::resize_all_cameras(const uint32_t swapchain_width, const uint32_t swapchain_height) {
	std::cout << "CameraManager: Resizing all cameras to " << swapchain_width << "x" << swapchain_height << std::endl;
	for(auto &camera : cameras) {
		camera.camera_width = swapchain_width;
		camera.camera_height = swapchain_height;
	}
}

glm::mat4 CameraManager::get_perspective() const{
    const Camera& active_camera = cameras[active_camera_index];
    glm::mat4 perspective = glm::perspectiveRH_ZO(active_camera.camera_fov, (float)active_camera.camera_width / (float)active_camera.camera_height, active_camera.camera_near, active_camera.camera_far);
    perspective[1][1] *= -1.0f;
	return perspective;
}

glm::mat4 CameraManager::get_view() const{
    const Camera& active_camera = cameras[active_camera_index];
    return glm::lookAtRH(active_camera.camera_position, active_camera.camera_position +  active_camera.camera_forward, active_camera.camera_up);
}

void CameraManager::on_input(const InputEvent& event) {
	if (event.type == InputEvent::KeyDown) {
		if (event.key.key >= 0 && event.key.key <= GLFW_KEY_LAST) {
			keys_down[event.key.key] = true;
		}

		// Change active camera with TAB
		if (event.key.key == GLFW_KEY_TAB) {
			active_camera_index = (active_camera_index + 1) % cameras.size();
		}
	} else if (event.type == InputEvent::KeyUp) {
		if (event.key.key >= 0 && event.key.key <= GLFW_KEY_LAST) {
			keys_down[event.key.key] = false;
		}
	} else if (event.type == InputEvent::MouseMotion) {
		// Mouse motion handling can be added here if needed
		// float dx = event.motion.x - last_mouse_x;
		// float dy = event.motion.y - last_mouse_y;
		// const float sensitivity = 0.0005f;
		// camera_phi += dx * sensitivity;
		// camera_theta += dy * sensitivity;
		// last_mouse_x = event.motion.x;
		// last_mouse_y = event.motion.y;
	}
}
