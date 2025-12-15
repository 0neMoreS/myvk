#include "RTG.hpp"

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

class TextureManager {
    public:
        // textures grouped by logical slot; index corresponds to material index
        std::array< std::vector< std::shared_ptr<Texture2DLoader::Texture> >, 4 > textures_by_slot{};

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        // descriptor_sets[layout_index][material_index]
        std::vector< std::vector< VkDescriptorSet > > descriptor_sets;
        
        void create(
            RTG & rtg,
            std::shared_ptr<S72Loader::Document> &doc,
            const std::vector<Pipeline::TextureDescriptorConfig> &texture_descriptor_configs
        );
        void destroy(RTG &rtg);

        TextureManager() = default;
        ~TextureManager();
};