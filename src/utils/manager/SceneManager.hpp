#pragma once

#include "S72Loader.hpp"
#include "VK.hpp"
#include "RTG.hpp"

#include <glm/glm.hpp>

class SceneManager {
public:
    Helpers::AllocatedBuffer vertex_buffer;
    Helpers::AllocatedBuffer cubemap_vertex_buffer;
    
    struct ObjectRange {
		uint32_t first = 0; // first vertex index
		uint32_t count = 0; // number of vertices
		glm::vec3 aabb_min; // axis-aligned bounding box min
		glm::vec3 aabb_max; // axis-aligned bounding box max
	};
    std::vector<ObjectRange> object_ranges;
    
    void create(RTG &rtg, std::shared_ptr<S72Loader::Document> doc);
    void destroy(RTG &rtg);

    SceneManager() = default;
    ~SceneManager() = default;
};