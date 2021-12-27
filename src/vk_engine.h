// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

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
};
