#include "SceneManager.hpp"

void SceneManager::create(RTG &rtg, std::shared_ptr<S72Loader::Document> doc) {
    { //create object vertices
		std::vector< uint8_t > all_vertices;

		// Load vertices from all meshes in the document
		uint32_t vertex_offset = 0;
		for (const auto &mesh : doc->meshes) {
			try {
				std::vector<uint8_t> mesh_data = S72Loader::load_mesh_data(s72_dir, mesh);
				
				// Calculate AABB
				glm::vec3 aabb_min(FLT_MAX);
				glm::vec3 aabb_max(-FLT_MAX);
				
				// Assuming vertex format has position at start (3 floats: x, y, z)
				size_t vertex_stride = mesh_data.size() / mesh.count; // bytes per vertex
				
				for (uint32_t v = 0; v < mesh.count; ++v) {
					const float* pos = reinterpret_cast<const float*>(mesh_data.data() + v * vertex_stride);
					glm::vec3 p(pos[0], pos[1], pos[2]);
					aabb_min = glm::min(aabb_min, p);
					aabb_max = glm::max(aabb_max, p);
				}
				
				ObjectRange object_range;
				object_range.first = vertex_offset;
				object_range.count = mesh.count;
				object_range.aabb_min = aabb_min;
				object_range.aabb_max = aabb_max;
				object_ranges.push_back(object_range);

				all_vertices.insert(all_vertices.end(), mesh_data.begin(), mesh_data.end());
				vertex_offset += mesh.count;
			} catch (const std::exception &e) {
				std::cerr << "Warning: Failed to load mesh '" << mesh.name << "': " << e.what() << std::endl;
			}
		}

		size_t bytes = all_vertices.size();

		if (bytes > 0) {
			vertex_buffer = rtg.helpers.create_buffer(
				bytes,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped
			);

			//copy data to buffer:
			rtg.helpers.transfer_to_buffer(all_vertices.data(), bytes, vertex_buffer);
		}
	}

	std::vector<uint8_t> cubemap_vertex_data;
	{ // load cubemap vertices
		if(doc->environments.size() > 0) {
			try {
				cubemap_vertex_data = S72Loader::load_mesh_data(s72_dir, "env-cube.b72");
			} catch (const std::exception &e) {
				std::cerr << "Warning: Failed to load mesh 'env_cube.b72': " << e.what() << std::endl;
			}
		}
	}

	size_t cubemap_bytes = cubemap_vertex_data.size();	

	if (cubemap_bytes > 0) {
		cubemap_vertex_buffer = rtg.helpers.create_buffer(
			cubemap_bytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		//copy data to buffer:
		rtg.helpers.transfer_to_buffer(cubemap_vertex_data.data(), cubemap_bytes, cubemap_vertex_buffer);
	}
}

void SceneManager::destroy(RTG &rtg) {
    rtg.helpers.destroy_buffer(std::move(vertex_buffer));
	rtg.helpers.destroy_buffer(std::move(cubemap_vertex_buffer));
}