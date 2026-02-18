#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"

#include <string>
#include <vector>

// Pre-integrates a cubemap (stored as RGBE PNG atlas) for IBL:
//   run_lambertian: outputs a 32x32 irradiance cubemap
//   run_ggx:        outputs 5 mip levels of GGX-prefiltered specular cubemaps
struct CubeIntegrator {
    CubeIntegrator(RTG &rtg);
    ~CubeIntegrator();
    CubeIntegrator(CubeIntegrator const &) = delete;

    // Read in.png (RGBE atlas), integrate Lambertian, write out.png (RGBE atlas, 32x192)
    void run_lambertian(const std::string &in_path, const std::string &out_path);

    // Read in.png (RGBE atlas), integrate GGX at 5 roughness levels,
    // write out.1.png (512x3072) ... out.5.png (32x192)
    void run_ggx(const std::string &in_path, const std::string &out_stem);

private:
    RTG &rtg;

    // --- Lambertian pipeline ---
    VkDescriptorSetLayout lambertian_descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout      lambertian_pipeline_layout       = VK_NULL_HANDLE;
    VkPipeline            lambertian_pipeline              = VK_NULL_HANDLE;
    VkShaderModule        lambertian_shader                = VK_NULL_HANDLE;

    // --- GGX pipeline ---
    VkDescriptorSetLayout ggx_descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout      ggx_pipeline_layout       = VK_NULL_HANDLE;
    VkPipeline            ggx_pipeline              = VK_NULL_HANDLE;
    VkShaderModule        ggx_shader                = VK_NULL_HANDLE;

    // Descriptor pool (shared for both pipelines; re-allocated per dispatch)
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    // Command pool on the graphics queue (which supports compute)
    VkCommandPool  command_pool   = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkFence         fence          = VK_NULL_HANDLE;

    void create_pipelines();
    void destroy_pipelines();

    // Load the RGBE atlas PNG and upload to a VK_FORMAT_R32G32B32A32_SFLOAT image2DArray (6 layers)
    struct LoadedCubemap {
        Helpers::AllocatedImage image;
        VkImageView             view;
        VkSampler               sampler;
        uint32_t                face_size; // width (and height per face) in pixels
    };
    LoadedCubemap load_input(const std::string &path);
    void destroy_loaded_cubemap(LoadedCubemap &c);

    // Allocate a rgba32f image2DArray output (face_size x face_size x 6)
    struct OutputImage {
        Helpers::AllocatedImage image;
        VkImageView             view;
        uint32_t                face_size;
    };
    OutputImage create_output(uint32_t face_size);
    void destroy_output(OutputImage &o);

    // Readback from GPU image to CPU float buffer, encode RGBE, write PNG atlas
    void readback_and_save(const OutputImage &out, const std::string &path);

    // Submit a compute dispatch and wait for completion
    void dispatch_and_wait(
        VkPipeline pipeline,
        VkPipelineLayout layout,
        VkDescriptorSet descriptor_set,
        uint32_t groups_x, uint32_t groups_y, uint32_t groups_z,
        const void *push_constants = nullptr,
        uint32_t push_constants_size = 0
    );
};
