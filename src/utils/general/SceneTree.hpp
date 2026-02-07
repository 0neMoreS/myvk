#pragma once

#include "VK.hpp"
#include "S72Loader.hpp"
#include "SceneManager.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

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

inline std::unordered_map<size_t, glm::mat4> world_matrix_cache;

void traverse_scene(std::shared_ptr<S72Loader::Document> doc, 
                    std::vector<MeshTreeData> &out_meshes,
                    std::vector<LightTreeData> &out_lights,
                    std::vector<CameraTreeData> &out_cameras,
                    std::vector<EnvironmentTreeData> &out_environments);

void update_animation(std::shared_ptr<S72Loader::Document> doc, float time);

// helper function to mark a node and all its descendants as dirty
void mark_dirty(std::shared_ptr<S72Loader::Document> doc, size_t node_index);

}