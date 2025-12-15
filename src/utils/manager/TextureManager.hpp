#include "RTG.hpp"

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

class TextureManager {
    public:
        std::vector<  std::shared_ptr<Texture2DLoader::Texture> > textures;
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        std::vector< VkDescriptorSet > descriptor_sets;
        
        void create(RTG & rtg, S72Loader::Document &doc, std::vector<VkDescriptorSetLayout> texture_descriptor_layouts);
        void destroy(RTG &rtg);

        TextureManager() = default;
        ~TextureManager();
};