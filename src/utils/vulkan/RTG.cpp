#include "RTG.hpp"

#include "VK.hpp"

#include <vulkan/vulkan_core.h>
#if defined(__APPLE__)
#include <vulkan/vulkan_beta.h> //for portability subset
#include <vulkan/vulkan_metal.h> //for VK_EXT_METAL_SURFACE_EXTENSION_NAME
#endif
#include <vulkan/vk_enum_string_helper.h> //useful for debug output
#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <set>

void RTG::Configuration::parse(int argc, char **argv) {
	for (int argi = 1; argi < argc; ++argi) {
		std::string arg = argv[argi];
		if (arg == "--debug") {
			debug = true;
		} else if (arg == "--no-debug") {
			debug = false;
		} else if (arg == "--physical-device") {
			if (argi + 1 >= argc) throw std::runtime_error("--physical-device requires a parameter (a device name).");
			argi += 1;
			physical_device_name = argv[argi];
		} else if (arg == "--drawing-size") {
			if (argi + 2 >= argc) throw std::runtime_error("--drawing-size requires two parameters (width and height).");
			auto conv = [&](std::string const &what) {
				argi += 1;
				std::string val = argv[argi];
				for (size_t i = 0; i < val.size(); ++i) {
					if (val[i] < '0' || val[i] > '9') {
						throw std::runtime_error("--drawing-size " + what + " should match [0-9]+, got '" + val + "'.");
					}
				}
				return std::stoul(val);
			};
			surface_extent.width = conv("width");
			surface_extent.height = conv("height");
		} else if (arg == "--headless"){
			if(argi + 1 >= argc) throw std::runtime_error("--headless requires a parameter (events file name).");
			argi += 1;
			headless_events_filename = argv[argi];
			headless = true;
		}
		else {
			throw std::runtime_error("Unrecognized argument '" + arg + "'.");
		}
	}
}

void RTG::Configuration::usage(std::function< void(const char *, const char *) > const &callback) {
	callback("--debug, --no-debug", "Turn on/off debug and validation layers.");
	callback("--physical-device <name>", "Run on the named physical device (guesses, otherwise).");
	callback("--drawing-size <w> <h>", "Set the size of the surface to draw to.");
	callback("--headless <events file>", "Run in headless mode, reading events from the given file.");
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *data,
	void *user_data
) {
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		std::cerr << "\x1b[91m" << "E: ";
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << "\x1b[33m" << "w: ";
	} else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		std::cerr << "\x1b[90m" << "i: ";
	} else { //VERBOSE
		std::cerr << "\x1b[90m" << "v: ";
	}
	std::cerr << data->pMessage << "\x1b[0m" << std::endl;

	return VK_FALSE;
}

RTG::RTG(Configuration const &configuration_) : helpers(*this) {

	//copy input configuration:
	configuration = configuration_;

	//fill in flags/extensions/layers information:

	{ //create the `instance` (main handle to Vulkan library):
		VkInstanceCreateFlags instance_flags = 0;
		std::vector< const char * > instance_extensions;
		std::vector< const char * > instance_layers;

		//add extensions for MoltenVK portability layer on macOS
		#if defined(__APPLE__)
		instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		instance_extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
		instance_extensions.emplace_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
		#endif

		//add extensions and layers for debugging:
		if (configuration.debug) {
			instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		{ //add extensions needed by glfw (only in windowed mode):
			if (!configuration.headless) {
				glfwInit();
				if (!glfwVulkanSupported()) {
					throw std::runtime_error("GLFW reports Vulkan is not supported.");
				}

				uint32_t count;
				const char **extensions = glfwGetRequiredInstanceExtensions(&count);
				if (extensions == nullptr) {
					throw std::runtime_error("GLFW failed to return a list of requested instance extensions. Perhaps it was not compiled with Vulkan support.");
				}
				for (uint32_t i = 0; i < count; ++i) {
					instance_extensions.emplace_back(extensions[i]);
				}
			}
		}

		//write debug messenger structure
		VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
				| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debug_callback,
			.pUserData = nullptr
		};

		VkInstanceCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = (configuration.debug ? &debug_messenger_create_info : nullptr),
			.flags = instance_flags,
			.pApplicationInfo = &configuration.application_info,
			.enabledLayerCount = uint32_t(instance_layers.size()),
			.ppEnabledLayerNames = instance_layers.data(),
			.enabledExtensionCount = uint32_t(instance_extensions.size()),
			.ppEnabledExtensionNames = instance_extensions.data()
		};
		VK( vkCreateInstance(&create_info, nullptr, &instance) );

		//create debug messenger
		if (configuration.debug) {
			PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (!vkCreateDebugUtilsMessengerEXT) {
				throw std::runtime_error("Failed to lookup debug utils create fn.");
			}
			VK( vkCreateDebugUtilsMessengerEXT(instance, &debug_messenger_create_info, nullptr, &debug_messenger) );
		}
	}

	//create the `window` and `surface` (where things get drawn):
	if (!configuration.headless)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		window = glfwCreateWindow(configuration.surface_extent.width, configuration.surface_extent.height, configuration.application_info.pApplicationName, nullptr, nullptr);

		if (!window) {
			throw std::runtime_error("GLFW failed to create a window.");
		}

		VK( glfwCreateWindowSurface(instance, window, nullptr, &surface) );
	} else {
		//in headless mode, initialize GLFW but don't create a window
		glfwInit();
	}
	{
		std::vector< std::string > physical_device_names; //for later error message
		{ //pick a physical device
			uint32_t count = 0;
			VK( vkEnumeratePhysicalDevices(instance, &count, nullptr) );
			std::vector< VkPhysicalDevice > physical_devices(count);
			VK( vkEnumeratePhysicalDevices(instance, &count, physical_devices.data()) );

			uint32_t best_score = 0;

			for (auto const &pd : physical_devices) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(pd, &properties);

				VkPhysicalDeviceFeatures features;
				vkGetPhysicalDeviceFeatures(pd, &features);

				physical_device_names.emplace_back(properties.deviceName);

				if (!configuration.physical_device_name.empty()) {
					if (configuration.physical_device_name == properties.deviceName) {
						if (physical_device) {
							std::cerr << "WARNING: have two physical devices with the name '" << properties.deviceName << "'; using the first to be enumerated." << std::endl;
						} else {
							physical_device = pd;
						}
					}
				} else {
					uint32_t score = 1;
					if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
						score += 0x8000;
					}

					if (score > best_score) {
						best_score = score;
						physical_device = pd;
					}
				}
			}
		}	

		if (physical_device == VK_NULL_HANDLE) {
			std::cerr << "Physical devices:\n";
			for (std::string const &name : physical_device_names) {
				std::cerr << "    " << name << "\n";
			}
			std::cerr.flush();

			if (!configuration.physical_device_name.empty()) {
				throw std::runtime_error("No physical device with name '" + configuration.physical_device_name + "'.");
			} else {
				throw std::runtime_error("No suitable GPU found.");
			}
		}

		{ //report device name:
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(physical_device, &properties);
			std::cout << "Selected physical device '" << properties.deviceName << "'." << std::endl;
		}
	}

	if(!configuration.headless) {
		//select the `surface_format` and `present_mode` which control how colors are represented on the surface and how new images are supplied to the surface:
		std::vector< VkSurfaceFormatKHR > formats;
		std::vector< VkPresentModeKHR > present_modes;
		
		{
			uint32_t count = 0;
			VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, nullptr) );
			formats.resize(count);
			VK( vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &count, formats.data()) );
		}

		{
			uint32_t count = 0;
			VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, nullptr) );
			present_modes.resize(count);
			VK( vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &count, present_modes.data()) );
		}

		//find first available surface format matching config:
		surface_format = [&](){
			for (auto const &config_format : configuration.surface_formats) {
				for (auto const &format : formats) {
					if (config_format.format == format.format && config_format.colorSpace == format.colorSpace) {
						return format;
					}
				}
			}
			throw std::runtime_error("No format matching requested format(s) found.");
		}();

		//find first available present mode matching config:
		present_mode = [&](){
			for (auto const &config_mode : configuration.present_modes) {
				for (auto const &mode : present_modes) {
					if (config_mode == mode) {
						return mode;
					}
				}
			}
			throw std::runtime_error("No present mode matching requested mode(s) found.");
		}();
	} else {
		//in headless mode, use default format and extent
		surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
		surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	}

	{//create the `device` (logical interface to the GPU) and the `queue`s to which we can submit commands:
		{ //look up queue indices:
			uint32_t count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
			std::vector< VkQueueFamilyProperties > queue_families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_families.data());

			for (auto const &queue_family : queue_families) {
				uint32_t i = uint32_t(&queue_family - &queue_families[0]);

				//if it does graphics, set the graphics queue family:
				if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					if (!graphics_queue_family) graphics_queue_family = i;
				}

				//if it has present support, set the present queue family:
				if (!configuration.headless) {
					VkBool32 present_support = VK_FALSE;
					VK( vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support) );
					if (present_support == VK_TRUE) {
						if (!present_queue_family) present_queue_family = i;
					}
				} else {
					//in headless mode, use graphics queue for present as well
					if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
						if (!present_queue_family) present_queue_family = i;
					}
				}
			}

			if (!graphics_queue_family) {
				throw std::runtime_error("No queue with graphics support.");
			}

			if (!present_queue_family) {
				if (configuration.headless) {
					present_queue_family = graphics_queue_family;
				} else {
					throw std::runtime_error("No queue with present support.");
				}
			}
		}

		//select device extensions:
		std::vector< const char * > device_extensions;
		#if defined(__APPLE__)
		device_extensions.emplace_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		#endif
		//Add the swapchain extension
		device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

		{ //create the logical device:
			std::vector< VkDeviceQueueCreateInfo > queue_create_infos;
			std::set< uint32_t > unique_queue_families{
				graphics_queue_family.value(),
				present_queue_family.value()
			};

			float queue_priorities[1] = { 1.0f };
			for (uint32_t queue_family : unique_queue_families) {
				queue_create_infos.emplace_back(VkDeviceQueueCreateInfo{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueFamilyIndex = queue_family,
					.queueCount = 1,
					.pQueuePriorities = queue_priorities,
				});
			}

			VkPhysicalDeviceDescriptorIndexingFeatures indexing_features{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
				.pNext = nullptr,
				.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
				.descriptorBindingVariableDescriptorCount = VK_TRUE,
				.runtimeDescriptorArray = VK_TRUE,
			};

			VkPhysicalDeviceFeatures2 device_features2{
				.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
				.pNext = &indexing_features,
			};

			VkDeviceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = &device_features2,
				.queueCreateInfoCount = uint32_t(queue_create_infos.size()),
				.pQueueCreateInfos = queue_create_infos.data(),
				.enabledLayerCount = 0,
				.ppEnabledLayerNames = nullptr,
				.enabledExtensionCount = static_cast< uint32_t>(device_extensions.size()),
				.ppEnabledExtensionNames = device_extensions.data(),
				.pEnabledFeatures = nullptr,
			};

			VK( vkCreateDevice(physical_device, &create_info, nullptr, &device) );

			vkGetDeviceQueue(device, graphics_queue_family.value(), 0, &graphics_queue);
			vkGetDeviceQueue(device, present_queue_family.value(), 0, &present_queue);
		}
	}

	//create initial swapchain or headless images:
	if(!configuration.headless) {
		recreate_swapchain();
	} else {
		create_headless_images();
	}

	//create workspace resources:
	workspaces.resize(configuration.workspaces);
	for (auto &workspace : workspaces) {
		{ //create workspace fences:
			VkFenceCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.flags = VK_FENCE_CREATE_SIGNALED_BIT, //start signaled, because all workspaces are available to start
			};

			VK( vkCreateFence(device, &create_info, nullptr, &workspace.workspace_available) );
		}

		{ //create workspace semaphores:
			VkSemaphoreCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			};

			VK( vkCreateSemaphore(device, &create_info, nullptr, &workspace.image_available) );
		}
	}

	//run any resource creation required by Helpers structure:
	helpers.create();
}
RTG::~RTG() {
	//don't destroy until device is idle:
	if (device != VK_NULL_HANDLE) {
		if (VkResult result = vkDeviceWaitIdle(device); result != VK_SUCCESS) {
			std::cerr << "Failed to vkDeviceWaitIdle in RTG::~RTG [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
		}
	}

	//destroy any resource destruction required by Helpers structure:
	helpers.destroy();

	//destroy workspace resources:
	for (auto &workspace : workspaces) {
		if (workspace.workspace_available != VK_NULL_HANDLE) {
			vkDestroyFence(device, workspace.workspace_available, nullptr);
			workspace.workspace_available = VK_NULL_HANDLE;
		}
		if (workspace.image_available != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, workspace.image_available, nullptr);
			workspace.image_available = VK_NULL_HANDLE;
		}
	}
	workspaces.clear();

	//destroy the swapchain or headless images:
	if (!configuration.headless) {
		destroy_swapchain();
	} else {
		destroy_headless_images();
	}

	//destroy the rest of the resources:
	if (device != VK_NULL_HANDLE) {
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (window != nullptr) {
		glfwDestroyWindow(window);
		window = nullptr;
	}

	if (debug_messenger != VK_NULL_HANDLE) {
		PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (vkDestroyDebugUtilsMessengerEXT) {
			vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
			debug_messenger = VK_NULL_HANDLE;
		}
	}

	if (instance != VK_NULL_HANDLE) {
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}


void RTG::recreate_swapchain() {
	//clean up swapchain if it already exists:
	if (!swapchain_images.empty()) {
		destroy_swapchain();
	}

	//determine size, image count, and transform for swapchain:
	VkSurfaceCapabilitiesKHR capabilities;
	VK( vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities) );

	swapchain_extent = capabilities.currentExtent;

	uint32_t requested_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount != 0) {
		requested_count = std::min(capabilities.maxImageCount, requested_count);
	}

	{ //create swapchain
		VkSwapchainCreateInfoKHR create_info{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = requested_count,
			.imageFormat = surface_format.format,
			.imageColorSpace = surface_format.colorSpace,
			.imageExtent = swapchain_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = present_mode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE //NOTE: could be more efficient by passing old swapchain handle here instead of destroying it
		};

		std::vector< uint32_t > queue_family_indices{
			graphics_queue_family.value(),
			present_queue_family.value()
		};

		if (queue_family_indices[0] != queue_family_indices[1]) {
			//if images will be presented on a different queue, make sure they are shared:
			create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			create_info.queueFamilyIndexCount = uint32_t(queue_family_indices.size());
			create_info.pQueueFamilyIndices = queue_family_indices.data();
		} else {
			create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		VK( vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain) );
	}

	{ //get the swapchain images:
		uint32_t count = 0;
		VK( vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr) );
		swapchain_images.resize(count);
		VK( vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data()) );
	}

	//create views for swapchain images:
	swapchain_image_views.assign(swapchain_images.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain_images.size(); ++i) {
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surface_format.format,
			.components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};
		VK( vkCreateImageView(device, &create_info, nullptr, &swapchain_image_views[i]) );
	}

	//create one "image done" semaphore per swapchain image:
	swapchain_image_done_semaphores.assign(swapchain_images.size(), VK_NULL_HANDLE);
	{
		VkSemaphoreCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		for (size_t i = 0; i < swapchain_image_done_semaphores.size(); ++i) {
			VK( vkCreateSemaphore(device, &create_info, nullptr, &swapchain_image_done_semaphores[i]) );
		}
	}

	if (configuration.debug) {
		std::cout << "Swapchain is now " << swapchain_images.size() << " images of size " << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
	}
}


void RTG::destroy_swapchain() {
	VK( vkDeviceWaitIdle(device) ); //wait for any rendering to old swapchain to finish

	//destroy per-swapchain-image semaphores:
	for (auto &semaphore : swapchain_image_done_semaphores) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, semaphore, nullptr);
			semaphore = VK_NULL_HANDLE;
		}
	}
	swapchain_image_done_semaphores.clear();

	//clean up image views referencing the swapchain:
	for (auto &image_view : swapchain_image_views) {
		vkDestroyImageView(device, image_view, nullptr);
		image_view = VK_NULL_HANDLE;
	}
	swapchain_image_views.clear();

	//forget handles to swapchain images (will destroy by deallocating the swapchain itself):
	swapchain_images.clear();

	//deallocate the swapchain and (thus) its images:
	if (swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

//above RTG::run:
static void cursor_pos_callback(GLFWwindow *window, double xpos, double ypos) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	event.type = InputEvent::MouseMotion;
	event.motion.x = float(xpos);
	event.motion.y = float(ypos);
	event.motion.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) {
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) {
			event.motion.state |= (1 << b);
		}
	}

	event_queue->emplace_back(event);
}

//above RTG::run:
static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	if (action == GLFW_PRESS) {
		event.type = InputEvent::MouseButtonDown;
	} else if (action == GLFW_RELEASE) {
		event.type = InputEvent::MouseButtonUp;
	} else {
		std::cerr << "Strange: unknown mouse button action." << std::endl;
		return;
	}

	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	event.button.x = float(xpos);
	event.button.y = float(ypos);
	event.button.state = 0;
	for (int b = 0; b < 8 && b < GLFW_MOUSE_BUTTON_LAST; ++b) {
		if (glfwGetMouseButton(window, b) == GLFW_PRESS) {
			event.button.state |= (1 << b);
		}
	}
	event.button.button = uint8_t(button);
	event.button.mods = uint8_t(mods);

	event_queue->emplace_back(event);
}

//above RTG::run:
static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	event.type = InputEvent::MouseWheel;
	event.wheel.x = float(xoffset);
	event.wheel.y = float(yoffset);

	event_queue->emplace_back(event);
}

//above RTG::run:
static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	std::vector< InputEvent > *event_queue = reinterpret_cast< std::vector< InputEvent > * >(glfwGetWindowUserPointer(window));
	if (!event_queue) return;

	InputEvent event;
	std::memset(&event, '\0', sizeof(event));

	if (action == GLFW_PRESS) {
		event.type = InputEvent::KeyDown;
	} else if (action == GLFW_RELEASE) {
		event.type = InputEvent::KeyUp;
	} else if (action == GLFW_REPEAT) {
		//ignore repeats
		return;
	} else {
		std::cerr << "Strange: got unknown keyboard action." << std::endl;
	}

	event.key.key = key;
	event.key.mods = mods;

	event_queue->emplace_back(event);
}

void RTG::run(Application &application) {
	//initial on_swapchain
	auto on_swapchain = [&,this]() {
		application.on_swapchain(*this, SwapchainEvent{
			.extent = swapchain_extent,
			.images = swapchain_images,
			.image_views = swapchain_image_views,
		});
	};
	on_swapchain();

	if (configuration.headless) {
		//Headless mode: read events from file and render
		std::vector< InputEvent > event_queue;
		
		//load events from file if specified
		if (!configuration.headless_events_filename.empty()) {
			//TODO: implement event file reading
			std::cerr << "NOTE: event file reading not yet implemented, running in headless mode without events." << std::endl;
		}
		
		//setup time handling
		std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();
		
		uint32_t next_image_index = 0;
		uint32_t frame_count = 0;
		const uint32_t max_frames = 100; //render 100 frames in headless mode by default
		
		while (frame_count < max_frames) {
			//deliver all input events to application:
			for (InputEvent const &input : event_queue) {
				application.on_input(input);
			}
			event_queue.clear();
			
			{ //elapsed time handling:
				std::chrono::high_resolution_clock::time_point after = std::chrono::high_resolution_clock::now();
				float dt = float(std::chrono::duration< double >(after - before).count());
				before = after;
				
				dt = std::min(dt, 0.1f); //lag if frame rate dips too low
				
				application.update(dt);
			}
			
			uint32_t workspace_index;
			{ //acquire a workspace:
				assert(next_workspace < workspaces.size());
				workspace_index = next_workspace;
				next_workspace = (next_workspace + 1) % workspaces.size();
				
				//wait until the workspace is not being used:
				VK( vkWaitForFences(device, 1, &workspaces[workspace_index].workspace_available, VK_TRUE, UINT64_MAX) );
				
				//mark the workspace as in use:
				VK( vkResetFences(device, 1, &workspaces[workspace_index].workspace_available) );
			}
			
			//simulate acquiring an image by cycling through indices:
			uint32_t image_index = next_image_index;
			next_image_index = (next_image_index + 1) % headless_images.size();
			
			//In headless mode, manually signal the image_available semaphore
			//This simulates what vkAcquireNextImageKHR does in windowed mode
			{
				VkSubmitInfo signal_info{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.commandBufferCount = 0, //no commands to execute
					.signalSemaphoreCount = 1,
					.pSignalSemaphores = &workspaces[workspace_index].image_available,
				};
				
				VK( vkQueueSubmit(graphics_queue, 1, &signal_info, VK_NULL_HANDLE) );
			}
			
			//call render function:
			application.render(*this, RenderParams{
				.workspace_index = workspace_index,
				.image_index = image_index,
				.image_available = workspaces[workspace_index].image_available,
				.image_done = swapchain_image_done_semaphores.at(image_index),
				.workspace_available = workspaces[workspace_index].workspace_available,
			});
			
			//In headless mode, wait for rendering to complete
			//This simulates what vkQueuePresentKHR does in windowed mode
			{
				VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				VkSubmitInfo wait_info{
					.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &swapchain_image_done_semaphores.at(image_index),
					.pWaitDstStageMask = &wait_stage,
					.commandBufferCount = 0, //no commands to execute
				};
				
				//Use a fence to signal when this "presentation" is done
				VK( vkQueueSubmit(graphics_queue, 1, &wait_info, VK_NULL_HANDLE) );
			}

			++frame_count;

			std::cout << "Headless: rendered " << frame_count << " frames." << std::endl;
		}
		//wait for all work to complete:
		VK(vkDeviceWaitIdle(device));
	} else {
		//Windowed mode: original implementation
		//setup event handling:
		std::vector< InputEvent > event_queue;
		glfwSetWindowUserPointer(window, &event_queue);

		glfwSetCursorPosCallback(window, cursor_pos_callback);
		glfwSetMouseButtonCallback(window, mouse_button_callback);
		glfwSetScrollCallback(window, scroll_callback);
		glfwSetKeyCallback(window, key_callback);

		//setup time handling
		std::chrono::high_resolution_clock::time_point before = std::chrono::high_resolution_clock::now();

		while (!glfwWindowShouldClose(window)) {
			//event handling:
			glfwPollEvents();

			//deliver all input events to application:
			for (InputEvent const &input : event_queue) {
				application.on_input(input);
			}
			event_queue.clear();

			{ //elapsed time handling:
				std::chrono::high_resolution_clock::time_point after = std::chrono::high_resolution_clock::now();
				float dt = float(std::chrono::duration< double >(after - before).count());
				before = after;

				dt = std::min(dt, 0.1f); //lag if frame rate dips too low

				application.update(dt);
			}

			uint32_t workspace_index;
			{ //acquire a workspace:
				assert(next_workspace < workspaces.size());
				workspace_index = next_workspace;
				next_workspace = (next_workspace + 1) % workspaces.size();

				//wait until the workspace is not being used:
				VK( vkWaitForFences(device, 1, &workspaces[workspace_index].workspace_available, VK_TRUE, UINT64_MAX) );

				//mark the workspace as in use:
				VK( vkResetFences(device, 1, &workspaces[workspace_index].workspace_available) );
			}

			uint32_t image_index = -1U;
		//acquire an image:
retry:
			//Ask the swapchain for the next image index -- note careful return handling:
			if (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, workspaces[workspace_index].image_available, VK_NULL_HANDLE, &image_index);
				result == VK_ERROR_OUT_OF_DATE_KHR) {
				//if the swapchain is out-of-date, recreate it and run the loop again:
				std::cerr << "Recreating swapchain because vkAcquireNextImageKHR returned " << string_VkResult(result) << "." << std::endl;
				
				recreate_swapchain();
				on_swapchain();

				goto retry;
			} else if (result == VK_SUBOPTIMAL_KHR) {
				//if the swapchain is suboptimal, render to it and recreate it later:
				std::cerr << "Suboptimal swapchain format -- ignoring for the moment." << std::endl;
			} else if (result != VK_SUCCESS) {
				//other non-success results are genuine errors:
				throw std::runtime_error("Failed to acquire swapchain image (" + std::string(string_VkResult(result)) + ")!");
			}

			VkSemaphore image_done = swapchain_image_done_semaphores.at(image_index);

			//call render function:
			application.render(*this, RenderParams{
				.workspace_index = workspace_index,
				.image_index = image_index,
				.image_available = workspaces[workspace_index].image_available,
				.image_done = image_done,
				.workspace_available = workspaces[workspace_index].workspace_available,
			});

			{ //queue the work for presentation:
				VkPresentInfoKHR present_info{
					.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
					.waitSemaphoreCount = 1,
					.pWaitSemaphores = &image_done,
					.swapchainCount = 1,
					.pSwapchains = &swapchain,
					.pImageIndices = &image_index,
				};

				assert(present_queue);

				//note, again, the careful return handling:
				if (VkResult result = vkQueuePresentKHR(present_queue, &present_info);
					result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
					std::cerr << "Recreating swapchain because vkQueuePresentKHR returned " << string_VkResult(result) << "." << std::endl;
					recreate_swapchain();
					on_swapchain();
				} else if (result != VK_SUCCESS) {
					throw std::runtime_error("failed to queue presentation of image (" + std::string(string_VkResult(result)) + ")!");
				}
			}
		} //end while (!glfwWindowShouldClose(window))
	} //end else (windowed mode)

	//tear down event handling (windowed mode only):
	if (!configuration.headless) {
		glfwSetMouseButtonCallback(window, nullptr);
		glfwSetCursorPosCallback(window, nullptr);
		glfwSetScrollCallback(window, nullptr);
		glfwSetKeyCallback(window, nullptr);

		glfwSetWindowUserPointer(window, nullptr);
	}
}

void RTG::create_headless_images() {
	// Create a fixed number of images (e.g., 3 for triple buffering)
	uint32_t image_count = 3;
	
	headless_images.resize(image_count);
	headless_image_memory.resize(image_count);
	headless_image_views.resize(image_count);

	swapchain_extent = configuration.surface_extent;
	
	for (uint32_t i = 0; i < image_count; ++i) {
		// Create image
		VkImageCreateInfo image_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = surface_format.format,
			.extent = {
				.width = swapchain_extent.width,
				.height = swapchain_extent.height,
				.depth = 1
			},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		};
		
		VK(vkCreateImage(device, &image_info, nullptr, &headless_images[i]));
		
		// Allocate memory for image
		VkMemoryRequirements mem_requirements;
		vkGetImageMemoryRequirements(device, headless_images[i], &mem_requirements);
		
		VkPhysicalDeviceMemoryProperties mem_properties;
		vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
		
		uint32_t memory_type_index = uint32_t(-1);
		for (uint32_t j = 0; j < mem_properties.memoryTypeCount; ++j) {
			if ((mem_requirements.memoryTypeBits & (1 << j)) &&
				(mem_properties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
				memory_type_index = j;
				break;
			}
		}
		
		if (memory_type_index == uint32_t(-1)) {
			throw std::runtime_error("Failed to find suitable memory type for headless image.");
		}
		
		VkMemoryAllocateInfo alloc_info{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = mem_requirements.size,
			.memoryTypeIndex = memory_type_index,
		};
		
		VK(vkAllocateMemory(device, &alloc_info, nullptr, &headless_image_memory[i]));
		VK(vkBindImageMemory(device, headless_images[i], headless_image_memory[i], 0));
		
		// Create image view
		VkImageViewCreateInfo view_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = headless_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surface_format.format,
			.components{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		
		VK(vkCreateImageView(device, &view_info, nullptr, &headless_image_views[i]));
	}
	
	// Populate swapchain_images and swapchain_image_views for compatibility
	swapchain_images = headless_images;
	swapchain_image_views = headless_image_views;

	//create one "image done" semaphore per swapchain image:
	swapchain_image_done_semaphores.assign(swapchain_images.size(), VK_NULL_HANDLE);
	{
		VkSemaphoreCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		for (size_t i = 0; i < swapchain_image_done_semaphores.size(); ++i) {
			VK( vkCreateSemaphore(device, &create_info, nullptr, &swapchain_image_done_semaphores[i]) );
		}
	}
	
	if (configuration.debug) {
		std::cout << "Created " << image_count << " headless images of size " 
				  << swapchain_extent.width << "x" << swapchain_extent.height << "." << std::endl;
	}
}

void RTG::destroy_headless_images() {
	VK(vkDeviceWaitIdle(device));
	
	for (auto &image_view : headless_image_views) {
		if (image_view != VK_NULL_HANDLE) {
			vkDestroyImageView(device, image_view, nullptr);
			image_view = VK_NULL_HANDLE;
		}
	}
	
	for (auto &image : headless_images) {
		if (image != VK_NULL_HANDLE) {
			vkDestroyImage(device, image, nullptr);
			image = VK_NULL_HANDLE;
		}
	}
	
	for (auto &memory : headless_image_memory) {
		if (memory != VK_NULL_HANDLE) {
			vkFreeMemory(device, memory, nullptr);
			memory = VK_NULL_HANDLE;
		}
	}
	
	headless_image_views.clear();
	headless_images.clear();
	headless_image_memory.clear();
	swapchain_images.clear();
	swapchain_image_views.clear();
}
