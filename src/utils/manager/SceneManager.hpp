#pragma once

#include "S72Loader.hpp"
#include "VK.hpp"
#include "RTG.hpp"

#include <glm/glm.hpp>

class SceneManager {
public:
    Helpers::AllocatedBuffer vertex_buffer;
    Helpers::AllocatedBuffer cubemap_vertex_buffer;
    
    void create(RTG &rtg, std::shared_ptr<S72Loader::Document> doc);
    void destroy(RTG &rtg);

    SceneManager() = default;
    ~SceneManager() = default;
};