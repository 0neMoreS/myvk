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
	objects_pipeline{},
	scene_manager{},
	texture_manager{}
{
	render_pass_manager.create(rtg);

	objects_pipeline.create(rtg, render_pass_manager.render_pass, 0);

	workspace_manager.create(rtg, objects_pipeline.block_descriptor_configs, uint32_t(objects_pipeline.block_descriptor_configs.size()));
	workspace_manager.update_all_descriptors(rtg, objects_pipeline.block_descriptor_configs, 0, sizeof(world));

	scene_manager.create(rtg, doc);

	texture_manager.create(rtg, doc, objects_pipeline.texture_descriptor_configs);

	camera_manager.create(doc, rtg.swapchain_extent.width, rtg.swapchain_extent.height);
}

A1::~A1() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A1::~A1 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	texture_manager.destroy(rtg);

	scene_manager.destroy(rtg);

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	objects_pipeline.destroy(rtg);

	workspace_manager.destroy(rtg);

	render_pass_manager.destroy(rtg);
}

void A1::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	//[re]create framebuffers:
	//clean up existing framebuffers (and depth image):
	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	//Allocate depth image for framebuffers to share:
	swapchain_depth_image = rtg.helpers.create_image(
		swapchain.extent,
		render_pass_manager.depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ //create depth image view:
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &swapchain_depth_image_view) );
	}

	//Make framebuffers for each swapchain image:
	swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i) {
		std::array< VkImageView, 2 > attachments{
			swapchain.image_views[i],
			swapchain_depth_image_view,
		};
		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass_manager.render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &swapchain_framebuffers[i]) );
	}
}

void A1::destroy_framebuffers() {
	for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
		assert(framebuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
	swapchain_framebuffers.clear();

	assert(swapchain_depth_image_view != VK_NULL_HANDLE);
	vkDestroyImageView(rtg.device, swapchain_depth_image_view, nullptr);
	swapchain_depth_image_view = VK_NULL_HANDLE;

	rtg.helpers.destroy_image(std::move(swapchain_depth_image));
}


void A1::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspace_manager.workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	WorkspaceManager::Workspace &workspace = workspace_manager.workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	//record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	workspace.reset_recoring();
	
	{ //begin recording:
		workspace.begin_recording();

		{ //upload world info:
			assert(workspace.buffer_pairs[0].host.size == sizeof(world));

			//host-side copy into World_src:
			memcpy(workspace.buffer_pairs[0].host.allocation.data(), &world, sizeof(world));

			//add device-side copy from World_src -> World:
			assert(workspace.buffer_pairs[0].host.size == workspace.buffer_pairs[0].device.size);

			workspace.copy_buffer(rtg, objects_pipeline.block_descriptor_configs, 0, sizeof(world));
		}

		{ //upload object transforms
			if (!object_instances.empty()) { 
				size_t needed_bytes = object_instances.size() * sizeof(A1ObjectsPipeline::Transform);
				if (workspace.buffer_pairs[1].host.handle == VK_NULL_HANDLE || workspace.buffer_pairs[1].host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(rtg, objects_pipeline.block_descriptor_configs, 1, new_bytes);
				}

				assert(workspace.buffer_pairs[1].host.size == workspace.buffer_pairs[1].device.size);
				assert(workspace.buffer_pairs[1].host.size >= needed_bytes);

				{ //copy transforms into Transforms_src:
					assert(workspace.buffer_pairs[1].host.allocation.mapped);
					A1ObjectsPipeline::Transform *out = reinterpret_cast< A1ObjectsPipeline::Transform * >(workspace.buffer_pairs[1].host.allocation.data()); // Strict aliasing violation, but it doesn't matter
					for (ObjectInstance const &inst : object_instances) {
						*out = inst.transform;
						++out;
					}
				}

				workspace.copy_buffer(rtg, objects_pipeline.block_descriptor_configs, 1, needed_bytes);
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

		//render pass
		{
			std::array< VkClearValue, 2 > clear_values{
				VkClearValue{ .color{ .float32{63.0f / 255.0f, 63.0f / 255.0f, 63.0f / 255.0f, 1.0f} } },
				VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },
			};
			
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass_manager.render_pass,
				.framebuffer = framebuffer,
				.renderArea{
					.offset = {.x = 0, .y = 0},
					.extent = rtg.swapchain_extent,
				},
				.clearValueCount = uint32_t(clear_values.size()),
				.pClearValues = clear_values.data(),
			};

			vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
			{
				// run pipelines here
				{ //set scissor rectangle:
					VkRect2D scissor{
						.offset = {.x = 0, .y = 0},
						.extent = rtg.swapchain_extent,
					};
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor);
				}
				{ //configure viewport transform:
					VkViewport viewport{
						.x = 0.0f,
						.y = 0.0f,
						.width = float(rtg.swapchain_extent.width),
						.height = float(rtg.swapchain_extent.height),
						.minDepth = 0.0f,
						.maxDepth = 1.0f,
					};
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &viewport);
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
							std::array< VkDescriptorSet, 2 > descriptor_sets{
								workspace.buffer_pairs[0].descriptor, //0: World
								workspace.buffer_pairs[1].descriptor, //1: Transforms
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
						for (ObjectInstance const &inst : object_instances) {
							uint32_t index = uint32_t(&inst - &object_instances[0]);

							//bind texture descriptor set:
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								objects_pipeline.layout, //pipeline layout
								2, //second set
								1, &texture_manager.descriptor_sets[0][inst.texture], //descriptor sets count, ptr //TODO
								0, nullptr //dynamic offsets count, ptr
							);

							vkCmdDraw(workspace.command_buffer, inst.object_ranges.count, 1, inst.object_ranges.first, index);
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
	time = std::fmod(time + dt, 60.0f);

	{ //static sun and sky:
		world.SKY_DIRECTION.x = 0.0f;
		world.SKY_DIRECTION.y = 0.0f;
		world.SKY_DIRECTION.z = 1.0f;

		world.SKY_ENERGY.r = 0.1f;
		world.SKY_ENERGY.g = 0.1f;
		world.SKY_ENERGY.b = 0.2f;

		world.SUN_DIRECTION.x = 6.0f / 23.0f;
		world.SUN_DIRECTION.y = 13.0f / 23.0f;
		world.SUN_DIRECTION.z = 18.0f / 23.0f;

		world.SUN_ENERGY.r = 1.0f;
		world.SUN_ENERGY.g = 1.0f;
		world.SUN_ENERGY.b = 0.9f;
	}

	// Update camera
	camera_manager.update(dt, rtg.swapchain_extent.width, rtg.swapchain_extent.height);

	{
		object_instances.clear();
		glm::mat4 PERSPECTIVE = camera_manager.get_perspective();
		glm::mat4 VIEW = camera_manager.get_view();

		for(uint32_t i = 0; i < doc->meshes.size(); ++i) 
		{
			const auto& mesh = doc->meshes[i];

			for(auto &transform : mesh.transforms){
				glm::mat4 MODEL = BLENDER_TO_VULKAN_4 * transform;
				glm::mat4 MODEL_NORMAL = glm::transpose(glm::inverse(MODEL));

				object_instances.emplace_back(ObjectInstance{
					.object_ranges = scene_manager.object_ranges[i],
					.transform{
						.PERSPECTIVE = PERSPECTIVE,
						.VIEW = VIEW,
						.MODEL = MODEL,
						.MODEL_NORMAL = MODEL_NORMAL,
					},
					.texture = mesh.material_index.value_or(0),
				});
			}
		}
	}
}


void A1::on_input(InputEvent const &event) {
	camera_manager.on_input(event);
}
