#include "SceneTree.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <stack>

namespace SceneTree {

namespace {

// Compute local transform matrix from translation, rotation, scale
glm::mat4 compute_local_matrix(const glm::vec3 &translation, const glm::vec4 &rotation, const glm::vec3 &scale) {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    
    // rotation is stored as (x, y, z, w) quaternion
    glm::quat q(rotation.w, rotation.x, rotation.y, rotation.z);
    glm::mat4 R = glm::mat4_cast(q);
    
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    
    return T * R * S;
}

// Cache for computed world matrices
inline std::unordered_map<size_t, glm::mat4> world_matrix_cache;

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
    
    // Compute local matrix
    glm::mat4 local_matrix = compute_local_matrix(node.translation, node.rotation, node.scale);
    glm::mat4 world_matrix = parent_matrix * local_matrix;
    
    // Cache the result and clear dirty flag
    world_matrix_cache[node_index] = world_matrix;
    node.model_matrix_is_dirty = false;
    
    return world_matrix;
}

// Recursively mark children as dirty (downward propagation)
void mark_children_dirty(std::shared_ptr<S72Loader::Document> doc, size_t node_index) {
    S72Loader::Node &node = doc->nodes[node_index];
    node.model_matrix_is_dirty = true;
    
    for (const auto &child_name : node.children) {
        auto it = S72Loader::node_map.find(child_name);
        if (it != S72Loader::node_map.end()) {
            mark_children_dirty(doc, it->second);
        }
    }
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

    node_trs_cache[node.name] = NodeTRS{node.translation, node.rotation, node.scale};
    
    // Compute world matrix for this node
    glm::mat4 world_matrix = compute_world_matrix(doc, node_index, parent_matrix);
    
    // If this node has a mesh, add it to results
    if (node.mesh.has_value()) {
        const std::string &mesh_name = node.mesh.value();
        auto mesh_it = S72Loader::mesh_map.find(mesh_name);
        
        if (mesh_it != S72Loader::mesh_map.end()) {
            size_t mesh_index = mesh_it->second;
            const S72Loader::Mesh &mesh = doc->meshes[mesh_index];
            
            // Get material index (default to 0 if not found)
            size_t material_index = 0;
            if (mesh.material.has_value()) {
                auto mat_it = S72Loader::material_map.find(mesh.material.value());
                if (mat_it != S72Loader::material_map.end()) {
                    material_index = mat_it->second;
                }
            }
            
            MeshTreeData mesh_data;
            mesh_data.model_matrix = world_matrix;
            mesh_data.mesh_index = mesh_index;
            mesh_data.material_index = material_index;
            
            out_meshes.push_back(mesh_data);
        }
    } else if(node.light.has_value()) {
        const std::string &light_name = node.light.value();
        auto light_it = S72Loader::light_map.find(light_name);
        
        if (light_it != S72Loader::light_map.end()) {
            size_t light_index = light_it->second;
            
            LightTreeData light_data;
            light_data.model_matrix = world_matrix;
            light_data.light_index = light_index;
            
            out_lights.push_back(light_data);
        }
    } else if(node.camera.has_value()) {
        const std::string &camera_name = node.camera.value();
        auto camera_it = S72Loader::camera_map.find(camera_name);
        
        if (camera_it != S72Loader::camera_map.end()) {
            size_t camera_index = camera_it->second;
            
            CameraTreeData camera_data;
            camera_data.model_matrix = world_matrix;
            camera_data.camera_index = camera_index;
            
            out_cameras.push_back(camera_data);
        }
    } else if(node.environment.has_value()) {
        const std::string &env_name = node.environment.value();
        auto env_it = S72Loader::environment_map.find(env_name);
        
        if (env_it != S72Loader::environment_map.end()) {
            size_t environment_index = env_it->second;
            
            EnvironmentTreeData env_data;
            env_data.model_matrix = world_matrix;
            env_data.environment_index = environment_index;
            
            out_environments.push_back(env_data);
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

void transform_node(std::shared_ptr<S72Loader::Document> doc, 
                    const std::string &node_name, 
                    const glm::vec3 &T,
                    const glm::vec4 &R,
                    const glm::vec3 &S
                ) {
    // Find node by name using global node_map
    auto it = S72Loader::node_map.find(node_name);
    if (it == S72Loader::node_map.end()) {
        return; // Node not found
    }
    
    size_t node_index = it->second;
    S72Loader::Node &node = doc->nodes[node_index];

    node.translation = T;
    node.scale = S;
    node.rotation = R;
    
    // Mark this node and all children as dirty (downward propagation)
    mark_children_dirty(doc, node_index);
    
    // Invalidate cache for this node
    world_matrix_cache.erase(node_index);
}

} // anonymous namespace

void traverse_scene(std::shared_ptr<S72Loader::Document> doc, 
					std::vector<MeshTreeData> &out_meshes,
					std::vector<LightTreeData> &out_lights,
					std::vector<CameraTreeData> &out_cameras,
					std::vector<EnvironmentTreeData> &out_environments) {
    out_meshes.clear();
    out_lights.clear();
    out_cameras.clear();
    out_environments.clear();
    // Start traversal from scene roots
    glm::mat4 identity(1.0f);
    
    for (const auto &root_name : doc->scene.roots) {
        auto it = S72Loader::node_map.find(root_name);
        if (it != S72Loader::node_map.end()) {
            traverse_node(doc, it->second, identity, out_meshes, out_lights, out_cameras, out_environments);
        }
    }
}

void update_animation(std::shared_ptr<S72Loader::Document> doc, float time) {
    for (size_t i = 0; i < doc->drivers.size(); ++i) {
        const auto& driver = doc->drivers[i];

        NodeTRS& current_trs = node_trs_cache[driver.node];

        size_t pre_index; // previous keyframe index
        size_t tail_index; // current keyframe index
        if(time < driver.times.front()){
            pre_index = tail_index = 0;
        } else if(time > driver.times.back()){
            pre_index = tail_index = driver.times.size() - 1;
        } else {
            auto upper = std::upper_bound(driver.times.begin(), driver.times.end(), time);
            tail_index = upper - driver.times.begin();
            pre_index = tail_index - 1;
        }
        
        float ratio = 0.0f;
        if (pre_index != tail_index) {
            ratio = (time - driver.times[pre_index]) / (driver.times[tail_index] - driver.times[pre_index]);
        }

        if (driver.channel == "rotation") {
            glm::quat q1(driver.values[4*pre_index+3], driver.values[4*pre_index+0], driver.values[4*pre_index+1], driver.values[4*pre_index+2]);
            glm::quat q_result = q1;
            
            if (pre_index != tail_index) {
                glm::quat q2(driver.values[4*tail_index+3], driver.values[4*tail_index+0], driver.values[4*tail_index+1], driver.values[4*tail_index+2]);
                q_result = glm::slerp(q1, q2, ratio);
            }
            current_trs.rotation = glm::vec4(q_result.x, q_result.y, q_result.z, q_result.w);
        } else {
            glm::vec3 v1(driver.values[3*pre_index+0], driver.values[3*pre_index+1], driver.values[3*pre_index+2]);
            glm::vec3 v_result = v1;

            if (pre_index != tail_index && driver.interpolation == "LINEAR") {
                glm::vec3 v2(driver.values[3*tail_index+0], driver.values[3*tail_index+1], driver.values[3*tail_index+2]);
                v_result = glm::mix(v1, v2, ratio);
            }

            if (driver.channel == "translation") {
                current_trs.translation = v_result;
            } else if (driver.channel == "scale") {
                current_trs.scale = v_result;
            }
        }
    }

    for (const auto& pair : node_trs_cache) {
        const auto& node_name = pair.first;
        const auto& trs = pair.second;

        SceneTree::transform_node(
            doc, 
            node_name, 
            trs.translation, 
            trs.rotation,
            trs.scale
        );
    }
}

// Clear the world matrix cache (call when scene is reloaded)
void clear_cache() {
    world_matrix_cache.clear();
}

} // namespace SceneTree
