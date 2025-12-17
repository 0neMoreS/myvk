#pragma once

#include "S72Loader.hpp"
#include "VK.hpp"
#include "RTG.hpp"

class SceneManager {
public:
    Helpers::AllocatedBuffer vertex_buffer;
    
    struct ObjectRange {
		uint32_t first = 0; // first vertex index
		uint32_t count = 0; // number of vertices
	};
    std::vector<ObjectRange> object_ranges;
    
    void create(RTG &rtg, std::shared_ptr<S72Loader::Document> doc);
    void destroy(RTG &rtg);

    SceneManager() = default;
    ~SceneManager() = default;
};