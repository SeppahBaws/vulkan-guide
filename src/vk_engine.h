// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>

#include "vk_mesh.h"

#include <glm/glm.hpp>

struct Material
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject
{
	Mesh* mesh;
	Material* material;
	glm::mat4 transformMatrix;
};

struct MeshPushConstants
{
	glm::vec4 data;
	glm::mat4 renderMatrix;
};

class PipelineBuilder
{
public:
	VkPipeline BuildPipeline(VkDevice device, VkRenderPass renderPass);

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void PushFunction(std::function<void()>&& function)
	{
		deletors.push_back(function);
	}

	void Flush()
	{
		for (auto it = deletors.begin(); it != deletors.end(); it++)
		{
			(*it)();
		}

		deletors.clear();
	}
};

class VulkanEngine
{
public:

	//initializes everything in the engine
	void Init();

	//shuts down the engine
	void Cleanup();

	//draw loop
	void Draw();

	//run main loop
	void Run();

private:
	void InitVulkan();
	void InitSwapchain();
	void InitCommands();
	void InitDefaultRenderpass();
	void InitFramebuffers();
	void InitSyncStructures();
	void InitPipelines();
	void LoadMeshes();
	void InitScene();

	bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);
	void UploadMesh(Mesh& mesh);

	Material* CreateMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	Material* GetMaterial(const std::string& name);
	Mesh* GetMesh(const std::string& name);

	void DrawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

private:
	bool m_Isinitialized{ false };
	int m_FrameNumber{ 0 };
	DeletionQueue m_MainDeletionQueue;

	VkExtent2D m_WindowExtent{ 1700 , 900 };

	struct SDL_Window* m_Window{ nullptr };

	VkInstance m_Instance;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice m_ChosenGpu;
	VkDevice m_Device;
	VkSurfaceKHR m_Surface;

	VkSwapchainKHR m_Swapchain;
	VkFormat m_SwapchainImageFormat;
	std::vector<VkImage> m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;

	VkQueue m_GraphicsQueue;
	uint32_t m_GraphicsQueueFamily;

	VkCommandPool m_CommandPool;
	VkCommandBuffer m_MainCommandBuffer;

	VkRenderPass m_RenderPass;
	std::vector<VkFramebuffer> m_Framebuffers;

	VkSemaphore m_PresentSemaphore, m_RenderSemaphore;
	VkFence m_RenderFence;

	VkPipelineLayout m_TrianglePipelineLayout;
	VkPipeline m_TrianglePipeline;

	VmaAllocator m_Allocator;

	VkPipeline m_MeshPipeline;
	Mesh m_TriangleMesh;
	VkPipelineLayout m_MeshPipelineLayout;

	Mesh m_MonkeyMesh;

	VkImageView m_DepthImageView;
	AllocatedImage m_DepthImage;

	VkFormat m_DepthFormat;

	std::vector<RenderObject> m_Renderables;

	std::unordered_map<std::string, Material> m_Materials;
	std::unordered_map<std::string, Mesh> m_Meshes;
};
