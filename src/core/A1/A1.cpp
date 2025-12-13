#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "A1.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

A1::A1(RTG &rtg_) : A1(rtg_, "./external/s72/examples/origin-check.s72") {
}

A1::A1(RTG &rtg_, const std::string &filename) : rtg(rtg_), doc(S72Loader::load_file(filename)) {
	//select a depth format:
	//  (at least one of these two must be supported, according to the spec; but neither are required)
	depth_format = rtg.helpers.find_image_format(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	{ //create render pass
		//attachments
		std::array< VkAttachmentDescription, 2 > attachments{
			VkAttachmentDescription{ //0 - color attachment:
				.format = rtg.surface_format.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			VkAttachmentDescription{ //1 - depth attachment:
				.format = depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};

		//subpass
		VkAttachmentReference color_attachment_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depth_attachment_ref{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
			.pDepthStencilAttachment = &depth_attachment_ref,
		};

		//dependencies
		//this defers the image load actions for the attachments:
		std::array< VkSubpassDependency, 2 > dependencies {
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			},
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(dependencies.size()),
			.pDependencies = dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &create_info, nullptr, &render_pass) );
	}

	{ //create command pool
		VkCommandPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rtg.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &command_pool) );
	}

	objects_pipeline.create(rtg, render_pass, 0);

	{ //create descriptor pool:
		uint32_t per_workspace = uint32_t(rtg.workspaces.size()); //for easier-to-read counting

		std::array< VkDescriptorPoolSize, 2> pool_sizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2 * per_workspace, //one descriptor per set, one set per workspace
			},
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1 * per_workspace, //one descriptor per set, one set per workspace
			},
		};
		
		VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 3 * per_workspace, //one set per workspace
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool) );
	}

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) {
		{ //allocate command buffer:
			VkCommandBufferAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = command_pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &workspace.command_buffer) );
		}

		workspace.World_src = rtg.helpers.create_buffer(
			sizeof(ObjectsPipeline::World),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			Helpers::Mapped
		);
		workspace.World = rtg.helpers.create_buffer(
			sizeof(ObjectsPipeline::World),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			Helpers::Unmapped
		);

		{ //allocate descriptor set for World descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set0_World,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.World_descriptors) );
			//NOTE: will actually fill in this descriptor set just a bit lower
		}

		{ //allocate descriptor set for Transforms descriptor
			VkDescriptorSetAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &objects_pipeline.set1_Transforms,
			};

			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &workspace.Transforms_descriptors) );
			//NOTE: will fill in this descriptor set in render when buffers are [re-]allocated
		}

		{ //point descriptor to World buffer:
			VkDescriptorBufferInfo World_info{
				.buffer = workspace.World.handle,
				.offset = 0,
				.range = workspace.World.size,
			};

			std::array< VkWriteDescriptorSet, 1 > writes{
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = workspace.World_descriptors,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.pBufferInfo = &World_info,
				},
			};

			vkUpdateDescriptorSets(
				rtg.device, //device
				uint32_t(writes.size()), //descriptorWriteCount
				writes.data(), //pDescriptorWrites
				0, //descriptorCopyCount
				nullptr //pDescriptorCopies
			);
		}
	}

	{ //create object vertices
		std::vector< uint8_t > all_vertices;

		// Load vertices from all meshes in the document
		uint32_t vertex_offset = 0;
		for (const auto &mesh : doc->meshes) {
			try {
				std::vector<uint8_t> mesh_data = S72Loader::load_mesh_data(s72_dir, mesh);
				
				ObjectVertices vertices;
				vertices.first = vertex_offset;
				vertices.count = mesh.count;
				object_vertices_list.push_back(vertices);

				all_vertices.insert(all_vertices.end(), mesh_data.begin(), mesh_data.end());
				vertex_offset += mesh.count;
			} catch (const std::exception &e) {
				std::cerr << "Warning: Failed to load mesh '" << mesh.name << "': " << e.what() << std::endl;
			}
		}

		size_t bytes = all_vertices.size();

		if (bytes > 0) {
			object_vertices = rtg.helpers.create_buffer(
				bytes,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				Helpers::Unmapped
			);

			//copy data to buffer:
			rtg.helpers.transfer_to_buffer(all_vertices.data(), bytes, object_vertices);
		}
	}

	{ // make some textures
		textures.resize(doc->materials.size());
		for (const auto &mesh : doc->meshes) {
			if(mesh.material_index.has_value()) {
				S72Loader::Material const &material = doc->materials[mesh.material_index.value()];
				if(material.lambertian.has_value()) {
					if(material.lambertian.value().albedo_texture.has_value()){
						S72Loader::Texture const &texture = material.lambertian.value().albedo_texture.value();
						// TODO: handle other types and other formats of textures
						std::string texture_path = s72_dir + "/" + texture.src;
						// textures[mesh.material_index.value()] = Texture2DLoader::load_png(rtg.helpers, texture_path, VK_FILTER_LINEAR);
						textures[mesh.material_index.value()] = Texture2DLoader::load_png(rtg.helpers, texture_path, VK_FILTER_LINEAR);
					}
					else if(material.lambertian.value().albedo_value.has_value()) {
						textures[mesh.material_index.value()] = Texture2DLoader::create_rgb_texture(rtg.helpers, material.lambertian.value().albedo_value.value());
					}
				}
				else {
					std::cerr << "Warning: Material '" << material.name << "' has no texture." << std::endl;
				}
			}
			else {
				std::cerr << "Warning: Mesh '" << mesh.name << "' has no material." << std::endl;
			}
		}
	}
		
	{ // create the texture descriptor pool
		uint32_t per_texture = uint32_t(textures.size()); //for easier-to-read counting

		std::array< VkDescriptorPoolSize, 1> pool_sizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1 * 1 * per_texture, //one descriptor per set, one set per texture
			},
		};
		
		VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 1 * per_texture, //one set per texture
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &texture_descriptor_pool) );
	}

	{ //allocate and write the texture descriptor sets

		//allocate the descriptors (using the same alloc_info):
		VkDescriptorSetAllocateInfo alloc_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = texture_descriptor_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &objects_pipeline.set2_TEXTURE,
		};
		texture_descriptors.assign(textures.size(), VK_NULL_HANDLE);
		for (VkDescriptorSet &descriptor_set : texture_descriptors) {
			VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
		}

		std::vector< VkDescriptorImageInfo > infos(textures.size());
		std::vector< VkWriteDescriptorSet > writes(textures.size());

		for (auto const &texture : textures) {
			size_t i = &texture - &textures[0];
			
			infos[i] = VkDescriptorImageInfo{
				.sampler = texture->sampler,
				.imageView = texture->image_view,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			};
			writes[i] = VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = texture_descriptors[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[i],
			};
		}

		vkUpdateDescriptorSets( rtg.device, uint32_t(writes.size()), writes.data(), 0, nullptr );
	}

	{ // init camera
		if( !doc->cameras.empty() && doc->cameras[0].parent.has_value()) {
			const S72Loader::Node &camera_node = doc->nodes[*doc->cameras[0].parent];
			camera_position =  BLENDER_TO_VULKAN_3 * camera_node.translation;

            glm::quat blender_rotation = glm::quat(camera_node.rotation.w, camera_node.rotation.x, camera_node.rotation.y, camera_node.rotation.z);
            glm::mat4 blender_rotation_matrix = glm::mat4_cast(blender_rotation);
			glm::vec3 blender_foraward = blender_rotation_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
            glm::vec3 camera_forward = BLENDER_TO_VULKAN_3 * blender_foraward;
            camera_theta = std::acos(-camera_forward.y);
			camera_phi = std::atan2(camera_forward.z, camera_forward.x);

			glm::vec3 blender_up = blender_rotation_matrix * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
			camera_up = BLENDER_TO_VULKAN_3 * blender_up;
		}
	}
}

A1::~A1() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A1::~A1 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (texture_descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, texture_descriptor_pool, nullptr);
		texture_descriptor_pool = nullptr;

		//this also frees the descriptor sets allocated from the pool:
		texture_descriptors.clear();
	}

	// if (texture_sampler) {
	// 	vkDestroySampler(rtg.device, texture_sampler, nullptr);
	// 	texture_sampler = VK_NULL_HANDLE;
	// }

	// for (VkImageView &view : texture_views) {
	// 	vkDestroyImageView(rtg.device, view, nullptr);
	// 	view = VK_NULL_HANDLE;
	// }
	// texture_views.clear();

	for(auto &texture : textures) {
		Texture2DLoader::destroy_texture(texture, rtg);
	}
	textures.clear();

	rtg.helpers.destroy_buffer(std::move(object_vertices));

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	for (Workspace &workspace : workspaces) {
		if (workspace.command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(rtg.device, command_pool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
		}

		if (workspace.World_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.World_src));
		}
		if (workspace.World.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.World));
		}
		//World_descriptors freed when pool is destroyed.

		if (workspace.Transforms_src.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
		}
		if (workspace.Transforms.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
		}
		//Transforms_descriptors freed when pool is destroyed.
	}
	workspaces.clear();

	if (descriptor_pool) {
		vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
		descriptor_pool = nullptr;
		//(this also frees the descriptor sets allocated from the pool)
	}

	objects_pipeline.destroy(rtg);

	if (command_pool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(rtg.device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
	}

	if (render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}
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
		depth_format,
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
			.format = depth_format,
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
			.renderPass = render_pass,
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
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	//record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	VK( vkResetCommandBuffer(workspace.command_buffer, 0) );
	
	{ //begin recording:
		VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		
		VK( vkBeginCommandBuffer(workspace.command_buffer, &begin_info) );

		{ //upload world info:
			assert(workspace.World_src.size == sizeof(world));

			//host-side copy into World_src:
			memcpy(workspace.World_src.allocation.data(), &world, sizeof(world));

			//add device-side copy from World_src -> World:
			assert(workspace.World_src.size == workspace.World.size);
			VkBufferCopy copy_region{
				.srcOffset = 0,
				.dstOffset = 0,
				.size = workspace.World_src.size,
			};
			vkCmdCopyBuffer(workspace.command_buffer, workspace.World_src.handle, workspace.World.handle, 1, &copy_region);
		}

		{
			if (!object_instances.empty()) { //upload object transforms:
				size_t needed_bytes = object_instances.size() * sizeof(ObjectsPipeline::Transform);
				if (workspace.Transforms_src.handle == VK_NULL_HANDLE || workspace.Transforms_src.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					if (workspace.Transforms_src.handle) {
						rtg.helpers.destroy_buffer(std::move(workspace.Transforms_src));
					}
					if (workspace.Transforms.handle) {
						rtg.helpers.destroy_buffer(std::move(workspace.Transforms));
					}
					workspace.Transforms_src = rtg.helpers.create_buffer(
						new_bytes,
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT, //going to have GPU copy from this memory
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //host-visible memory, coherent (no special sync needed)
						Helpers::Mapped //get a pointer to the memory
					);
					workspace.Transforms = rtg.helpers.create_buffer(
						new_bytes,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, //going to use as storage buffer, also going to have GPU into this memory
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //GPU-local memory
						Helpers::Unmapped //don't get a pointer to the memory
					);

					//update the descriptor set:
					VkDescriptorBufferInfo Transforms_info{
						.buffer = workspace.Transforms.handle,
						.offset = 0,
						.range = workspace.Transforms.size,
					};

					std::array< VkWriteDescriptorSet, 1 > writes{
						VkWriteDescriptorSet{
							.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							.dstSet = workspace.Transforms_descriptors,
							.dstBinding = 0,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
							.pBufferInfo = &Transforms_info,
						},
					};

					vkUpdateDescriptorSets(
						rtg.device,
						uint32_t(writes.size()), writes.data(), //descriptorWrites count, data
						0, nullptr //descriptorCopies count, data
					);
				}

				assert(workspace.Transforms_src.size == workspace.Transforms.size);
				assert(workspace.Transforms_src.size >= needed_bytes);

				{ //copy transforms into Transforms_src:
					assert(workspace.Transforms_src.allocation.mapped);
					ObjectsPipeline::Transform *out = reinterpret_cast< ObjectsPipeline::Transform * >(workspace.Transforms_src.allocation.data()); // Strict aliasing violation, but it doesn't matter
					for (ObjectInstance const &inst : object_instances) {
						*out = inst.transform;
						++out;
					}
				}

				//device-side copy from Transforms_src -> Transforms:
				VkBufferCopy copy_region{
					.srcOffset = 0,
					.dstOffset = 0,
					.size = needed_bytes,
				};
				vkCmdCopyBuffer(workspace.command_buffer, workspace.Transforms_src.handle, workspace.Transforms.handle, 1, &copy_region);
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
				VkClearValue{ .color{ .float32{1.0f, 0.0f, 1.0f, 1.0f} } },
				VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },
			};
			
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass,
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
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.handle);

						{ //use object_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ object_vertices.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind Transforms descriptor set:
							std::array< VkDescriptorSet, 2 > descriptor_sets{
								workspace.World_descriptors, //0: World
								workspace.Transforms_descriptors, //1: Transforms
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
								1, &texture_descriptors[inst.texture], //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);

							vkCmdDraw(workspace.command_buffer, inst.vertices.count, 1, inst.vertices.first, index);
						}
					}
				}
			}

			vkCmdEndRenderPass(workspace.command_buffer);
		}

		//end recording:
		VK( vkEndCommandBuffer(workspace.command_buffer) );
	}

	{//submit `workspace.command buffer` for the GPU to run:
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

	{ // Camera movement
		// keyboard rotate
		if(keys_down[GLFW_KEY_J]) {
			camera_phi += rotate_speed * dt;
		}
		if(keys_down[GLFW_KEY_L]) {
			camera_phi -= rotate_speed * dt;
		}
		if(keys_down[GLFW_KEY_I]) {
			camera_theta -= rotate_speed * dt;
		}
		if(keys_down[GLFW_KEY_K]) {
			camera_theta += rotate_speed * dt;
		}

		glm::vec3 forward = glm::vec3(
			std::sin(camera_theta) * std::cos(camera_phi),
			-std::cos(camera_theta),
			std::sin(camera_theta) * std::sin(camera_phi)
		);

		glm::vec3 right = glm::normalize(glm::cross(forward, camera_up));
		// camera_up = glm::normalize(glm::cross(right, forward));
		// keyboard movement
		if (keys_down[GLFW_KEY_W]) {
			camera_position += forward * move_speed * dt;
		}
		if (keys_down[GLFW_KEY_S]) {
			camera_position -= forward * move_speed * dt;
		}
		if (keys_down[GLFW_KEY_A]) {
			camera_position -= right * move_speed * dt;
		}
		if (keys_down[GLFW_KEY_D]) {
			camera_position += right * move_speed * dt;
		}
		if (keys_down[GLFW_KEY_Q]) {
			camera_position += camera_up * move_speed * dt;
		}
		if (keys_down[GLFW_KEY_E]) {
			camera_position -= camera_up * move_speed * dt;
		}

		// keyboard fov
		if(keys_down[GLFW_KEY_R]) {
			camera_fov += fov_speed * dt;
		}
		if(keys_down[GLFW_KEY_F]) {
			camera_fov -= fov_speed * dt;
		}

		// Update VIEW matrix
		VIEW = glm::lookAtRH(camera_position, camera_position + forward, camera_up);

		// Update PERSPECTIVE matrix with current FOV
		PERSPECTIVE = glm::perspectiveRH_ZO(camera_fov, rtg.swapchain_extent.width / float(rtg.swapchain_extent.height), 0.1f, 1000.0f);
		PERSPECTIVE[1][1] *= -1.0f; // flip Y for Vulkan
	}
	
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

	{
		object_instances.clear();
		for(uint32_t i = 0; i < doc->meshes.size(); ++i) 
		{
			const auto& mesh = doc->meshes[i];
			
			glm::mat4 MODEL = glm::mat4(1.0f);
			
			if (mesh.parent.has_value()) {
				const S72Loader::Node &node = doc->nodes[*mesh.parent];
				glm::quat q(node.rotation.w, node.rotation.x, node.rotation.y, node.rotation.z);
				
				glm::mat4 T = glm::translate(glm::mat4(1.0f), node.translation);
				glm::mat4 R = glm::mat4_cast(q);
				glm::mat4 S = glm::scale(glm::mat4(1.0f), node.scale);
				
				MODEL = BLENDER_TO_VULKAN_4 * (T * R * S);
			}

			
			glm::mat4 MODEL_NORMAL = glm::transpose(glm::inverse(MODEL));

			object_instances.emplace_back(ObjectInstance{
				.vertices = object_vertices_list[i],
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


void A1::on_input(InputEvent const &event) {
	if (event.type == InputEvent::MouseWheel) {
		camera_fov -= event.wheel.y * fov_speed;
	} else if (event.type == InputEvent::KeyDown) {
		if (event.key.key >= 0 && event.key.key <= GLFW_KEY_LAST) {
			keys_down[event.key.key] = true;
		}
	} else if (event.type == InputEvent::KeyUp) {
		if (event.key.key >= 0 && event.key.key <= GLFW_KEY_LAST) {
			keys_down[event.key.key] = false;
		}
	} else if (event.type == InputEvent::MouseMotion) {
		// float dx = event.motion.x - last_mouse_x;
		// float dy = event.motion.y - last_mouse_y;

		// const float sensitivity = 0.0005f;
		// camera_phi += dx * sensitivity;
		// camera_theta += dy * sensitivity;
		
		// last_mouse_x = event.motion.x;
		// last_mouse_y = event.motion.y;
	}
}
