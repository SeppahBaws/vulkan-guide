
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"

#include <iostream>
#define VK_CHECK(x)														\
	do																	\
	{																	\
		VkResult err = x;												\
		if (err)														\
		{																\
			std::cout << "Detected Vulkan error: " << err << std::endl;	\
			abort();													\
		}																\
	} while (0)

void VulkanEngine::Init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	m_Window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_WindowExtent.width,
		m_WindowExtent.height,
		window_flags
	);

	InitVulkan();
	InitSwapchain();
	InitCommands();
	InitDefaultRenderpass();
	InitFramebuffers();
	InitSyncStructures();

	//everything went fine
	m_Isinitialized = true;
}
void VulkanEngine::Cleanup()
{
	if (m_Isinitialized)
	{
		VK_CHECK(vkDeviceWaitIdle(m_Device));

		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

		vkDestroyFence(m_Device, m_RenderFence, nullptr);
		vkDestroySemaphore(m_Device, m_RenderSemaphore, nullptr);
		vkDestroySemaphore(m_Device, m_PresentSemaphore, nullptr);

		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

		// Destroy swapchain resources
		for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
		{
			vkDestroyFramebuffer(m_Device, m_Framebuffers[i], nullptr);
			vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
		}

		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);
		vkDestroyInstance(m_Instance, nullptr);
		SDL_DestroyWindow(m_Window);
	}
}

void VulkanEngine::Draw()
{
	// timeout of 1 second, in nanoseconds
	constexpr uint64_t timeout = 1'000'000'000;

	VK_CHECK(vkWaitForFences(m_Device, 1, &m_RenderFence, true, timeout));
	VK_CHECK(vkResetFences(m_Device, 1, &m_RenderFence));

	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, timeout, m_PresentSemaphore, nullptr, &swapchainImageIndex));

	VK_CHECK(vkResetCommandBuffer(m_MainCommandBuffer, 0));

	VkCommandBuffer cmd = m_MainCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	VkClearValue clearValue;
	float flash = abs(sin(m_FrameNumber / 120.0f));
	clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

	VkRenderPassBeginInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.pNext = nullptr;
	rpInfo.renderPass = m_RenderPass;
	rpInfo.renderArea.offset = VkOffset2D{ 0, 0 };
	rpInfo.renderArea.extent = m_WindowExtent;
	rpInfo.framebuffer = m_Framebuffers[swapchainImageIndex];
	rpInfo.clearValueCount = 1;
	rpInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdEndRenderPass(cmd);
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &m_PresentSemaphore;
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &m_RenderSemaphore;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submit, m_RenderFence));

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderSemaphore;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

	m_FrameNumber++;
}

void VulkanEngine::Run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
		}

		Draw();
	}
}

void VulkanEngine::InitVulkan()
{
	vkb::InstanceBuilder builder;

	auto instRet = builder
		.set_app_name("Example Vulkan Application")
		.request_validation_layers(true)
		.require_api_version(1, 2, 0)
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkbInst = instRet.value();

	m_Instance = vkbInst.instance;
	m_DebugMessenger = vkbInst.debug_messenger;

	SDL_Vulkan_CreateSurface(m_Window, m_Instance, &m_Surface);

	vkb::PhysicalDeviceSelector selector{ vkbInst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
		.set_surface(m_Surface)
		.select()
		.value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	vkb::Device vkbDevice = deviceBuilder.build().value();

	m_Device = vkbDevice.device;
	m_ChosenGpu = physicalDevice.physical_device;

	m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::InitSwapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{ m_ChosenGpu, m_Device, m_Surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(m_WindowExtent.width, m_WindowExtent.height)
		.build()
		.value();

	m_Swapchain = vkbSwapchain.swapchain;
	m_SwapchainImages = vkbSwapchain.get_images().value();
	m_SwapchainImageViews = vkbSwapchain.get_image_views().value();
	m_SwapchainImageFormat = vkbSwapchain.image_format;
}

void VulkanEngine::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = VkInit::CommandPoolCreateInfo(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool));

	VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::CommandBufferAllocateInfo(m_CommandPool);
	VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_MainCommandBuffer));
}

void VulkanEngine::InitDefaultRenderpass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_SwapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass));
}

void VulkanEngine::InitFramebuffers()
{
	VkFramebufferCreateInfo fbInfo = {};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.pNext = nullptr;
	fbInfo.renderPass = m_RenderPass;
	fbInfo.attachmentCount = 1;
	fbInfo.width = m_WindowExtent.width;
	fbInfo.height = m_WindowExtent.height;
	fbInfo.layers = 1;

	const uint32_t swapchainImageCount = m_SwapchainImages.size();
	m_Framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

	for (size_t i = 0; i < swapchainImageCount; i++)
	{
		fbInfo.pAttachments = &m_SwapchainImageViews[i];
		VK_CHECK(vkCreateFramebuffer(m_Device, &fbInfo, nullptr, &m_Framebuffers[i]));
	}
}

void VulkanEngine::InitSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_RenderFence));

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_PresentSemaphore));
	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_RenderSemaphore));
}
