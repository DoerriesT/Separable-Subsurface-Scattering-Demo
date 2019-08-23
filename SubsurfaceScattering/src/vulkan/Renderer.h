#pragma once
#include "VKContext.h"
#include "SwapChain.h"
#include <glm/mat4x4.hpp>
#include <memory>
#include "Material.h"
#include "RenderResources.h"

namespace sss
{
	namespace vulkan
	{
		class Texture;
		class Mesh;

		class Renderer
		{
		public:
			explicit Renderer(void *windowHandle, uint32_t width, uint32_t height);
			~Renderer();
			void render(const glm::mat4 &viewProjection, 
				const glm::mat4 &shadowMatrix, 
				const glm::vec4 &lightPositionRadius, 
				const glm::vec4 &lightColorInvSqrAttRadius, 
				const glm::vec4 &cameraPosition, 
				bool subsurfaceScatteringEnabled);
			float getSSSEffectTiming() const;
			void resize(uint32_t width, uint32_t height);

		private:
			float m_sssTime;
			uint32_t m_width;
			uint32_t m_height;
			uint64_t m_frameIndex = 0;
			VKContext m_context;
			SwapChain m_swapChain;
			RenderResources m_renderResources;
			std::shared_ptr<Texture> m_radianceTexture;
			std::shared_ptr<Texture> m_irradianceTexture;
			std::shared_ptr<Texture> m_brdfLUT;
			std::shared_ptr<Texture> m_skyboxTexture;
			std::vector<std::shared_ptr<Texture>> m_textures;
			std::vector<std::shared_ptr<Mesh>> m_meshes;
			std::vector<std::pair<Material, bool>> m_materials; // bool is true if SSS
		};
	}
}