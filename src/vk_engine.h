// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>

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

	bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);

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
	VkPipeline m_RedTrianglePipeline;


	int m_SelectedShader{ 0 };
};
