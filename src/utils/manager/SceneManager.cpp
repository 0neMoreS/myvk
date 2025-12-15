#include "SceneManager.hpp"

void SceneManager::create(RTG &rtg, std::shared_ptr<S72Loader::Document> doc) {
    { //create object vertices
		std::vector< uint8_t > all_vertices;

		// Load vertices from all meshes in the document
		uint32_t vertex_offset = 0;
		for (const auto &mesh : doc->meshes) {
			try {
				std::vector<uint8_t> mesh_data = S72Loader::load_mesh_data(s72_dir, mesh);
				
				ObjectRange object_range;
				object_range.first = vertex_offset;
				object_range.count = mesh.count;
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
}

void SceneManager::destroy(RTG &rtg) {
    rtg.helpers.destroy_buffer(std::move(vertex_buffer));
}