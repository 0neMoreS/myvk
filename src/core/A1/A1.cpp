#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "A1.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

A1::A1(RTG &rtg) : A1(rtg, "origin-check.s72") {
}

A1::A1(RTG &rtg, const std::string &filename) : 
	rtg{rtg}, 
	doc{S72Loader::load_file(s72_dir + filename)}, 
	camera_manager{}, 
	workspace_manager{}, 
	render_pass_manager{},
	lines_pipeline{},
	objects_pipeline{},
	scene_manager{},
	texture_manager{},
	framebuffer_manager{},
	mesh_tree_data{},
	light_tree_data{},
	camera_tree_data{},
	environment_tree_data{}
{
	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);

	render_pass_manager.create(rtg);

	texture_manager.create(rtg, doc, 1);

	lines_pipeline.create(rtg, render_pass_manager.render_pass, 0, texture_manager);
	objects_pipeline.create(rtg, render_pass_manager.render_pass, 0, texture_manager);

	std::vector< std::vector< Pipeline::BlockDescriptorConfig > > block_descriptor_configs_by_pipeline;
	block_descriptor_configs_by_pipeline.push_back(lines_pipeline.block_descriptor_configs);
	block_descriptor_configs_by_pipeline.push_back(objects_pipeline.block_descriptor_configs);
	std::vector< WorkspaceManager::GlobalBufferConfig > global_buffer_configs{
		WorkspaceManager::GlobalBufferConfig{
			.name = "PV",
			.size = sizeof(A1CommonData::PV),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "World",
			.size = sizeof(A1CommonData::World),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		},
	};

	std::vector<size_t> global_buffer_counts{
		// A1LinesPipeline data buffers
		lines_pipeline.data_buffer_name_to_index.size()
	};
	workspace_manager.create(rtg, std::move(block_descriptor_configs_by_pipeline), std::move(global_buffer_configs), std::move(global_buffer_counts), 2);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A1LinesPipeline"], 
		lines_pipeline.block_descriptor_set_name_to_index["PV"], 
		lines_pipeline.block_binding_name_to_index["PV"], 
		"PV",
		sizeof(A1CommonData::PV)
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A1LinesPipeline"], 
		lines_pipeline.block_descriptor_set_name_to_index["PV"], 
		lines_pipeline.block_binding_name_to_index["World"], 
		"World",
		sizeof(A1CommonData::World)
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A1ObjectsPipeline"], 
		objects_pipeline.block_descriptor_set_name_to_index["PV"], 
		objects_pipeline.block_binding_name_to_index["PV"], 
		"PV",
		sizeof(A1CommonData::PV)
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A1ObjectsPipeline"], 
		objects_pipeline.block_descriptor_set_name_to_index["PV"], 
		objects_pipeline.block_binding_name_to_index["World"], 
		"World",
		sizeof(A1CommonData::World)
	);

	scene_manager.create(rtg, doc);

	camera_manager.create(doc, rtg.swapchain_extent.width, rtg.swapchain_extent.height, this->camera_tree_data, rtg.configuration.init_camera_name);
}

A1::~A1() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A1::~A1 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	texture_manager.destroy(rtg);

	scene_manager.destroy(rtg);

	framebuffer_manager.destroy(rtg);

	lines_pipeline.destroy(rtg);
	objects_pipeline.destroy(rtg);

	workspace_manager.destroy(rtg);

	render_pass_manager.destroy(rtg);
}

void A1::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	framebuffer_manager.create(rtg_, swapchain, render_pass_manager);
	camera_manager.resize_all_cameras(swapchain.extent.width, swapchain.extent.height);
	render_pass_manager.update_scissor_and_viewport(rtg_, swapchain.extent);
}

void A1::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspace_manager.workspaces.size());
	assert(render_params.image_index < framebuffer_manager.swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	WorkspaceManager::Workspace &workspace = workspace_manager.workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = framebuffer_manager.swapchain_framebuffers[render_params.image_index];
	//record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	workspace.reset_recording();
	
	{ //begin recording:
		workspace.begin_recording();

		{ //upload world info:
			assert(workspace.global_buffer_pairs["PV"]->host.size == sizeof(A1CommonData::PV));
			workspace.write_global_buffer(rtg, "PV", &pv_matrix, sizeof(A1CommonData::PV));
			assert(workspace.global_buffer_pairs["PV"]->host.size == workspace.global_buffer_pairs["PV"]->device.size);

			assert(workspace.global_buffer_pairs["World"]->host.allocation.mapped);
			workspace.write_global_buffer(rtg, "World", &world_lighting, sizeof(A1CommonData::World));
			assert(workspace.global_buffer_pairs["World"]->host.size == workspace.global_buffer_pairs["World"]->device.size);
		}

		{ // upload line vertices and indices
			if (!line_vertices.empty()) {
				size_t vertex_bytes = line_vertices.size() * sizeof(PosColVertex);

				auto& vertex_buffer_pair = workspace.data_buffer_pairs[pipeline_name_to_index["A1LinesPipeline"]][lines_pipeline.data_buffer_name_to_index["LinesVertex"]];
				if (vertex_buffer_pair->host.handle == VK_NULL_HANDLE || vertex_buffer_pair->host.size < vertex_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((vertex_bytes + 4096) / 4096) * 4096;
					workspace.update_data_buffer_pair(
						rtg, 
						pipeline_name_to_index["A1LinesPipeline"], 
						lines_pipeline.data_buffer_name_to_index["LinesVertex"],
						new_bytes
					);
				}

				{
					assert(vertex_buffer_pair->host.size == vertex_buffer_pair->device.size);
					assert(vertex_buffer_pair->host.size >= vertex_bytes);
					assert(vertex_buffer_pair->host.allocation.mapped);

					workspace.write_data_buffer(rtg, 
						pipeline_name_to_index["A1LinesPipeline"], 
						lines_pipeline.data_buffer_name_to_index["LinesVertex"],
						line_vertices.data(),
						vertex_bytes
					);
				}	
			}

		}

		{ //upload object transforms
			if (!object_instances.empty()) { 
				size_t needed_bytes = object_instances.size() * sizeof(A1ObjectsPipeline::Transform);

				auto& buffer_pair = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1ObjectsPipeline"]][objects_pipeline.block_descriptor_set_name_to_index["Transforms"]].buffer_pairs[objects_pipeline.block_binding_name_to_index["Transforms"]];
				if (buffer_pair->host.handle == VK_NULL_HANDLE || buffer_pair->host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(
						rtg, 
						pipeline_name_to_index["A1ObjectsPipeline"], 
						objects_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						objects_pipeline.block_binding_name_to_index["Transforms"],
						new_bytes
					);
				}

				{
					assert(buffer_pair->host.size == buffer_pair->device.size);
					assert(buffer_pair->host.size >= needed_bytes);
					assert(buffer_pair->host.allocation.mapped);

					std::vector<A1ObjectsPipeline::Transform> transform_data;

					for (const auto& inst : object_instances) {
						transform_data.push_back(inst.transform);
					}
					
					workspace.write_buffer(rtg, pipeline_name_to_index["A1ObjectsPipeline"], 
						objects_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						objects_pipeline.block_binding_name_to_index["Transforms"],
						transform_data.data(),
						needed_bytes
					);
				}	
			}
		}

		{ //memory barrier to make sure copies complete before rendering happens:
			VkMemoryBarrier memory_barrier{
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			};

			vkCmdPipelineBarrier( workspace.command_buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, //srcStageMask
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, //dstStageMask
				0, //dependencyFlags
				1, &memory_barrier, //memoryBarriers (count, data)
				0, nullptr, //bufferMemoryBarriers (count, data)
				0, nullptr //imageMemoryBarriers (count, data)
			);
		}

		
		{ //render pass
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass_manager.render_pass,
				.framebuffer = framebuffer,
				.renderArea{
					.offset = {.x = 0, .y = 0},
					.extent = rtg.swapchain_extent,
				},
				.clearValueCount = uint32_t(render_pass_manager.clears.size()),
				.pClearValues = render_pass_manager.clears.data(),
			};

			vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
			{
				// run pipelines here
				{ //set scissor rectangle:
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &render_pass_manager.scissor);
				}
				{ //configure viewport transform:
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &render_pass_manager.viewport);
				}

				{ // draw with the lines pipeline:
					if (!line_vertices.empty()) {
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline.pipeline);

						{ //use line_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ workspace.data_buffer_pairs[pipeline_name_to_index["A1LinesPipeline"]][lines_pipeline.data_buffer_name_to_index["LinesVertex"]]->device.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind PV descriptor set:
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1LinesPipeline"]][lines_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set;
							std::array< VkDescriptorSet, 1 > descriptor_sets{
								global_descriptor_set //0: World
							};
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								lines_pipeline.layout, //pipeline layout
								0, //first set
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						if(rtg.configuration.open_debug_camera){
							vkCmdDraw(workspace.command_buffer, uint32_t(line_vertices.size()), 1, 0, 0);
						}
					}
				}

				{ //draw with the objects pipeline:
					if (!object_instances.empty()) { //draw with the objects pipeline:
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.pipeline);

						{ //use object_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind Transforms descriptor set:
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1ObjectsPipeline"]][objects_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1ObjectsPipeline"]][objects_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
							auto &textures_descriptor_set = objects_pipeline.set2_TEXTURE_instance;
							std::array< VkDescriptorSet, 3 > descriptor_sets{
								global_descriptor_set, //0: World
								transform_descriptor_set, //1: Transforms
								textures_descriptor_set //2: Textures
							};
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								objects_pipeline.layout, //pipeline layout
								0, //first set
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						//draw all instances:
						for(uint32_t i = 0; i < object_instances.size(); ++i) {
							//draw all instances:
							A1ObjectsPipeline::Push push{
								.MATERIAL_INDEX = static_cast<uint32_t>(object_instances[i].material_index * 5 + 2) // albedo is at texture slot 2 + 5 * material_index
							};

							vkCmdPushConstants(workspace.command_buffer, objects_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
							vkCmdDraw(workspace.command_buffer, object_instances[i].object_ranges.count, 1, object_instances[i].object_ranges.first, i);
						}
					}
				}
			}

			vkCmdEndRenderPass(workspace.command_buffer);
		}

		//end recording:
		workspace.end_recording();
	}

	{ //submit `workspace.command buffer` for the GPU to run:
		std::array< VkSemaphore, 1 > wait_semaphores{
			render_params.image_available
		};
		std::array< VkPipelineStageFlags, 1 > wait_stages{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		static_assert(wait_semaphores.size() == wait_stages.size(), "every semaphore needs a stage");

		std::array< VkSemaphore, 1 > signal_semaphores{
			render_params.image_done
		};
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(wait_semaphores.size()),
			.pWaitSemaphores = wait_semaphores.data(),
			.pWaitDstStageMask = wait_stages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(signal_semaphores.size()),
			.pSignalSemaphores = signal_semaphores.data(),
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, render_params.workspace_available) );
	}
}


void A1::update(float dt) {
	time = fmod(time + dt, 5.0f);

	line_vertices.clear();
	object_instances.clear();

	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);
	SceneTree::update_animation(doc, time);

	// Update camera
	camera_manager.update(dt, camera_tree_data, rtg.configuration.open_debug_camera);
	CameraManager::Frustum frustum = camera_manager.get_frustum();

	{ // update global data
		pv_matrix.PERSPECTIVE = rtg.configuration.open_debug_camera ? camera_manager.get_debug_perspective() : camera_manager.get_perspective();
		pv_matrix.VIEW = rtg.configuration.open_debug_camera ? camera_manager.get_debug_view() : camera_manager.get_view();

		if(light_tree_data.size() > 0){
			// use the first light as sun direction
			const S72Loader::Light& light = doc->lights[light_tree_data[0].light_index];
			const std::optional<S72Loader::Light::Sun>& light_sun = light.sun;
			const glm::mat4 sky_model = BLENDER_TO_VULKAN_4 * light_tree_data[0].model_matrix;
			const glm::mat4 sun_model = BLENDER_TO_VULKAN_4 * light_tree_data[1].model_matrix;

			world_lighting.SKY_DIRECTION.x = 0.0f;
			world_lighting.SKY_DIRECTION.y = 0.0f;
			world_lighting.SKY_DIRECTION.z = -1.0f;

			world_lighting.SKY_ENERGY.r = light.tint.x * light_sun->strength;
			world_lighting.SKY_ENERGY.g = light.tint.y * light_sun->strength;
			world_lighting.SKY_ENERGY.b = light.tint.z * light_sun->strength;

			glm::vec3 sun_dir = glm::normalize(glm::vec3(sun_model[2][0], sun_model[2][1], sun_model[2][2]));
			world_lighting.SUN_DIRECTION.x = sun_dir.x;
			world_lighting.SUN_DIRECTION.y = sun_dir.y;
			world_lighting.SUN_DIRECTION.z = sun_dir.z;

;			world_lighting.SUN_ENERGY.r = light.tint.x * light_sun->strength;
			world_lighting.SUN_ENERGY.g = light.tint.y * light_sun->strength;
			world_lighting.SUN_ENERGY.b = light.tint.z * light_sun->strength;
		} else {
			world_lighting.SKY_DIRECTION.x = 0.0f;
			world_lighting.SKY_DIRECTION.y = 0.0f;
			world_lighting.SKY_DIRECTION.z = 1.0f;

			world_lighting.SKY_ENERGY.r = 0.1f;
			world_lighting.SKY_ENERGY.g = 0.1f;
			world_lighting.SKY_ENERGY.b = 0.2f;

			world_lighting.SUN_DIRECTION.x = 6.0f / 23.0f;
			world_lighting.SUN_DIRECTION.y = 13.0f / 23.0f;
			world_lighting.SUN_DIRECTION.z = 18.0f / 23.0f;

			world_lighting.SUN_ENERGY.r = 1.0f;
			world_lighting.SUN_ENERGY.g = 1.0f;
			world_lighting.SUN_ENERGY.b = 0.9f;
		}
	}

	const std::array<uint8_t, 4> yellow = {0xff, 0xff, 0x00, 0xff};
	const std::array<uint8_t, 4> red = {0xff, 0x00, 0x00, 0xff};
	const std::array<uint8_t, 4> green = {0x00, 0xff, 0x00, 0xff};
	auto add_line = [&](glm::vec3 corners[8] , int i, int j, std::array<uint8_t, 4> color) {
		line_vertices.push_back({{corners[i].x, corners[i].y, corners[i].z}, {color[0], color[1], color[2], color[3]}});
		line_vertices.push_back({{corners[j].x, corners[j].y, corners[j].z}, {color[0], color[1], color[2], color[3]}});
	};

	{ // generate frustum corners and lines for visualization
		auto intersection = [](const auto& p1, const auto& p2, const auto& p3) {
			const glm::vec3& n1 = p1.normal; const glm::vec3& n2 = p2.normal; const glm::vec3& n3 = p3.normal;
			float d1 = p1.distance; float d2 = p2.distance; float d3 = p3.distance;

			glm::vec3 cross23 = glm::cross(n2, n3);
			glm::vec3 cross31 = glm::cross(n3, n1);
			glm::vec3 cross12 = glm::cross(n1, n2);

			return -(d1 * cross23 + d2 * cross31 + d3 * cross12) / glm::dot(n1, cross23);
		};

		// planes: 0:left, 1:right, 2:bottom, 3:top, 4:near, 5:far
		const auto& planes = frustum.planes;
		glm::vec3 corners[8] = {
			intersection(planes[0], planes[2], planes[4]), // Left, Bottom, Near
			intersection(planes[1], planes[2], planes[4]), // Right, Bottom, Near
			intersection(planes[1], planes[3], planes[4]), // Right, Top, Near
			intersection(planes[0], planes[3], planes[4]), // Left, Top, Near
			intersection(planes[0], planes[2], planes[5]), // Left, Bottom, Far
			intersection(planes[1], planes[2], planes[5]), // Right, Bottom, Far
			intersection(planes[1], planes[3], planes[5]), // Right, Top, Far
			intersection(planes[0], planes[3], planes[5])  // Left, Top, Far
		};

		// Near Face
		add_line(corners, 0, 1, yellow); add_line(corners, 1, 2, yellow); add_line(corners, 2, 3, yellow); add_line(corners, 3, 0, yellow);
		// Far Face
		add_line(corners, 4, 5, yellow); add_line(corners, 5, 6, yellow); add_line(corners, 6, 7, yellow); add_line(corners, 7, 4, yellow);
		// Connecting Edges
		add_line(corners, 0, 4, yellow); add_line(corners, 1, 5, yellow); add_line(corners, 2, 6, yellow); add_line(corners, 3, 7, yellow);
	}

	{
		for(auto mtd : mesh_tree_data){
			const size_t mesh_index = mtd.mesh_index;
			const size_t material_index = mtd.material_index;
			const glm::mat4 MODEL = BLENDER_TO_VULKAN_4 * mtd.model_matrix;
			const glm::mat4 MODEL_NORMAL = glm::transpose(glm::inverse(MODEL));
			const auto& object_range = doc->meshes[mesh_index].range;

			// Transform local AABB to world AABB (8 corners method)
            const glm::vec3& bmin = object_range.aabb_min;
            const glm::vec3& bmax = object_range.aabb_max;
            glm::vec3 corners[8] = {
				{bmin.x, bmin.y, bmin.z}, {bmax.x, bmin.y, bmin.z}, {bmin.x, bmax.y, bmin.z}, {bmax.x, bmax.y, bmin.z},
				{bmin.x, bmin.y, bmax.z}, {bmax.x, bmin.y, bmax.z}, {bmin.x, bmax.y, bmax.z}, {bmax.x, bmax.y, bmax.z}
			};
			glm::vec3 world_min(std::numeric_limits<float>::max());
			glm::vec3 world_max(std::numeric_limits<float>::lowest());
			for (int c = 0; c < 8; ++c) {
				glm::vec3 wp = glm::vec3(MODEL * glm::vec4(corners[c], 1.0f));
				world_min = glm::min(world_min, wp);
				world_max = glm::max(world_max, wp);
			}

			glm::vec3 world_corners[8] = {
				{world_min.x, world_min.y, world_min.z}, {world_max.x, world_min.y, world_min.z}, {world_min.x, world_max.y, world_min.z}, {world_max.x, world_max.y, world_min.z},
				{world_min.x, world_min.y, world_max.z}, {world_max.x, world_min.y, world_max.z}, {world_min.x, world_max.y, world_max.z}, {world_max.x, world_max.y, world_max.z}
			};

			if(!frustum.is_box_visible(world_min, world_max)) {
				add_line(world_corners, 0, 1, red); add_line(world_corners, 1, 3, red); add_line(world_corners, 3, 2, red); add_line(world_corners, 2, 0, red);
				// Top face (z = max): 4-5, 5-7, 7-6, 6-4
				add_line(world_corners, 4, 5, red); add_line(world_corners, 5, 7, red); add_line(world_corners, 7, 6, red); add_line(world_corners, 6, 4, red);
				// Vertical edges: 0-4, 1-5, 2-6, 3-7
				add_line(world_corners, 0, 4, red); add_line(world_corners, 1, 5, red); add_line(world_corners, 2, 6, red); add_line(world_corners, 3, 7, red);
				continue;
			} else {
				add_line(world_corners, 0, 1, green); add_line(world_corners, 1, 3, green); add_line(world_corners, 3, 2, green); add_line(world_corners, 2, 0, green);
				// Top face (z = max): 4-5, 5-7, 7-6, 6-4
				add_line(world_corners, 4, 5, green); add_line(world_corners, 5, 7, green); add_line(world_corners, 7, 6, green); add_line(world_corners, 6, 4, green);
				// Vertical edges: 0-4, 1-5, 2-6, 3-7
				add_line(world_corners, 0, 4, green); add_line(world_corners, 1, 5, green); add_line(world_corners, 2, 6, green); add_line(world_corners, 3, 7, green);
			}

			object_instances.emplace_back(ObjectInstance{
				.object_ranges = object_range,
				.transform{
					.MODEL = MODEL,
					.MODEL_NORMAL = MODEL_NORMAL,
				},
				.material_index = material_index,
			});
		}
	}
}


void A1::on_input(InputEvent const &event) {
	camera_manager.on_input(event);
}
