#pragma once

#include "VK.hpp"
#include "S72Loader.hpp"
#include "SceneManager.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace SceneTree {

struct MeshTreeData{
	glm::mat4 model_matrix;
	size_t mesh_index;
	size_t material_index;
};

struct LightTreeData{
	glm::mat4 model_matrix;
	size_t light_index;
};

struct CameraTreeData{
	glm::mat4 model_matrix;
	size_t camera_index;
};

struct EnvironmentTreeData
{
	glm::mat4 model_matrix;
	size_t environment_index;
};

struct NodeTRS {
    glm::vec3 translation = glm::vec3(0.0f);
    glm::vec4 rotation = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

inline std::unordered_map<std::string, NodeTRS> node_trs_cache;

void traverse_scene(std::shared_ptr<S72Loader::Document> doc, 
					std::vector<MeshTreeData> &out_meshes,
					std::vector<LightTreeData> &out_lights,
					std::vector<CameraTreeData> &out_cameras,
					std::vector<EnvironmentTreeData> &out_environments);

void transform_node(std::shared_ptr<S72Loader::Document> doc, const std::string &node_name, const glm::vec3 &T, const glm::vec4 &R, const glm::vec3 &S);

// Update AABBs bottom-up using mesh AABBs from object_ranges
// object_ranges is indexed by mesh_index to get the local AABB of each mesh
void update_aabbs(std::shared_ptr<S72Loader::Document> doc, const std::vector<SceneManager::ObjectRange> &object_ranges);

void update_animation(std::shared_ptr<S72Loader::Document> doc, float time);

// Clear the world matrix cache (call when scene is reloaded)
void clear_cache();
}