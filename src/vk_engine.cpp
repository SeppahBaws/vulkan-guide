
#include "vk_engine.h"

#include <fstream>
#include <array>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <glm/gtc/matrix_transform.hpp>

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

VkPipeline PipelineBuilder::BuildPipeline(VkDevice device, VkRenderPass renderPass)
{
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;

	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.pNext = nullptr;

	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;

	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	pipelineInfo.pDepthStencilState = &depthStencil;

	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline" << std::endl;
		return VK_NULL_HANDLE;
	}

	return newPipeline;
}

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
	InitPipelines();
	LoadMeshes();
	InitScene();

	//everything went fine
	m_Isinitialized = true;
}
void VulkanEngine::Cleanup()
{
	if (m_Isinitialized)
	{
		vkWaitForFences(m_Device, 1, &m_RenderFence, true, 1'000'000'000);

		m_MainDeletionQueue.Flush();

		vmaDestroyAllocator(m_Allocator);

		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		vkDestroyDevice(m_Device, nullptr);
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

	VkClearValue colorClear;
	float flash = abs(sin(m_FrameNumber / 120.0f));
	colorClear.color = { { 0.0f, 0.0f, flash, 1.0f } };

	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.0f;

	std::array<VkClearValue, 2> clearValues = { colorClear, depthClear };

	VkRenderPassBeginInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpInfo.pNext = nullptr;
	rpInfo.renderPass = m_RenderPass;
	rpInfo.renderArea.offset = VkOffset2D{ 0, 0 };
	rpInfo.renderArea.extent = m_WindowExtent;
	rpInfo.framebuffer = m_Framebuffers[swapchainImageIndex];
	rpInfo.clearValueCount = clearValues.size();
	rpInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	DrawObjects(cmd, m_Renderables.data(), m_Renderables.size());

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
			else if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_SPACE)
				{
					m_SelectedShader += 1;
					if (m_SelectedShader > 1)
					{
						m_SelectedShader = 0;
					}
				}
			}
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

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = m_ChosenGpu;
	allocatorInfo.device = m_Device;
	allocatorInfo.instance = m_Instance;
	vmaCreateAllocator(&allocatorInfo, &m_Allocator);
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

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
	});

	VkExtent3D depthImageExtent = {
		m_WindowExtent.width,
		m_WindowExtent.height,
		1
	};

	m_DepthFormat = VK_FORMAT_D32_SFLOAT;

	VkImageCreateInfo depthImgInfo = VkInit::ImageCreateInfo(m_DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

	VmaAllocationCreateInfo depthImgAllocInfo = {};
	depthImgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	depthImgAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_Allocator, &depthImgInfo, &depthImgAllocInfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);

	VkImageViewCreateInfo depthViewInfo = VkInit::ImageViewCreateInfo(m_DepthFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(m_Device, &depthViewInfo, nullptr, &m_DepthImageView));

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
		vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
	});
}

void VulkanEngine::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = VkInit::CommandPoolCreateInfo(m_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_CommandPool));

	VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::CommandBufferAllocateInfo(m_CommandPool);
	VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_MainCommandBuffer));

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
	});
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

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.flags = 0;
	depthAttachment.format = m_DepthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass));

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
	});
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

	const uint32_t swapchainImageCount = static_cast<uint32_t>(m_SwapchainImages.size());
	m_Framebuffers = std::vector<VkFramebuffer>(swapchainImageCount);

	for (size_t i = 0; i < swapchainImageCount; i++)
	{
		std::array<VkImageView, 2> attachments = { m_SwapchainImageViews[i], m_DepthImageView };

		fbInfo.attachmentCount = attachments.size();
		fbInfo.pAttachments = attachments.data();

		VK_CHECK(vkCreateFramebuffer(m_Device, &fbInfo, nullptr, &m_Framebuffers[i]));

		m_MainDeletionQueue.PushFunction([=]()
		{
			vkDestroyFramebuffer(m_Device, m_Framebuffers[i], nullptr);
			vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
		});
	}
}

void VulkanEngine::InitSyncStructures()
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_RenderFence));

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyFence(m_Device, m_RenderFence, nullptr);
	});

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_PresentSemaphore));
	VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_RenderSemaphore));

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroySemaphore(m_Device, m_PresentSemaphore, nullptr);
		vkDestroySemaphore(m_Device, m_RenderSemaphore, nullptr);
	});
}

void VulkanEngine::InitPipelines()
{
	VkShaderModule triangleFragShader;
	if (!LoadShaderModule("../../shaders/colored_triangle.frag.spv", &triangleFragShader))
	{
		std::cout << "Error when building the colored_triangle fragment shader module" << std::endl;
	}
	else
	{
		std::cout << "colored_triangle fragment shader successfully loaded" << std::endl;
	}

	VkShaderModule triangleVertexShader;
	if (!LoadShaderModule("../../shaders/colored_triangle.vert.spv", &triangleVertexShader))
	{
		std::cout << "Error when building the colored_triangle vertex shader module" << std::endl;
	}
	else
	{
		std::cout << "colored_triangle vertex shader successfully loaded" << std::endl;
	}

	VkShaderModule redTriangleFragShader;
	if (!LoadShaderModule("../../shaders/triangle.frag.spv", &redTriangleFragShader))
	{
		std::cout << "Error when building the red triangle fragment shader module" << std::endl;
	}
	else
	{
		std::cout << "red triangle fragment shader successfully loaded" << std::endl;
	}

	VkShaderModule redTriangleVertexShader;
	if (!LoadShaderModule("../../shaders/triangle.vert.spv", &redTriangleVertexShader))
	{
		std::cout << "Error when building the red triangle vertex shader module" << std::endl;
	}
	else
	{
		std::cout << "red triangle vertex shader successfully loaded" << std::endl;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::PipelineLayoutCreateInfo();
	VK_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_TrianglePipelineLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.shaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader));
	pipelineBuilder.shaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

	pipelineBuilder.vertexInputInfo = VkInit::VertexInputStateCreateInfo();

	pipelineBuilder.inputAssembly = VkInit::InputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	pipelineBuilder.viewport.x = 0.0f;
	pipelineBuilder.viewport.y = 0.0f;
	pipelineBuilder.viewport.width = static_cast<float>(m_WindowExtent.width);
	pipelineBuilder.viewport.height = static_cast<float>(m_WindowExtent.height);
	pipelineBuilder.viewport.minDepth = 0.0f;
	pipelineBuilder.viewport.maxDepth = 1.0f;

	pipelineBuilder.scissor.offset = { 0, 0 };
	pipelineBuilder.scissor.extent = m_WindowExtent;

	pipelineBuilder.rasterizer = VkInit::RasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

	pipelineBuilder.multisampling = VkInit::MultisampleStateCreateInfo();
	pipelineBuilder.colorBlendAttachment = VkInit::ColorBlendAttachmentState();
	pipelineBuilder.pipelineLayout = m_TrianglePipelineLayout;
	pipelineBuilder.depthStencil = VkInit::DepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	m_TrianglePipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

	pipelineBuilder.shaderStages.clear();

	pipelineBuilder.shaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, redTriangleVertexShader));
	pipelineBuilder.shaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, redTriangleFragShader));

	m_RedTrianglePipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

	
	VertexInputDescription vertexDescription = Vertex::GetVertexDescrption();

	pipelineBuilder.vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder.vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

	pipelineBuilder.vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder.vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	pipelineBuilder.shaderStages.clear();


	VkShaderModule meshVertShader;
	if (!LoadShaderModule("../../shaders/tri_mesh.vert.spv", &meshVertShader))
	{
		std::cout << "Error when building the triangle mesh vertex shader module" << std::endl;
	}
	else
	{
		std::cout << "Triangle mesh vertex shader successfully loaded" << std::endl;
	}

	pipelineBuilder.shaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
	pipelineBuilder.shaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

	m_MeshPipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);


	VkPipelineLayoutCreateInfo meshPipelineLayoutInfo = VkInit::PipelineLayoutCreateInfo();

	VkPushConstantRange pushConstant;
	pushConstant.offset = 0;
	pushConstant.size = sizeof(MeshPushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	meshPipelineLayoutInfo.pPushConstantRanges = &pushConstant;
	meshPipelineLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_Device, &meshPipelineLayoutInfo, nullptr, &m_MeshPipelineLayout));

	pipelineBuilder.pipelineLayout = m_MeshPipelineLayout;
	m_MeshPipeline = pipelineBuilder.BuildPipeline(m_Device, m_RenderPass);

	CreateMaterial(m_MeshPipeline, m_MeshPipelineLayout, "defaultMesh");

	
	vkDestroyShaderModule(m_Device, meshVertShader, nullptr);
	vkDestroyShaderModule(m_Device, redTriangleVertexShader, nullptr);
	vkDestroyShaderModule(m_Device, redTriangleFragShader, nullptr);
	vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);
	vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);

	m_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyPipeline(m_Device, m_RedTrianglePipeline, nullptr);
		vkDestroyPipeline(m_Device, m_TrianglePipeline, nullptr);
		vkDestroyPipeline(m_Device, m_MeshPipeline, nullptr);

		vkDestroyPipelineLayout(m_Device, m_TrianglePipelineLayout, nullptr);
		vkDestroyPipelineLayout(m_Device, m_MeshPipelineLayout, nullptr);
	});
}

void VulkanEngine::LoadMeshes()
{
	m_TriangleMesh.vertices.resize(3);

	m_TriangleMesh.vertices[0].position = { 1.0f, 1.0f, 0.0f };
	m_TriangleMesh.vertices[1].position = { -1.0f, 1.0f, 0.0f };
	m_TriangleMesh.vertices[2].position = { 0.0f, -1.0f, 0.0f };

	m_TriangleMesh.vertices[0].color = {0.0f, 1.0f, 0.0f };
	m_TriangleMesh.vertices[1].color = {0.0f, 1.0f, 0.0f };
	m_TriangleMesh.vertices[2].color = {0.0f, 1.0f, 0.0f };

	m_MonkeyMesh.LoadFromObj("../../assets/monkey_smooth.obj");

	UploadMesh(m_TriangleMesh);
	UploadMesh(m_MonkeyMesh);

	m_Meshes["monkey"] = m_MonkeyMesh;
	m_Meshes["triangle"] = m_TriangleMesh;
}

void VulkanEngine::InitScene()
{
	RenderObject monkey;
	monkey.mesh = GetMesh("monkey");
	monkey.material = GetMaterial("defaultMesh");
	monkey.transformMatrix = glm::mat4{ 1.0f };

	m_Renderables.push_back(monkey);

	for (int x = -20; x <= 20; x++)
	{
		for (int y = -20; y <= 20; y++)
		{
			RenderObject tri;
			tri.mesh = GetMesh("triangle");
			tri.material = GetMaterial("defaultMesh");
			glm::mat4 translation = glm::translate(glm::mat4{ 1.0f }, glm::vec3{ x, 0, y });
			glm::mat4 scale = glm::scale(glm::mat4{ 1.0f }, glm::vec3{ 0.2f, 0.2f, 0.2f });
			tri.transformMatrix = translation * scale;

			m_Renderables.push_back(tri);
		}
	}
}

bool VulkanEngine::LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return false;
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
	file.close();

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		return false;
	}

	*outShaderModule = shaderModule;
	return true;
}

void VulkanEngine::UploadMesh(Mesh& mesh)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = mesh.vertices.size() * sizeof(Vertex);
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaAllocInfo, &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

	m_MainDeletionQueue.PushFunction([=]()
	{
		vmaDestroyBuffer(m_Allocator, mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation);
	});

	void* data;
	vmaMapMemory(m_Allocator, mesh.vertexBuffer.allocation, &data);

	memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

	vmaUnmapMemory(m_Allocator, mesh.vertexBuffer.allocation);
}

Material* VulkanEngine::CreateMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	Material mat;
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
	m_Materials[name] = mat;
	return &m_Materials[name];
}

Material* VulkanEngine::GetMaterial(const std::string& name)
{
	auto it = m_Materials.find(name);
	if (it == m_Materials.end())
		return nullptr;

	return &(*it).second;
}

Mesh* VulkanEngine::GetMesh(const std::string& name)
{
	auto it = m_Meshes.find(name);
	if (it == m_Meshes.end())
		return nullptr;

	return &(*it).second;
}

void VulkanEngine::DrawObjects(VkCommandBuffer cmd, RenderObject* first, int count)
{
	glm::vec3 camPos{ 0.0f, -6.0f, -10.0f };
	glm::mat4 view = glm::translate(glm::mat4{ 1.0f }, camPos);
	glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f);
	projection[1][1] *= -1;

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < count; i++)
	{
		RenderObject& object = first[i];

		if (object.material != lastMaterial)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;
		}

		glm::mat4 model = object.transformMatrix;
		glm::mat4 meshMatrix = projection * view * model;

		MeshPushConstants constants;
		constants.renderMatrix = meshMatrix;

		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		if (object.mesh != lastMesh)
		{
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->vertexBuffer.buffer, &offset);
			lastMesh = object.mesh;
		}
		vkCmdDraw(cmd, object.mesh->vertices.size(), 1, 0, 0);
	}
}
