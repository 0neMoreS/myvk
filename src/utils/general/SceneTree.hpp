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
    glm::vec3 translation;
    glm::vec4 rotation;
    glm::vec3 scale;
};

inline std::unordered_map<std::string, NodeTRS> node_trs_cache; // no need for this? TRS are stored in nodes already

void traverse_scene(std::shared_ptr<S72Loader::Document> doc, 
					std::vector<MeshTreeData> &out_meshes,
					std::vector<LightTreeData> &out_lights,
					std::vector<CameraTreeData> &out_cameras,
					std::vector<EnvironmentTreeData> &out_environments);

// Update AABBs bottom-up using mesh AABBs from object_ranges
// object_ranges is indexed by mesh_index to get the local AABB of each mesh
void update_aabbs(std::shared_ptr<S72Loader::Document> doc);

void update_animation(std::shared_ptr<S72Loader::Document> doc, float time);

// Clear the world matrix cache (call when scene is reloaded)
void clear_cache();
}