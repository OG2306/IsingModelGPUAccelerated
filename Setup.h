#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <iostream>

enum eComputeShaderType
{
	COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN,
	COMPUTE_SHADER_TYPE_1_INT_PER_SPIN
};

struct sVulkanBufferAndMore
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
	void* pVulkanBufferMemory = nullptr;
	VkDeviceSize memoryOffset = 0;
};

/* The Vulkan context */
struct sVulkanContext
{
	VkInstance instance                                    = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger                = VK_NULL_HANDLE;
	VkPhysicalDevice gpu                                   = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties gpuProperties;
	VkDevice device                                        = VK_NULL_HANDLE;
	VkQueue computeQueue                                   = VK_NULL_HANDLE;
	int computeQueueIndex                                  = -1;
	VkDescriptorSetLayout descriptorSetLayout              = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool                        = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet                          = VK_NULL_HANDLE;
	VkPipelineLayout computePipelineLayout                 = VK_NULL_HANDLE;
	VkPipeline computePipeline                             = VK_NULL_HANDLE;
	VkCommandPool commandPool					           = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer				           = VK_NULL_HANDLE;
	uint32_t localWorkGroupSizeInX			               = 1;
	uint32_t maxWorkGroupCountPerDispatchInX               = 1;

	sVulkanBufferAndMore bigDeviceLocalBufferAndMore;
	VkDeviceSize bigDeviceLocalBufferBytesLeft = 0;
	VkDeviceSize bigDeviceLocalBufferNextAvailableByte = 0;
	VkDeviceSize bigDeviceLocalBufferByteAlignment = 0;
	VkBufferUsageFlags bigDeviceLocalBufferUsageFlags = 0;

	sVulkanBufferAndMore bigHostVisibleVulkanBufferAndMore;
	VkDeviceSize bigHostVisibleVulkanBufferBytesLeft = 0;
	VkDeviceSize bigHostVisibleVulkanBufferByteAlignment = 0;
	VkDeviceSize bigHostVisibleVulkanBufferNextAvailableByte = 0;
	VkBufferUsageFlags bigHostVisibleVulkanBufferUsageFlags = 0;

	VkBuffer persistentStagingBuffer = VK_NULL_HANDLE;
	VkDeviceSize persistentStagingBufferByteOffsetIntoTheBigHostVisibleBuffer = 0;
	VkDeviceSize persistentStagingBufferByteSize = 0;

	VkBuffer SSBSpinBuffer = VK_NULL_HANDLE;
	VkDeviceSize SSBSpinBufferByteOffsetIntoTheBigDeviceLocalBuffer = 0;
	VkDeviceSize SSBSpinBufferByteSize = 0;

	VkBuffer SSBRandomNumbersBuffer = VK_NULL_HANDLE;
	VkDeviceSize SSBRandomNumbersBufferByteOffsetIntoTheBigDeviceLocalBuffer = 0;
	VkDeviceSize SSBRandomNumbersBufferByteSize = 0;

	VkBuffer SSBSpinSumBuffer = VK_NULL_HANDLE;
	VkDeviceSize SSBSpinSumBufferByteOffsetIntoTheBigDeviceLocalBuffer = 0;
	VkDeviceSize SSBSpinSumBufferByteSize = 0;

	VkBuffer uniformBuffer = VK_NULL_HANDLE;
	VkDeviceSize uniformBufferByteOffsetIntoTheBigHostVisibleBuffer = 0;
	VkDeviceSize uniformBufferByteSize = 0;

	VkBuffer spinSumOutputBuffer = VK_NULL_HANDLE;
	VkDeviceSize spinSumOutputBufferByteOffsetIntoTheBigHostVisibleBuffer = 0;
	VkDeviceSize spinSumOutputBufferByteSize = 0;

	VkBuffer SSBSpinBatchesBuffer = VK_NULL_HANDLE;
	VkDeviceSize SSBSpinBatchesBufferByteOffsetIntoTheBigDeviceLocalBuffer = 0;
	VkDeviceSize SSBSpinBatchesBufferByteSize = 0;
};

/* The uniform buffer object */
struct sUniformBufferObject
{
	uint32_t transitionProbability4;				// The transition probability if a spin flip gives +4 energy
	uint32_t transitionProbability8;				// The transition probability if a spin flip gives +8 energy
	uint32_t isingL;
	uint32_t isingN;
};

/* Push constants */
struct sPushConstantObject
{
	uint32_t phase = 0;
};

/* The setup class */
class cSetup
{
	friend uint32_t FindVulkanMemoryType(sVulkanContext& vulkanContext, uint32_t memory_type_bits, VkMemoryPropertyFlags memory_property_flags);
	friend VkShaderModule LoadShaderModule(sVulkanContext& vulkanContext, const char* path);
	friend void CreateAndBeginOneTimeVulkanCommandBuffer(sVulkanContext& vulkanContext, VkCommandPool& commandPool, VkCommandBuffer& commandBuffer);
	friend void CreateVulkanBufferAndMemoryAndBindBuffer(sVulkanContext& vulkanContext, VkDeviceMemory& bufferMemory, VkBuffer& buffer, VkDeviceSize bufferSize,
		VkBufferUsageFlags bufferUsage, VkSharingMode sharingMode, uint32_t queueFamilyIndexCount,
		uint32_t* pQueueFamilyIndices, VkMemoryPropertyFlags requiredMemoryProperties);
	// Dispatch work to the GPU
	friend void DoTheIsingGridSweepsGPU(cSetup* pTheSetup, const uint32_t isingL, const double beta,
		const uint32_t numberOfSweepsPerTemperature, const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts,
		const uint32_t sweepsPerSpinSumSample);
	// Collect work from the GPU
	friend double CalculateBinderCumulantGPU(cSetup* pTheSetup, const uint32_t isingL);

private:
	sVulkanContext context;

	// Init the Vulkan instance
	void PrepareVulkanInstance(const std::vector<const char*>& requiredInstanceExtensions, const std::vector<const char*>& requiredValidationLayers);
	// Find a physical GPU and init the logical device
	void PrepareVulkanDevice(const std::vector<const char*> requiredDeviceExtensions);
	// Prepare a big device local buffer used for suballoction
	void PrepareBigDeviceLocalVulkanBufferAndMore(VkDeviceSize bufferByteSize);
	// Suballocate from the big device local buffer
	VkBuffer SuballocateBufferFromTheBigDeviceLocalVulkanBuffer(VkBufferUsageFlags bufferUsage, VkDeviceSize bufferByteSize, VkDeviceSize& memoryByteOffset);
	// Prepare a big host visible buffer used for suballocation
	void PrepareBigHostVisibleVulkanBufferAndMore(VkDeviceSize bufferByteSize);
	// Suballocate from the big host visible buffer
	VkBuffer SuballocateBufferFromTheBigHostVisibleVulkanBuffer(VkBufferUsageFlags bufferUsage, VkDeviceSize bufferByteSize, VkDeviceSize& memoryByteOffset);
	// Init the spin buffer (used with the compute shader of type COMPUTE_SHADER_TYPE_1_SPIN_PER_UINT)
	void PrepareVulkanSSBSpinBuffer(const uint32_t isingL);
	// Init the spin batches buffer (used with the compute shader of type COMPUTE_SHADER_TYPE_32_SPINs_PER_UINT)
	void PrepareVulkanSSBSpinBatchesBuffer(const uint32_t isingL);
	// Init a shader storage buffer with random numbers
	void PrepareVulkanSSBRandomNumbersBuffer(const uint32_t isingL);
	// Init the shader storage buffer where the spin sum will be kept
	void PrepareVulkanSSBSpinSumBuffer(const uint32_t ising_L);
	// Init the output buffer that the sampled spin sums will be written to
	void PrepareVulkanSpinSumOutputBuffer(const uint32_t numberOfSweepsPerTemperature,
		const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample);
	// Init the descriptor set
	void PrepareDescriptorSet(const uint32_t ising_L, eComputeShaderType computeShaderType);
	// Init the compute pipeline
	void PrepareComputePipeline(eComputeShaderType computeShaderType);
	// Init command pool and buffer
	void PrepareCommandPoolAndCommandBuffer();
	// Destroy a Vulkan buffer and more
	void DestroyVulkanBufferAndMore(sVulkanBufferAndMore& bufferAndMore);

public:
	cSetup(const uint32_t isingL, const uint32_t numberOfSweepsPerTemperature,
		const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample, eComputeShaderType computeShaderType);

	~cSetup();

	void WriteToUniformBufferAndUpdateDescriptorSet(const double beta, const uint32_t isingL);
};

uint32_t XORShift(uint32_t rngState);

void DoTheIsingGridSweepsCPU(uint32_t* pArraySpinBatches, int* pArraySpinSumOutputs, int& TheSpinSum, const uint32_t isingL,
	const double beta, const uint32_t numberOfSweepsPerTemperature, const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample);

double CalculateBinderCumulantCPU(int* pArraySpinSumOutputs, const uint32_t isingL, const uint32_t numberOfElementsInTheSpinSumOutputArray);