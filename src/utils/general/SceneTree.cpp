#include "SceneTree.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm> // for std::upper_bound

namespace SceneTree {

namespace {

// Compute local transform matrix from translation, rotation, scale
glm::mat4 compute_local_matrix(const glm::vec3 &translation, const glm::vec4 &rotation, const glm::vec3 &scale) {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    
    // rotation is stored as (x, y, z, w) quaternion in S72 usually, 
    // ensure constructor matches glm::quat(w, x, y, z)
    glm::quat q(rotation.w, rotation.x, rotation.y, rotation.z);
    glm::mat4 R = glm::mat4_cast(q);
    
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    
    return T * R * S;
}

// Compute world matrix for a node, using cache if available
glm::mat4 compute_world_matrix(std::shared_ptr<S72Loader::Document> doc, 
                                size_t node_index, 
                                const glm::mat4 &parent_matrix) {
    S72Loader::Node &node = doc->nodes[node_index];
    
    // Check cache if not dirty
    if (!node.model_matrix_is_dirty) {
        auto cache_it = world_matrix_cache.find(node_index);
        if (cache_it != world_matrix_cache.end()) {
            return cache_it->second;
        }
    }
    
    // Dirty or not in cache: Compute from scratch
    glm::mat4 local_matrix = compute_local_matrix(node.translation, node.rotation, node.scale);
    glm::mat4 world_matrix = parent_matrix * local_matrix;
    
    // Update Cache & Reset Flag
    world_matrix_cache[node_index] = world_matrix;
    node.model_matrix_is_dirty = false;
    
    return world_matrix;
}

// Recursive traversal helper
void traverse_node(std::shared_ptr<S72Loader::Document> doc,
                   size_t node_index,
                   const glm::mat4 &parent_matrix,
                   std::vector<MeshTreeData> &out_meshes,
                   std::vector<LightTreeData> &out_lights,
                   std::vector<CameraTreeData> &out_cameras,
                   std::vector<EnvironmentTreeData> &out_environments
                   ) {
    S72Loader::Node &node = doc->nodes[node_index];

    // Compute world matrix for this node (handles caching internally)
    glm::mat4 world_matrix = compute_world_matrix(doc, node_index, parent_matrix);
    
    // --- Collect Data ---
    if (node.mesh.has_value()) {
        const std::string &mesh_name = node.mesh.value();
        auto mesh_it = S72Loader::mesh_map.find(mesh_name);
        
        if (mesh_it != S72Loader::mesh_map.end()) {
            size_t mesh_index = mesh_it->second;
            const S72Loader::Mesh &mesh = doc->meshes[mesh_index];
            
            size_t material_index = 0;
            if (mesh.material.has_value()) {
                auto mat_it = S72Loader::material_map.find(mesh.material.value());
                if (mat_it != S72Loader::material_map.end()) {
                    material_index = mat_it->second;
                }
            }
            out_meshes.push_back({world_matrix, mesh_index, material_index});
        }
    } 
    else if(node.light.has_value()) {
        auto it = S72Loader::light_map.find(node.light.value());
        if (it != S72Loader::light_map.end()) {
            out_lights.push_back({world_matrix, it->second});
        }
    } 
    else if(node.camera.has_value()) {
        auto it = S72Loader::camera_map.find(node.camera.value());
        if (it != S72Loader::camera_map.end()) {
            out_cameras.push_back({world_matrix, it->second});
        }
    } 
    else if(node.environment.has_value()) {
        auto it = S72Loader::environment_map.find(node.environment.value());
        if (it != S72Loader::environment_map.end()) {
            out_environments.push_back({world_matrix, it->second});
        }
    }
    
    // Recursively traverse children
    for (const auto &child_name : node.children) {
        auto it = S72Loader::node_map.find(child_name);
        if (it != S72Loader::node_map.end()) {
            traverse_node(doc, it->second, world_matrix, out_meshes, out_lights, out_cameras, out_environments);
        }
    }
}

// recursive helper to mark a node and all its descendants as dirty
void mark_children_dirty_recursive(std::shared_ptr<S72Loader::Document> doc, size_t node_index) {
    S72Loader::Node &node = doc->nodes[node_index];

    node.model_matrix_is_dirty = true;

    world_matrix_cache.erase(node_index);
    
    for (const auto &child_name : node.children) {
        auto it = S72Loader::node_map.find(child_name);
        if (it != S72Loader::node_map.end()) {
            mark_children_dirty_recursive(doc, it->second);
        }
    }
}

} // anonymous namespace

void mark_dirty(std::shared_ptr<S72Loader::Document> doc, size_t node_index) {
    mark_children_dirty_recursive(doc, node_index);
}

void traverse_scene(std::shared_ptr<S72Loader::Document> doc, 
                    std::vector<MeshTreeData> &out_meshes,
                    std::vector<LightTreeData> &out_lights,
                    std::vector<CameraTreeData> &out_cameras,
                    std::vector<EnvironmentTreeData> &out_environments) {
    out_meshes.clear();
    out_lights.clear();
    out_cameras.clear();
    out_environments.clear();
    
    glm::mat4 identity(1.0f);
    
    for (const auto &root_name : doc->scene.roots) {
        auto it = S72Loader::node_map.find(root_name);
        if (it != S72Loader::node_map.end()) {
            traverse_node(doc, it->second, identity, out_meshes, out_lights, out_cameras, out_environments);
        }
    }
}

void update_animation(std::shared_ptr<S72Loader::Document> doc, float time) {
    for (const auto& driver : doc->drivers) {
        // Find the target node for this driver
        auto node_it = S72Loader::node_map.find(driver.node);
        if (node_it == S72Loader::node_map.end()) continue;
        
        size_t node_index = node_it->second;
        S72Loader::Node& target_node = doc->nodes[node_index];

        // Find the appropriate keyframe interval for the current time
        size_t pre_index;
        size_t tail_index;
        
        if(driver.times.empty()) continue;

        if(time < driver.times.front()){
            pre_index = tail_index = 0;
        } else if(time > driver.times.back()){
            pre_index = tail_index = driver.times.size() - 1;
        } else {
            auto upper = std::upper_bound(driver.times.begin(), driver.times.end(), time);
            tail_index = std::distance(driver.times.begin(), upper);
            pre_index = tail_index > 0 ? tail_index - 1 : 0;
        }
        
        float ratio = 0.0f;
        if (pre_index != tail_index) {
            float duration = driver.times[tail_index] - driver.times[pre_index];
            if (duration > std::numeric_limits<float>::epsilon())
                ratio = (time - driver.times[pre_index]) / duration;
        }

        bool changed = false;

        // calculate interpolation and directly update Node data
        if (driver.channel == "rotation") {
            glm::quat q1(driver.values[4*pre_index+3], driver.values[4*pre_index+0], driver.values[4*pre_index+1], driver.values[4*pre_index+2]);
            glm::quat q_result = q1;
            
            if (pre_index != tail_index) {
                glm::quat q2(driver.values[4*tail_index+3], driver.values[4*tail_index+0], driver.values[4*tail_index+1], driver.values[4*tail_index+2]);
                q_result = glm::slerp(q1, q2, ratio);
            }
            
            glm::vec4 new_rot(q_result.x, q_result.y, q_result.z, q_result.w);
            if (target_node.rotation != new_rot) {
                target_node.rotation = new_rot;
                changed = true;
            }
        } else {
            glm::vec3 v1(driver.values[3*pre_index+0], driver.values[3*pre_index+1], driver.values[3*pre_index+2]);
            glm::vec3 v_result = v1;

            if (pre_index != tail_index && driver.interpolation == "LINEAR") {
                glm::vec3 v2(driver.values[3*tail_index+0], driver.values[3*tail_index+1], driver.values[3*tail_index+2]);
                v_result = glm::mix(v1, v2, ratio);
            }

            if (driver.channel == "translation") {
                if (target_node.translation != v_result) {
                    target_node.translation = v_result;
                    changed = true;
                }
            } else if (driver.channel == "scale") {
                if (target_node.scale != v_result) {
                    target_node.scale = v_result;
                    changed = true;
                }
            }
        }

        // if changed, mark this node and all its descendants as dirty to ensure world matrix will be updated
        if (changed) {
            mark_children_dirty_recursive(doc, node_index);
        }
    }
}

} // namespace SceneTree