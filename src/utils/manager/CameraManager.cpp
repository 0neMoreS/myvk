#include "CameraManager.hpp"

#include "RTG.hpp"

#include <cmath>
#include <glm/gtc/constants.hpp>

void CameraManager::create(const std::shared_ptr<S72Loader::Document> doc, 
				const uint32_t swapchain_width, const uint32_t swapchain_height, 
				const std::vector<SceneTree::CameraTreeData>& camera_tree_data, 
				std::string init_camera_name
			) {
    // Create a user camera at index 0
	cameras.emplace_back(CameraManager::Camera {
		.camera_position = glm::vec3{0.0f, 0.0f, -5.0f},
		.camera_forward = glm::vec3{0.0f, 0.0f, 1.0f},
		.camera_up = glm::vec3{0.0f, -1.0f, 0.0f},
		.world_up = glm::vec3{0.0f, -1.0f, 0.0f},
		.camera_fov = glm::radians(60.0f),
		.camera_height = swapchain_height,
		.camera_width = swapchain_width,
		.camera_near = 0.1f,
		.camera_far = 1000.0f,
	});


	this->debug_camera = cameras[0];

	// Create scene cameras
	
	for(auto &ctd : camera_tree_data) {
        glm::mat4 transform = ctd.model_matrix;
		auto &camera = doc->cameras[ctd.camera_index];

		glm::mat3 blender_rotation = glm::mat3(transform);
		glm::vec3 blender_forward = blender_rotation * glm::vec3{0.0f, 0.0f, -1.0f};
		cameras.emplace_back(CameraManager::Camera {
			.camera_position = BLENDER_TO_VULKAN_3 * glm::vec3{transform[3][0], transform[3][1], transform[3][2]},
			.camera_forward = BLENDER_TO_VULKAN_3 * blender_forward,
			.camera_up = BLENDER_TO_VULKAN_3 * blender_rotation * glm::vec3{0.0f, 1.0f, 0.0f},
			.world_up = glm::vec3{0.0f, -1.0f, 0.0f},
			.camera_fov = camera.perspective.has_value() ? camera.perspective.value().vfov : 90.0f,
			.camera_height = swapchain_height,
			.camera_width = swapchain_width,
			.camera_near = camera.perspective.has_value() ? camera.perspective.value().near : 0.1f,
			.camera_far = camera.perspective.has_value() ? (camera.perspective.value().far.has_value() ? camera.perspective.value().far.value() : 1000.0f) : 1000.0f,
		});

		if(camera.name == init_camera_name){
			active_camera_index = cameras.size() - 1;
		}
    }
}

void CameraManager::update(float dt, const std::vector<SceneTree::CameraTreeData>& camera_tree_data, bool open_debug_camera) {
	if(open_debug_camera) {
		update_user_camera(dt, debug_camera);
	} else if(active_camera_index == 0) {
		update_user_camera(dt, cameras[0]);
	}
	else {
		for(size_t i = 1; i < cameras.size(); ++i){
			update_scene_camera(i, camera_tree_data[i - 1]);
		}
	}
}

void CameraManager::update_scene_camera(size_t index, const SceneTree::CameraTreeData &ctd) {
	Camera& camera = cameras[index];
	
	glm::mat4 transform = ctd.model_matrix;
	glm::mat3 blender_rotation = glm::mat3(transform);
	glm::vec3 blender_forward = blender_rotation * glm::vec3{0.0f, 0.0f, -1.0f};
	
	camera.camera_position = BLENDER_TO_VULKAN_3 * glm::vec3{transform[3][0], transform[3][1], transform[3][2]};
	camera.camera_forward = BLENDER_TO_VULKAN_3 * blender_forward;
	camera.camera_up = BLENDER_TO_VULKAN_3 * blender_rotation * glm::vec3{0.0f, 1.0f, 0.0f};
}

void CameraManager::update_user_camera(float dt, Camera &active_camera) {
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

	// Mouse rotation (accumulated deltas from on_input)
	if (mouse_look_enabled && mouse_look_held) {
		// +dx to the right should yaw to the right; if this feels inverted, flip the sign.
		phi -= pending_mouse_dx * mouse_sensitivity;
		// +dy downward should pitch downward; if you prefer inverted Y, flip the sign.
		theta += pending_mouse_dy * mouse_sensitivity;
		pending_mouse_dx = 0.0f;
		pending_mouse_dy = 0.0f;
	}

	// Clamp theta away from the poles to avoid flipping / NaNs
	const float eps = 1e-3f;
	theta = glm::clamp(theta, eps, glm::pi<float>() - eps);

	// Calculate forward direction
	active_camera.camera_forward = glm::normalize(glm::vec3(
		std::sin(theta) * std::cos(phi),
		-std::cos(theta),
		std::sin(theta) * std::sin(phi)
	));
	glm::vec3 right = glm::normalize(glm::cross(active_camera.camera_forward, active_camera.world_up));
	active_camera.camera_up = glm::normalize(glm::cross(right, active_camera.camera_forward));

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
}

void CameraManager::resize_all_cameras(const uint32_t swapchain_width, const uint32_t swapchain_height) {
	for(auto &camera : cameras) {
		camera.camera_width = swapchain_width;
		camera.camera_height = swapchain_height;
	}

	debug_camera.camera_width = swapchain_width;
	debug_camera.camera_height = swapchain_height;
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

glm::mat4 CameraManager::get_debug_perspective() const{
	glm::mat4 perspective = glm::perspectiveRH_ZO(debug_camera.camera_fov, (float)debug_camera.camera_width / (float)debug_camera.camera_height, debug_camera.camera_near, debug_camera.camera_far);
	perspective[1][1] *= -1.0f;
	return perspective;
}

glm::mat4 CameraManager::get_debug_view() const{
	return glm::lookAtRH(debug_camera.camera_position, debug_camera.camera_position +  debug_camera.camera_forward, debug_camera.camera_up);
}

CameraManager::Frustum CameraManager::get_frustum() const {
	Frustum frustum;
	glm::mat4 vp = get_perspective() * get_view();
	
	// Extract frustum planes from view-projection matrix
	// Left plane
	frustum.planes[0].normal = glm::vec3(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0], vp[2][3] + vp[2][0]);
	frustum.planes[0].distance = vp[3][3] + vp[3][0];
	
	// Right plane
	frustum.planes[1].normal = glm::vec3(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0], vp[2][3] - vp[2][0]);
	frustum.planes[1].distance = vp[3][3] - vp[3][0];
	
	// Bottom plane
	frustum.planes[2].normal = glm::vec3(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1], vp[2][3] + vp[2][1]);
	frustum.planes[2].distance = vp[3][3] + vp[3][1];
	
	// Top plane
	frustum.planes[3].normal = glm::vec3(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1], vp[2][3] - vp[2][1]);
	frustum.planes[3].distance = vp[3][3] - vp[3][1];
	
	// Near plane
	frustum.planes[4].normal = glm::vec3(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2], vp[2][3] + vp[2][2]);
	frustum.planes[4].distance = vp[3][3] + vp[3][2];
	
	// Far plane
	frustum.planes[5].normal = glm::vec3(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2], vp[2][3] - vp[2][2]);
	frustum.planes[5].distance = vp[3][3] - vp[3][2];
	
	// Normalize planes
	for (auto& plane : frustum.planes) {
		float len = glm::length(plane.normal);
		plane.normal /= len;
		plane.distance /= len;
	}
	
	return frustum;
}

bool CameraManager::Frustum::is_box_visible(const glm::vec3& min, const glm::vec3& max) const {
	// Check if AABB is outside any plane
	for (const auto& plane : planes) {
		// Find the positive vertex (furthest along plane normal)
		glm::vec3 positive_vertex;
		positive_vertex.x = (plane.normal.x >= 0) ? max.x : min.x;
		positive_vertex.y = (plane.normal.y >= 0) ? max.y : min.y;
		positive_vertex.z = (plane.normal.z >= 0) ? max.z : min.z;
		
		// If positive vertex is outside, the box is outside
		if (glm::dot(plane.normal, positive_vertex) + plane.distance < 0) {
			return false;
		}
	}
	return true;
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
	} else if (event.type == InputEvent::MouseButtonDown) {
		if (mouse_look_enabled && event.button.button == GLFW_MOUSE_BUTTON_LEFT) {
			mouse_look_held = true;
			has_last_mouse_pos = false; // reset delta on first motion after press
			pending_mouse_dx = 0.0f;
			pending_mouse_dy = 0.0f;
		}
	} else if (event.type == InputEvent::MouseButtonUp) {
		if (event.button.button == GLFW_MOUSE_BUTTON_LEFT) {
			mouse_look_held = false;
			has_last_mouse_pos = false;
			pending_mouse_dx = 0.0f;
			pending_mouse_dy = 0.0f;
		}
	} else if (event.type == InputEvent::MouseMotion) {
		if (!mouse_look_enabled || !mouse_look_held) return;

		const float x = event.motion.x;
		const float y = event.motion.y;

		if (!has_last_mouse_pos) {
			has_last_mouse_pos = true;
			last_mouse_x = x;
			last_mouse_y = y;
			return;
		}

		pending_mouse_dx += (x - last_mouse_x);
		pending_mouse_dy += (y - last_mouse_y);
		last_mouse_x = x;
		last_mouse_y = y;
	}
}
