// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>

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

private:
	bool m_Isinitialized{ false };
	int m_FrameNumber {0};

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
};
