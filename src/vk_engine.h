// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>

#include "vk_mesh.h"

#include <glm/glm.hpp>

struct GPUCameraData
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewProj;
};

struct GPUSceneData
{
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; // x for min, y for max, zw unused
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

struct GPUObjectData
{
	glm::mat4 modelMatrix;
};

struct FrameData
{
	VkSemaphore presentSemaphore, renderSemaphore;
	VkFence renderFence;

	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	AllocatedBuffer cameraBuffer;

	VkDescriptorSet globalDescriptor;

	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptor;
};

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

constexpr uint32_t FRAME_OVERLAP = 2;

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
	void InitDescriptors();

	bool LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule);
	void UploadMesh(Mesh& mesh);

	Material* CreateMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	Material* GetMaterial(const std::string& name);
	Mesh* GetMesh(const std::string& name);

	void DrawObjects(VkCommandBuffer cmd, RenderObject* first, int count);

	FrameData& GetCurrentFrame();

	AllocatedBuffer CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	size_t PadUniformBufferSize(size_t originalSize);

private:
	bool m_Isinitialized{ false };
	int m_FrameNumber{ 0 };
	DeletionQueue m_MainDeletionQueue;

	VkExtent2D m_WindowExtent{ 1700 , 900 };

	struct SDL_Window* m_Window{ nullptr };

	VkInstance m_Instance;
	VkDebugUtilsMessengerEXT m_DebugMessenger;
	VkPhysicalDevice m_ChosenGpu;
	VkPhysicalDeviceProperties m_GpuProperties;
	VkDevice m_Device;
	VkSurfaceKHR m_Surface;

	VkSwapchainKHR m_Swapchain;
	VkFormat m_SwapchainImageFormat;
	std::vector<VkImage> m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;

	VkQueue m_GraphicsQueue;
	uint32_t m_GraphicsQueueFamily;

	FrameData m_Frames[FRAME_OVERLAP];

	VkRenderPass m_RenderPass;
	std::vector<VkFramebuffer> m_Framebuffers;

	VmaAllocator m_Allocator;

	Mesh m_TriangleMesh;
	Mesh m_MonkeyMesh;

	VkImageView m_DepthImageView;
	AllocatedImage m_DepthImage;

	VkFormat m_DepthFormat;

	std::vector<RenderObject> m_Renderables;

	std::unordered_map<std::string, Material> m_Materials;
	std::unordered_map<std::string, Mesh> m_Meshes;

	VkDescriptorSetLayout m_GlobalSetLayout;
	VkDescriptorSetLayout m_ObjectSetLayout;
	VkDescriptorPool m_DescriptorPool;

	GPUSceneData m_SceneParameters;
	AllocatedBuffer m_SceneParameterBuffer;
};
