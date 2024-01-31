#include "Setup.h"
#include <vulkan/vulkan.h>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>
#include <array>
#include <time.h>
#include <cassert>
#include <cmath>
#include <chrono>
#include <random>
#include <limits>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdlib>

//#define VKB_VALIDATION_LAYERS

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			throw std::runtime_error("Error detected!");            \
		}                                                           \
	} while (0)

/**********************************************************************/

// Simple logger
void Log(const std::string & msg)
{
	static bool first_write = true;
	std::filebuf fb;

	// Delete content from previous file
	if (first_write)
	{
		fb.open("Log.txt", std::ios_base::out);
		std::ostream os(&fb);
		os << msg << '\n';
		fb.close();
		first_write = false;
	}
	else
	{
		fb.open("Log.txt", std::ios_base::out | std::ios_base::app);
		std::ostream os(&fb);
		os << msg << '\n';
		fb.close();
	}
}

/**********************************************************************/

#if defined(VKB_VALIDATION_LAYERS)
// A Vulkan validation layer debug callback function
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		std::string errorMessage = "\nValidation layer error: ";
		errorMessage.append(callbackData->pMessage);
		Log(errorMessage);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		std::string warningMessage = "\nValidation layer warning: ";
		warningMessage.append(callbackData->pMessage);
		Log(warningMessage);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		std::string infoMessage = "\nValidation layer info: ";
		infoMessage.append(callbackData->pMessage);
		Log(infoMessage);
	}
	else
	{
		std::string otherMessage = "\nValidation layer other: ";
		otherMessage.append(callbackData->pMessage);
		Log(otherMessage);
	}

	return VK_FALSE;
}
#endif

/**********************************************************************/

bool ValidateVulkanExtensions(const std::vector<const char*>& requiredVulkanExtensionNames, const std::vector<VkExtensionProperties>& availableVulkanExtensionProperties)
{
	for (const char* extensionName : requiredVulkanExtensionNames)
	{
		bool bExtensionNameFound = false;

		for (const VkExtensionProperties& availableExtensionProperty : availableVulkanExtensionProperties)
		{
			if (std::strcmp(extensionName, availableExtensionProperty.extensionName) == 0) {
				bExtensionNameFound = true;
				break;
			}
		}

		if (!bExtensionNameFound)
		{
			return false;
		}
	}
	return true;
}

/**********************************************************************/

bool ValidateVulkanLayers(const std::vector<const char*>& requiredVulkanValidationLayers, const std::vector<VkLayerProperties>& availableVulkanValidationLayerProperties)
{
	for (const char* requiredLayer : requiredVulkanValidationLayers)
	{
		bool bValidationLayerFound = false;

		for (const VkLayerProperties& availableValidationLayerProperty : availableVulkanValidationLayerProperties)
		{
			if (std::strcmp(requiredLayer, availableValidationLayerProperty.layerName) == 0) {
				bValidationLayerFound = true;
				break;
			}
		}

		if (!bValidationLayerFound)
		{
			return false;
		}
	}
	return true;
}

/**********************************************************************/

uint32_t FindVulkanMemoryType(sVulkanContext& vulkanContext, uint32_t availableMemoryTypeBits, VkMemoryPropertyFlags requiredMemoryProperties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(vulkanContext.gpu, &memoryProperties);
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		// If bit number i is 1 in 'availableMemoryTypeBits' it means that the memory type
		// 'memoryProperties.memoryTypes[i]' is available for the resource
		if (availableMemoryTypeBits & (1 << i))
		{
			// We then check to see if the memory type has the desired properties
			if ((memoryProperties.memoryTypes[i].propertyFlags & requiredMemoryProperties) == requiredMemoryProperties)
			{
				return i;
			}
		}
	}

	throw std::runtime_error("Failed to find suitable memory type index!");
}

/**********************************************************************/

VkShaderModule LoadShaderModule(sVulkanContext& vulkanContext, const char* const spvFilename)
{
	// Read the file in binary starting from the end
	std::ifstream infile(spvFilename, std::ios_base::binary | std::ios_base::ate);
	if (!infile.is_open())
	{
		throw std::runtime_error("Failed to load SPIR-V file!");
	}

	// Get the filesize in bytes
	size_t fileSize = infile.tellg();

	std::vector<char> fileBuffer(fileSize);
	infile.seekg(0);
	infile.read(fileBuffer.data(), fileSize);

	const VkShaderModuleCreateInfo shaderModuleCI =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		fileBuffer.size(),
		reinterpret_cast<uint32_t*>(fileBuffer.data())
	};

	VkShaderModule shaderModule;
	VK_CHECK(vkCreateShaderModule(vulkanContext.device, &shaderModuleCI, nullptr, &shaderModule));

	infile.close();
	return shaderModule;
}

/**********************************************************************/

void CreateAndBeginOneTimeVulkanCommandBuffer(sVulkanContext& vulkanContext, VkCommandPool& commandPool, VkCommandBuffer& commandBuffer)
{
	// Create the command pool
	const VkCommandPoolCreateInfo commandPoolCI =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		static_cast<uint32_t>(vulkanContext.computeQueueIndex)
	};

	VK_CHECK(vkCreateCommandPool(vulkanContext.device, &commandPoolCI, nullptr, &commandPool));

	// Create the command buffer
	const VkCommandBufferAllocateInfo commandBufferAllocateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		commandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};

	VK_CHECK(vkAllocateCommandBuffers(vulkanContext.device, &commandBufferAllocateInfo, &commandBuffer));

	// Begin the command buffer
	const VkCommandBufferBeginInfo commandBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		nullptr
	};

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));
}

/**********************************************************************/

void CreateVulkanBufferAndMemoryAndBindBuffer(sVulkanContext& vulkanContext, VkDeviceMemory& bufferMemory, VkBuffer& buffer, VkDeviceSize bufferSize,
	VkBufferUsageFlags bufferUsage, VkSharingMode sharingMode, uint32_t queueFamilyIndexCount,
	uint32_t* pQueueFamilyIndices, VkMemoryPropertyFlags requiredMemoryProperties)
{
	// Buffer
	const VkBufferCreateInfo bufferCI =
	{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		nullptr,
		0,
		bufferSize,
		bufferUsage,
		sharingMode,
		queueFamilyIndexCount,
		pQueueFamilyIndices
	};

	VK_CHECK(vkCreateBuffer(vulkanContext.device, &bufferCI, nullptr, &buffer));

	// Memory
	VkMemoryRequirements bufferMemoryRequirements;
	vkGetBufferMemoryRequirements(vulkanContext.device, buffer, &bufferMemoryRequirements);

	const VkMemoryAllocateInfo bufferMemoryAllocateInfo =
	{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr,
		bufferMemoryRequirements.size,
		FindVulkanMemoryType(vulkanContext, bufferMemoryRequirements.memoryTypeBits, requiredMemoryProperties)
	};

	VK_CHECK(vkAllocateMemory(vulkanContext.device, &bufferMemoryAllocateInfo, nullptr, &bufferMemory));
	VK_CHECK(vkBindBufferMemory(vulkanContext.device, buffer, bufferMemory, 0));
}

/**********************************************************************/

void cSetup::DestroyVulkanBufferAndMore(sVulkanBufferAndMore& bufferAndMore)
{
	vkQueueWaitIdle(context.computeQueue);
	if (bufferAndMore.pVulkanBufferMemory != nullptr)
	{
		vkUnmapMemory(context.device, bufferAndMore.bufferMemory);
		bufferAndMore.pVulkanBufferMemory = nullptr;
	}
	if (bufferAndMore.buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, bufferAndMore.buffer, nullptr);
		bufferAndMore.buffer = VK_NULL_HANDLE;
	}
	if (bufferAndMore.bufferMemory != VK_NULL_HANDLE)
	{
		vkFreeMemory(context.device, bufferAndMore.bufferMemory, nullptr);
		bufferAndMore.bufferMemory = VK_NULL_HANDLE;
	}
}

/**********************************************************************/

void cSetup::PrepareVulkanInstance(const std::vector<const char*>& requiredInstanceExtensions, const std::vector<const char*>& requiredValidationLayers)
{
	Log(""); // Start logging

	// ---- Validate instance extensions ----
	uint32_t numberOfAvailableInstanceExtensions = 0;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &numberOfAvailableInstanceExtensions, nullptr));

	std::vector<VkExtensionProperties> availableInstanceExtensionsProperties(numberOfAvailableInstanceExtensions);
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &numberOfAvailableInstanceExtensions, availableInstanceExtensionsProperties.data()));

	if (!ValidateVulkanExtensions(requiredInstanceExtensions, availableInstanceExtensionsProperties))
	{
		throw std::runtime_error("Failed to find required instance extensions!");
	}

	std::vector<const char*> activeInstanceExtensions(requiredInstanceExtensions);

#if defined(VKB_VALIDATION_LAYERS)
	activeInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // Allows for enabling of validation layers
#endif

	// ---- Validate validation layers ----
	uint32_t numberOfAvailableValidationLayers = 0;
	std::vector<const char*> activeValidationLayers(0);

#if defined(VKB_VALIDATION_LAYERS)
	VK_CHECK(vkEnumerateInstanceLayerProperties(&numberOfAvailableValidationLayers, nullptr));
	std::vector<VkLayerProperties> availableValidationLayerProperties(numberOfAvailableValidationLayers);
	VK_CHECK(vkEnumerateInstanceLayerProperties(&numberOfAvailableValidationLayers, availableValidationLayerProperties.data()));

	if (!ValidateVulkanLayers(requiredValidationLayers, availableValidationLayerProperties))
	{
		throw std::runtime_error("Failed to find required layer properties!");
	}
	activeValidationLayers = requiredValidationLayers;
#endif

	// ---- Instance create info ----
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	VkValidationFeaturesEXT validationFeaturesInfo{ VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };

#if defined(VKB_VALIDATION_LAYERS)
	debugMessengerCI =
	{
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		nullptr,
		0,
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		VulkanDebugCallback,
		nullptr
	};

	std::array<VkValidationFeatureEnableEXT, 2> activeValidationFeatures = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
	validationFeaturesInfo =
	{
		VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
		&debugMessengerCI,
		(uint32_t)activeValidationFeatures.size(),
		activeValidationFeatures.data(),
		0,
		nullptr
	};
#endif

	const VkApplicationInfo applicationInfo =
	{
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr,
		"Ising GPU",
		1,													// App version
		nullptr,
		1,													// Engine version
		VK_API_VERSION_1_3									// API version
	};

	const VkInstanceCreateInfo instanceCI =
	{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		&validationFeaturesInfo,
		0,
		&applicationInfo,
		(uint32_t)activeValidationLayers.size(),
		activeValidationLayers.data(),
		(uint32_t)activeInstanceExtensions.size(),
		activeInstanceExtensions.data()
	};

	VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &context.instance));


#if defined(VKB_VALIDATION_LAYERS)
	// ---- Debug messenger handle creation ----
	PFN_vkCreateDebugUtilsMessengerEXT pfnCreateVulkanDebugUtilsMessenger =
		(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT");

	if (pfnCreateVulkanDebugUtilsMessenger != nullptr)
	{
		VK_CHECK(pfnCreateVulkanDebugUtilsMessenger(context.instance, &debugMessengerCI, nullptr, &context.debugMessenger));
	}
	else
	{
		throw std::runtime_error("Failed to get address to 'vkCreateDebugUtilsMessengerEXT' function!");
	}
#endif
}

/**********************************************************************/

void cSetup::PrepareVulkanDevice(const std::vector<const char*> requiredDeviceExtensions)
{
	// ---- Find a GPU and a queue index of a graphics and compute queue ----
	uint32_t numberOfGPUs = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(context.instance, &numberOfGPUs, nullptr));

	if (numberOfGPUs < 1)
	{
		throw std::runtime_error("Failed to find GPU!");
	}

	std::vector<VkPhysicalDevice> gpus(numberOfGPUs);
	VK_CHECK(vkEnumeratePhysicalDevices(context.instance, &numberOfGPUs, gpus.data()));

	for (uint32_t i = 0; i < numberOfGPUs && (context.computeQueueIndex < 0); i++)
	{
		context.gpu = gpus[i];

		uint32_t numberOfQueueFamilies = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &numberOfQueueFamilies, nullptr);

		if (numberOfQueueFamilies < 1)
		{
			vkGetPhysicalDeviceProperties(context.gpu, &context.gpuProperties);
			std::cout << "No queue family found for GPU: " << context.gpuProperties.deviceName << '\n';
			std::abort();
		}

		std::vector<VkQueueFamilyProperties> queueFamilyProperties(numberOfQueueFamilies);
		vkGetPhysicalDeviceQueueFamilyProperties(context.gpu, &numberOfQueueFamilies, queueFamilyProperties.data());

		// Find a compute queue
		for (uint32_t j = 0; j < numberOfQueueFamilies; j++)
		{
			if (queueFamilyProperties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				context.computeQueueIndex = j;
				break;
			}
		}
	}

	if (context.computeQueueIndex < 0)
	{
		throw std::runtime_error("Failed to find queue with compute support!");
	}

	const uint32_t desiredLocalWorkGroupSize = 64;	// <--- Important
	vkGetPhysicalDeviceProperties(context.gpu, &context.gpuProperties);
	context.localWorkGroupSizeInX = (context.gpuProperties.limits.maxComputeWorkGroupInvocations > desiredLocalWorkGroupSize) ?
		desiredLocalWorkGroupSize : context.gpuProperties.limits.maxComputeWorkGroupInvocations;
	context.maxWorkGroupCountPerDispatchInX = context.gpuProperties.limits.maxComputeWorkGroupCount[0];

	// ---- Validate required device extensions ----
	uint32_t numberOfAvailableDeviceExtensions = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &numberOfAvailableDeviceExtensions, nullptr));

	std::vector<VkExtensionProperties> availableDeviceExtensionProperties(numberOfAvailableDeviceExtensions);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(context.gpu, nullptr, &numberOfAvailableDeviceExtensions, availableDeviceExtensionProperties.data()));

	if (!ValidateVulkanExtensions(requiredDeviceExtensions, availableDeviceExtensionProperties))
	{
		throw std::runtime_error("Failed to find required device extension(s)!");
	}

	std::vector<const char*> activeDeviceExtensions(requiredDeviceExtensions);

	// ---- Device create info ----
	const float computeQueuePriority = 1.0f;

	VkDeviceQueueCreateInfo computeQueueCI =
	{
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		nullptr,
		0,
		(uint32_t)context.computeQueueIndex,
		1,
		&computeQueuePriority
	};

	const VkDeviceCreateInfo deviceCI =
	{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr,
		0,
		1,
		&computeQueueCI,
		0,												// Is ignored
		nullptr,										// Is ignored
		(uint32_t)activeDeviceExtensions.size(),
		activeDeviceExtensions.data(),
		nullptr
	};

	VK_CHECK(vkCreateDevice(context.gpu, &deviceCI, nullptr, &context.device));

	// Get the queues
	vkGetDeviceQueue(context.device, context.computeQueueIndex, 0, &context.computeQueue);;
}

/**********************************************************************/

void cSetup::PrepareVulkanSSBSpinBuffer(const uint32_t isingL)
{
	const uint32_t isingN = isingL * isingL;
	const VkDeviceSize bufferByteSize = isingN * sizeof(uint32_t);
	context.SSBSpinBufferByteSize = bufferByteSize;
	int* pIsingGrid = new int[isingN];

	// Align the spins
	for (uint32_t i = 0; i < isingN; i++)
	{
		pIsingGrid[i] = 1;
	}

	// Copy the spin grid to the staging buffer
	assert(context.persistentStagingBuffer != VK_NULL_HANDLE);
	void* pStagingBuffer = reinterpret_cast<char*>(context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory) +
		context.persistentStagingBufferByteOffsetIntoTheBigHostVisibleBuffer;
	std::memcpy(pStagingBuffer, pIsingGrid, bufferByteSize);

	// Suballocate the spin buffer from the device local buffer
	context.SSBSpinBuffer = SuballocateBufferFromTheBigDeviceLocalVulkanBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, bufferByteSize, context.SSBSpinBufferByteOffsetIntoTheBigDeviceLocalBuffer);

	// Prepare the command pool, command buffer and copy region
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	CreateAndBeginOneTimeVulkanCommandBuffer(context, commandPool, commandBuffer);

	const VkBufferCopy bufferCopyRegion =
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferByteSize
	};

	vkCmdCopyBuffer(commandBuffer, context.persistentStagingBuffer, context.SSBSpinBuffer, 1, &bufferCopyRegion);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit the command buffer
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(context.computeQueue));

	// Resource destruction
	vkDestroyCommandPool(context.device, commandPool, nullptr);
	delete[] pIsingGrid;
}

/**********************************************************************/

void cSetup::PrepareVulkanSSBRandomNumbersBuffer(const uint32_t isingL)
{
	const uint32_t isingN = isingL * isingL;
	const uint32_t randomSeed = (uint32_t) std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) % std::numeric_limits<uint32_t>::max();
	std::default_random_engine randomNumberGenerator(randomSeed);

	const VkDeviceSize bufferByteSize = isingN * sizeof(uint32_t);
	context.SSBRandomNumbersBufferByteSize = bufferByteSize;
	uint32_t* pRandomNumbers = new uint32_t [isingN];

	for (uint32_t i = 0; i < isingN; i++)
	{
		pRandomNumbers[i] = randomNumberGenerator();
	}

	assert(context.persistentStagingBuffer != VK_NULL_HANDLE);
	void* pStagingBuffer = reinterpret_cast<char*>(context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory) + context.persistentStagingBufferByteOffsetIntoTheBigHostVisibleBuffer;
	std::memcpy(pStagingBuffer, pRandomNumbers, bufferByteSize);

	context.SSBRandomNumbersBuffer = SuballocateBufferFromTheBigDeviceLocalVulkanBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, bufferByteSize, context.SSBRandomNumbersBufferByteOffsetIntoTheBigDeviceLocalBuffer);

	// Prepare the command pool, command buffer and copy region
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	CreateAndBeginOneTimeVulkanCommandBuffer(context, commandPool, commandBuffer);

	const VkBufferCopy bufferCopyRegion =
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferByteSize
	};

	vkCmdCopyBuffer(commandBuffer, context.persistentStagingBuffer, context.SSBRandomNumbersBuffer, 1, &bufferCopyRegion);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit the command buffer
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(context.computeQueue));

	// Resource destruction
	vkDestroyCommandPool(context.device, commandPool, nullptr);
	delete[] pRandomNumbers;
}

/**********************************************************************/

void cSetup::PrepareVulkanSSBSpinSumBuffer(const uint32_t isingL)
{
	const VkDeviceSize bufferByteSize = sizeof(int);
	context.SSBSpinSumBufferByteSize = bufferByteSize;
	const uint32_t isingN = isingL * isingL;
	const int startSpinSum = isingN;
	
	assert(context.persistentStagingBuffer != VK_NULL_HANDLE);
	void* pStagingBuffer = reinterpret_cast<char*>(context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory) + context.persistentStagingBufferByteOffsetIntoTheBigHostVisibleBuffer;
	std::memcpy(pStagingBuffer, &startSpinSum, bufferByteSize);

	context.SSBSpinSumBuffer = SuballocateBufferFromTheBigDeviceLocalVulkanBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		bufferByteSize, context.SSBSpinSumBufferByteOffsetIntoTheBigDeviceLocalBuffer
	);

	// Prepare the command pool, command buffer and copy region
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	CreateAndBeginOneTimeVulkanCommandBuffer(context, commandPool, commandBuffer);

	const VkBufferCopy bufferCopyRegion =
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferByteSize
	};
	
	vkCmdCopyBuffer(commandBuffer, context.persistentStagingBuffer, context.SSBSpinSumBuffer, 1, &bufferCopyRegion);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit the command buffer
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(context.computeQueue));

	// Resource destruction
	vkDestroyCommandPool(context.device, commandPool, nullptr);
}

/**********************************************************************/

void cSetup::PrepareVulkanSpinSumOutputBuffer(const uint32_t numberOfSweepsPerTemperature, const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample)
{
	context.spinSumOutputBufferByteSize = (VkDeviceSize)
		std::ceil((double)(numberOfSweepsPerTemperature - numberOfSweepsToWaitBeforeSpinSumSamplingStarts - 1) / sweepsPerSpinSumSample)
		* sizeof(int);
	
	context.spinSumOutputBuffer = SuballocateBufferFromTheBigHostVisibleVulkanBuffer(
		VK_BUFFER_USAGE_TRANSFER_DST_BIT, context.spinSumOutputBufferByteSize, context.spinSumOutputBufferByteOffsetIntoTheBigHostVisibleBuffer
	);
}

/**********************************************************************/

void cSetup::PrepareDescriptorSet(const uint32_t isingL, eComputeShaderType computeShaderType)
{
	// Descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBindings;
	// binding = 0 <=> spin buffer or spin batches buffer
	descriptorSetLayoutBindings[0] =
	{
		0,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};
	// binding = 1 <=> random numbers buffer
	descriptorSetLayoutBindings[1] =
	{
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};
	// binding = 2 <=> spin sum buffer
	descriptorSetLayoutBindings[2] =
	{
		2,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};
	// binding = 3 <=> uniform buffer
	descriptorSetLayoutBindings[3] =
	{
		3,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1,
		VK_SHADER_STAGE_COMPUTE_BIT,
		nullptr
	};

	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		(uint32_t)descriptorSetLayoutBindings.size(),
		descriptorSetLayoutBindings.data()
	};

	VK_CHECK(vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCI, nullptr, &context.descriptorSetLayout));

	// Descriptor pool
	std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes;
	descriptorPoolSizes[0] =
	{
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		3
	};
	descriptorPoolSizes[1] =
	{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		1
	};

	const VkDescriptorPoolCreateInfo descriptorPoolCI =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		nullptr,
		0,
		1,
		(uint32_t)descriptorPoolSizes.size(),
		descriptorPoolSizes.data()
	};

	VK_CHECK(vkCreateDescriptorPool(context.device, &descriptorPoolCI, nullptr, &context.descriptorPool));

	// Descriptor set
	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
	{
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		nullptr,
		context.descriptorPool,
		1,
		&context.descriptorSetLayout
	};

	VK_CHECK(vkAllocateDescriptorSets(context.device, &descriptorSetAllocateInfo, &context.descriptorSet));

	// Write the descriptor set (binding 0, 1, 2, and 3)
	VkDescriptorBufferInfo SSBSpinBufferOrSpinBatchesBufferDescriptorBufferInfo;
	if (computeShaderType == COMPUTE_SHADER_TYPE_1_INT_PER_SPIN)
	{
		SSBSpinBufferOrSpinBatchesBufferDescriptorBufferInfo =
		{
			context.SSBSpinBuffer,
			0,
			context.SSBSpinBufferByteSize
		};
	}
	else if (computeShaderType == COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN)
	{
		SSBSpinBufferOrSpinBatchesBufferDescriptorBufferInfo =
		{
			context.SSBSpinBatchesBuffer,
			0,
			context.SSBSpinBatchesBufferByteSize
		};
	}

	const VkDescriptorBufferInfo SSBRandomNumbersBufferDescriptorBufferInfo =
	{
		context.SSBRandomNumbersBuffer,
		0,
		context.SSBRandomNumbersBufferByteSize
	};

	const VkDescriptorBufferInfo SSBSpinSumBufferDescriptorBufferInfo =
	{
		context.SSBSpinSumBuffer,
		0,
		context.SSBSpinSumBufferByteSize
	};

	const VkDescriptorBufferInfo uniformBufferDescriptorBufferInfo =
	{
		context.uniformBuffer,
		0,
		context.uniformBufferByteSize
	};

	std::array<VkWriteDescriptorSet, 4> descriptorSetWrites;

	// binding = 0 <=> spin buffer
	descriptorSetWrites[0] =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		context.descriptorSet,
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		nullptr,
		&SSBSpinBufferOrSpinBatchesBufferDescriptorBufferInfo,
		nullptr
	};

	// binding = 1 <=> random numbers buffer
	descriptorSetWrites[1] =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		context.descriptorSet,
		1,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		nullptr,
		&SSBRandomNumbersBufferDescriptorBufferInfo,
		nullptr
	};

	// binding = 2 <=> spin sum buffer
	descriptorSetWrites[2] =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		context.descriptorSet,
		2,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		nullptr,
		&SSBSpinSumBufferDescriptorBufferInfo,
		nullptr
	};
	
	// binding = 3 <=> uniform buffer
	descriptorSetWrites[3] =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		context.descriptorSet,
		3,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&uniformBufferDescriptorBufferInfo,
		nullptr
	};

	vkUpdateDescriptorSets(context.device, (uint32_t)descriptorSetWrites.size(), descriptorSetWrites.data(), 0, nullptr);
}

/**********************************************************************/

void cSetup::PrepareComputePipeline(eComputeShaderType computeShaderType)
{
	// Push constant info
	const VkPushConstantRange pushConstantRange =
	{
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(sPushConstantObject)
	};

	// Create the compute pipeline layout first
	const VkPipelineLayoutCreateInfo pipelineLayoutCI =
	{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		1,																		// Descriptor set layout count
		&context.descriptorSetLayout,
		1,																		// Push constant count
		&pushConstantRange
	};

	VK_CHECK(vkCreatePipelineLayout(context.device, &pipelineLayoutCI, nullptr, &context.computePipelineLayout));

	// Use specialization constants to pass in context.localWorkGroupSizeInX to the kernel
	const VkSpecializationMapEntry specializationMapEntry =
	{
		0,
		0,
		sizeof(uint32_t)
	};
	const VkSpecializationInfo specializationInfo =
	{
		1,
		&specializationMapEntry,
		sizeof(uint32_t),
		&context.localWorkGroupSizeInX
	};

	VkPipelineShaderStageCreateInfo shaderStageCI;

	if (computeShaderType == COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN)
	{
		shaderStageCI =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			LoadShaderModule(context, "IsingKernelOneBitPerSpin.spv"),
			"main",
			&specializationInfo
		};
	}
	else if (computeShaderType == COMPUTE_SHADER_TYPE_1_INT_PER_SPIN)
	{
		shaderStageCI =
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_COMPUTE_BIT,
			LoadShaderModule(context, "IsingKernelOneIntPerSpin.spv"),
			"main",
			&specializationInfo
		};
	}

	// Then create the compute pipeline
	const VkComputePipelineCreateInfo computePipelineCI =
	{
		VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		nullptr,
		0,
		shaderStageCI,
		context.computePipelineLayout,
		VK_NULL_HANDLE,
		0
	};

	VK_CHECK(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &context.computePipeline));

	// Destroy the shader module
	vkDestroyShaderModule(context.device, shaderStageCI.module, nullptr);
}

/**********************************************************************/

void cSetup::PrepareCommandPoolAndCommandBuffer()
{
	const VkCommandPoolCreateInfo commandPoolCI =
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		0,
		(uint32_t)context.computeQueueIndex
	};

	VK_CHECK(vkCreateCommandPool(context.device, &commandPoolCI, nullptr, &context.commandPool));

	const VkCommandBufferAllocateInfo commandBufferAllocateInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		context.commandPool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};

	VK_CHECK(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &context.commandBuffer));
}

/**********************************************************************/

void cSetup::WriteToUniformBufferAndUpdateDescriptorSet(const double beta, const uint32_t isingL)
{
	const uint32_t isingN = isingL * isingL;
	
	// Prepare the UBO with data
	sUniformBufferObject ubo;
	ubo.transitionProbability4 = (uint32_t)std::ceil(std::exp(-beta * 4.0) * 100'000'000);
	ubo.transitionProbability8 = (uint32_t)std::ceil(std::exp(-beta * 8.0) * 100'000'000);
	ubo.isingL = isingL;
	ubo.isingN = isingN;

	// Copy to the VkBuffer
	assert(context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory != nullptr);
	void* pUniformBuffer = reinterpret_cast<char*>(context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory) + context.uniformBufferByteOffsetIntoTheBigHostVisibleBuffer;
	std::memcpy(pUniformBuffer, &ubo, sizeof(ubo));
	
	// Update the descriptor set
	const VkDescriptorBufferInfo uniformBufferDescriptorBufferInfo =
	{
		context.uniformBuffer,
		0,
		sizeof(sUniformBufferObject)
	};

	const VkWriteDescriptorSet descriptorSetWrite =
	{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		nullptr,
		context.descriptorSet,
		3,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		nullptr,
		&uniformBufferDescriptorBufferInfo,
		nullptr
	};
	
	vkUpdateDescriptorSets(context.device, 1, &descriptorSetWrite, 0, nullptr);
}

/**********************************************************************/

void DoTheIsingGridSweepsGPU(cSetup* pTheSetup, const uint32_t isingL, const double beta,
	const uint32_t numberOfSweepsPerTemperature, const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample)
{
	const uint32_t isingN = isingL * isingL;
	uint32_t numberOfWorkGroupsInX = (uint32_t)std::ceil(isingN / (2.0 * pTheSetup->context.localWorkGroupSizeInX));
	assert(numberOfWorkGroupsInX < pTheSetup->context.maxWorkGroupCountPerDispatchInX);

	// Write descriptor set
	pTheSetup->WriteToUniformBufferAndUpdateDescriptorSet(beta, isingL);

	// Barrier to synchronize access to the spin sum buffer
	const VkBufferMemoryBarrier SSBSpinSumBufferMemoryBarrier =
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		nullptr,
		VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		0,
		0,
		pTheSetup->context.SSBSpinSumBuffer,
		0,
		pTheSetup->context.SSBSpinSumBufferByteSize
	};

	// Begin the command buffer
	const VkCommandBufferBeginInfo commandBufferBeginInfo =
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		0,
		nullptr
	};

	VK_CHECK(vkBeginCommandBuffer(pTheSetup->context.commandBuffer, &commandBufferBeginInfo));

	// -----------------------------------------------------------------
	// Record commands
	vkCmdBindPipeline(pTheSetup->context.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pTheSetup->context.computePipeline);
	vkCmdBindDescriptorSets(
		pTheSetup->context.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pTheSetup->context.computePipelineLayout,
		0, 1, &pTheSetup->context.descriptorSet, 0, nullptr);
	for (uint32_t i = 0; i < numberOfSweepsPerTemperature; i++)
	{
		const sPushConstantObject pushConstantObject = { .phase = i % 2 };

		vkCmdPushConstants(
			pTheSetup->context.commandBuffer, pTheSetup->context.computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstantObject), &pushConstantObject
		);

		vkCmdDispatch(pTheSetup->context.commandBuffer, numberOfWorkGroupsInX, 1, 1);

		vkCmdPipelineBarrier(
			pTheSetup->context.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, // MUST BE OUTSIDE IF!!!
			0, 0, nullptr, 1, &SSBSpinSumBufferMemoryBarrier, 0, nullptr
		);

		// Check if the magnetization (spin sum) should be stored
		if (i >= numberOfSweepsToWaitBeforeSpinSumSamplingStarts && (i - numberOfSweepsToWaitBeforeSpinSumSamplingStarts) % sweepsPerSpinSumSample == 0)
		{
			const VkDeviceSize dst_offset = ((i - numberOfSweepsToWaitBeforeSpinSumSamplingStarts) / sweepsPerSpinSumSample) * sizeof(int);
			const VkBufferCopy copy_region =
			{
				0,
				dst_offset,
				sizeof(int)
			};
			vkCmdCopyBuffer(pTheSetup->context.commandBuffer, pTheSetup->context.SSBSpinSumBuffer, pTheSetup->context.spinSumOutputBuffer, 1, &copy_region);
		}

		vkCmdPipelineBarrier(pTheSetup->context.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // MUST BE OUTSIDE IF!!!
			0, 0, nullptr, 1, &SSBSpinSumBufferMemoryBarrier, 0, nullptr);

		if ((i+1) % 500000 == 0)
		{
			VK_CHECK(vkEndCommandBuffer(pTheSetup->context.commandBuffer));
			VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &pTheSetup->context.commandBuffer;
			VK_CHECK(vkQueueSubmit(pTheSetup->context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
			VK_CHECK(vkQueueWaitIdle(pTheSetup->context.computeQueue));
			VK_CHECK(vkResetCommandPool(pTheSetup->context.device, pTheSetup->context.commandPool, 0));
			VK_CHECK(vkBeginCommandBuffer(pTheSetup->context.commandBuffer, &commandBufferBeginInfo));
			vkCmdBindPipeline(pTheSetup->context.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pTheSetup->context.computePipeline);
			vkCmdBindDescriptorSets(
				pTheSetup->context.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pTheSetup->context.computePipelineLayout, 0, 1, &pTheSetup->context.descriptorSet, 0, nullptr
			);
		}
	}
	// -----------------------------------------------------------------

	// End and submit
	VK_CHECK(vkEndCommandBuffer(pTheSetup->context.commandBuffer));

	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pTheSetup->context.commandBuffer;

	VK_CHECK(vkQueueSubmit(pTheSetup->context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(pTheSetup->context.computeQueue));
	
	// Reset the command pool (and buffer)
	VK_CHECK(vkResetCommandPool(pTheSetup->context.device, pTheSetup->context.commandPool, 0));
}

/**********************************************************************/

/**********************************************************************/

void cSetup::PrepareBigDeviceLocalVulkanBufferAndMore(VkDeviceSize bufferByteSize)
{
	const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	context.bigDeviceLocalBufferUsageFlags = bufferUsageFlags;
	context.bigDeviceLocalBufferBytesLeft = bufferByteSize;

	const VkBufferCreateInfo bufferCI =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = bufferByteSize,
		.usage = bufferUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = reinterpret_cast<uint32_t*>(&context.computeQueueIndex)
	};

	VK_CHECK(vkCreateBuffer(context.device, &bufferCI, nullptr, &context.bigDeviceLocalBufferAndMore.buffer));

	VkMemoryRequirements bufferMemoryRequirements;
	vkGetBufferMemoryRequirements(context.device, context.bigDeviceLocalBufferAndMore.buffer, &bufferMemoryRequirements);
	context.bigDeviceLocalBufferByteAlignment = bufferMemoryRequirements.alignment;

	const VkMemoryAllocateInfo bufferMemoryAllocateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = bufferMemoryRequirements.size,
		.memoryTypeIndex = FindVulkanMemoryType(context, bufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};

	VK_CHECK(vkAllocateMemory(context.device, &bufferMemoryAllocateInfo, nullptr, &context.bigDeviceLocalBufferAndMore.bufferMemory));
}

/**********************************************************************/

VkBuffer cSetup::SuballocateBufferFromTheBigDeviceLocalVulkanBuffer(VkBufferUsageFlags bufferUsage, VkDeviceSize bufferByteSize, VkDeviceSize& memoryByteOffset)
{
	assert(bufferByteSize < context.bigDeviceLocalBufferBytesLeft);
	assert(bufferUsage & context.bigDeviceLocalBufferUsageFlags);

	VkBuffer buffer;
	const VkBufferCreateInfo bufferCI =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = bufferByteSize,
		.usage = bufferUsage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = reinterpret_cast<uint32_t*>(&context.computeQueueIndex)
	};

	VK_CHECK(vkCreateBuffer(context.device, &bufferCI, nullptr, &buffer));
	VkMemoryRequirements bufferMemoryRequirements;
	vkGetBufferMemoryRequirements(context.device, buffer, &bufferMemoryRequirements);

	if (context.bigDeviceLocalBufferNextAvailableByte % bufferMemoryRequirements.alignment == 0)
	{
		VK_CHECK(vkBindBufferMemory(context.device, buffer, context.bigDeviceLocalBufferAndMore.bufferMemory,
			context.bigDeviceLocalBufferNextAvailableByte));

		memoryByteOffset = context.bigDeviceLocalBufferNextAvailableByte;

		context.bigDeviceLocalBufferBytesLeft -= bufferMemoryRequirements.size;
		context.bigDeviceLocalBufferNextAvailableByte += bufferMemoryRequirements.size;
	}
	else
	{
		VkDeviceSize aligmentByteAdjusment = bufferMemoryRequirements.alignment -
			(context.bigDeviceLocalBufferNextAvailableByte % bufferMemoryRequirements.alignment);

		context.bigDeviceLocalBufferNextAvailableByte += aligmentByteAdjusment;

		VK_CHECK(vkBindBufferMemory(context.device, buffer, context.bigDeviceLocalBufferAndMore.bufferMemory,
			context.bigDeviceLocalBufferNextAvailableByte));

		memoryByteOffset = context.bigDeviceLocalBufferNextAvailableByte;

		context.bigDeviceLocalBufferBytesLeft -= bufferMemoryRequirements.size;
		context.bigDeviceLocalBufferNextAvailableByte += bufferMemoryRequirements.size;
	}

	return buffer;
}

/**********************************************************************/

void cSetup::PrepareBigHostVisibleVulkanBufferAndMore(VkDeviceSize bufferByteSize)
{
	const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	context.bigHostVisibleVulkanBufferUsageFlags = bufferUsageFlags;
	context.bigHostVisibleVulkanBufferBytesLeft = bufferByteSize;

	const VkBufferCreateInfo bufferCI =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = bufferByteSize,
		.usage = bufferUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = reinterpret_cast<uint32_t*>(&context.computeQueueIndex)
	};

	VK_CHECK(vkCreateBuffer(context.device, &bufferCI, nullptr, &context.bigHostVisibleVulkanBufferAndMore.buffer));

	VkMemoryRequirements bufferMemoryRequirements;
	vkGetBufferMemoryRequirements(context.device, context.bigHostVisibleVulkanBufferAndMore.buffer, &bufferMemoryRequirements);
	context.bigHostVisibleVulkanBufferByteAlignment = bufferMemoryRequirements.alignment;

	const VkMemoryAllocateInfo bufferMemoryAllocateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = bufferMemoryRequirements.size,
		.memoryTypeIndex = FindVulkanMemoryType(context, bufferMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};

	VK_CHECK(vkAllocateMemory(context.device, &bufferMemoryAllocateInfo, nullptr, &context.bigHostVisibleVulkanBufferAndMore.bufferMemory));
	VK_CHECK(vkMapMemory(context.device, context.bigHostVisibleVulkanBufferAndMore.bufferMemory, 0, bufferByteSize, 0, &context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory));
}

/**********************************************************************/

VkBuffer cSetup::SuballocateBufferFromTheBigHostVisibleVulkanBuffer(VkBufferUsageFlags bufferUsage, VkDeviceSize bufferByteSize, VkDeviceSize& memoryByteOffset)
{
	assert(bufferByteSize < context.bigHostVisibleVulkanBufferBytesLeft);
	assert(bufferUsage & context.bigHostVisibleVulkanBufferUsageFlags);

	VkBuffer buffer;
	const VkBufferCreateInfo bufferCI =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = bufferByteSize,
		.usage = bufferUsage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = reinterpret_cast<uint32_t*>(&context.computeQueueIndex)
	};

	VK_CHECK(vkCreateBuffer(context.device, &bufferCI, nullptr, &buffer));
	VkMemoryRequirements bufferMemoryRequirements;
	vkGetBufferMemoryRequirements(context.device, buffer, &bufferMemoryRequirements);

	if (context.bigHostVisibleVulkanBufferNextAvailableByte % bufferMemoryRequirements.alignment == 0)
	{
		VK_CHECK(vkBindBufferMemory(context.device, buffer, context.bigHostVisibleVulkanBufferAndMore.bufferMemory,
			context.bigHostVisibleVulkanBufferNextAvailableByte));

		memoryByteOffset = context.bigHostVisibleVulkanBufferNextAvailableByte;

		context.bigHostVisibleVulkanBufferBytesLeft -= bufferMemoryRequirements.size;
		context.bigHostVisibleVulkanBufferNextAvailableByte += bufferMemoryRequirements.size;
	}
	else
	{
		const VkDeviceSize aligmentByteAdjusment = bufferMemoryRequirements.alignment -
			(context.bigHostVisibleVulkanBufferNextAvailableByte % bufferMemoryRequirements.alignment);

		context.bigHostVisibleVulkanBufferNextAvailableByte += aligmentByteAdjusment;

		VK_CHECK(vkBindBufferMemory(context.device, buffer, context.bigHostVisibleVulkanBufferAndMore.bufferMemory,
			context.bigHostVisibleVulkanBufferNextAvailableByte));

		memoryByteOffset = context.bigHostVisibleVulkanBufferNextAvailableByte;

		context.bigHostVisibleVulkanBufferBytesLeft -= bufferMemoryRequirements.size;
		context.bigHostVisibleVulkanBufferNextAvailableByte += bufferMemoryRequirements.size;
	}

	return buffer;
}

/**********************************************************************/

void cSetup::PrepareVulkanSSBSpinBatchesBuffer(const uint32_t isingL)
{
	const uint32_t isingN = isingL * isingL;
	const uint32_t numberOfSpinBatches = (uint32_t)std::ceil(isingN / 32.0f);
	const uint32_t bufferByteSize = numberOfSpinBatches * 4;
	context.SSBSpinBatchesBufferByteSize = bufferByteSize;

	uint32_t* spinBatches = new uint32_t[numberOfSpinBatches];
	for (uint32_t i = 0; i < numberOfSpinBatches; i++)
	{
		spinBatches[i] = ~0U;																// All spins are 1
	}

	assert(bufferByteSize <= context.persistentStagingBufferByteSize);
	void* pStagingBuffer = reinterpret_cast<char*>(context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory)
		+ context.persistentStagingBufferByteOffsetIntoTheBigHostVisibleBuffer;
	std::memcpy(pStagingBuffer, spinBatches, bufferByteSize);
	delete[] spinBatches;

	assert(bufferByteSize <= context.bigDeviceLocalBufferBytesLeft);

	context.SSBSpinBatchesBuffer = SuballocateBufferFromTheBigDeviceLocalVulkanBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, bufferByteSize, context.SSBSpinBatchesBufferByteOffsetIntoTheBigDeviceLocalBuffer
	);

	// Prepare the command pool, command buffer and copy region
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	CreateAndBeginOneTimeVulkanCommandBuffer(context, commandPool, commandBuffer);

	const VkBufferCopy bufferCopyRegion =
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = bufferByteSize
	};

	vkCmdCopyBuffer(commandBuffer, context.persistentStagingBuffer, context.SSBSpinBatchesBuffer, 1, &bufferCopyRegion);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit the command buffer
	VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK(vkQueueSubmit(context.computeQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(context.computeQueue));

	// Resource destruction
	vkDestroyCommandPool(context.device, commandPool, nullptr);
}

/**********************************************************************/

cSetup::cSetup(const uint32_t ising_L, const uint32_t numberOfSweepsPerTemperature,
	const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample, eComputeShaderType computeShaderType)
{
	PrepareVulkanInstance({}, { "VK_LAYER_KHRONOS_validation" });
	PrepareVulkanDevice({});
	PrepareBigDeviceLocalVulkanBufferAndMore(48'000'000);
	PrepareBigHostVisibleVulkanBufferAndMore(48'000'000);
	context.persistentStagingBufferByteSize = 24'000'000;
	context.persistentStagingBuffer = SuballocateBufferFromTheBigHostVisibleVulkanBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, context.persistentStagingBufferByteSize, context.persistentStagingBufferByteOffsetIntoTheBigHostVisibleBuffer
	);
	context.uniformBufferByteSize = sizeof(sUniformBufferObject);
	context.uniformBuffer = SuballocateBufferFromTheBigHostVisibleVulkanBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, context.uniformBufferByteSize, context.uniformBufferByteOffsetIntoTheBigHostVisibleBuffer
	);
	if (computeShaderType == COMPUTE_SHADER_TYPE_1_INT_PER_SPIN)
	{
		PrepareVulkanSSBSpinBuffer(ising_L);
	}
	else if (computeShaderType == COMPUTE_SHADER_TYPE_1_BIT_PER_SPIN)
	{
		PrepareVulkanSSBSpinBatchesBuffer(ising_L);
	}
	PrepareVulkanSSBRandomNumbersBuffer(ising_L);
	PrepareVulkanSSBSpinSumBuffer(ising_L);
	PrepareVulkanSpinSumOutputBuffer(numberOfSweepsPerTemperature, numberOfSweepsToWaitBeforeSpinSumSamplingStarts, sweepsPerSpinSumSample);
	PrepareDescriptorSet(ising_L, computeShaderType);
	PrepareComputePipeline(computeShaderType);
	PrepareCommandPoolAndCommandBuffer();
}

/**********************************************************************/

cSetup::~cSetup()
{
	if (context.persistentStagingBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.persistentStagingBuffer, nullptr);
	}
	if (context.commandPool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(context.device, context.commandPool, nullptr);
	}
	if (context.computePipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(context.device, context.computePipeline, nullptr);
	}
	if (context.computePipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(context.device, context.computePipelineLayout, nullptr);
	}
	if (context.descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(context.device, context.descriptorPool, nullptr);
	}
	if (context.descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(context.device, context.descriptorSetLayout, nullptr);
	}
	if (context.spinSumOutputBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.spinSumOutputBuffer, nullptr);
	}
	if (context.uniformBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.uniformBuffer, nullptr);
	}
	if (context.SSBSpinSumBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.SSBSpinSumBuffer, nullptr);
	}
	if (context.SSBSpinBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.SSBSpinBuffer, nullptr);
	}
	if (context.SSBRandomNumbersBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.SSBRandomNumbersBuffer, nullptr);
	}
	if (context.SSBSpinBatchesBuffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(context.device, context.SSBSpinBatchesBuffer, nullptr);
	}
	DestroyVulkanBufferAndMore(context.bigDeviceLocalBufferAndMore);
	DestroyVulkanBufferAndMore(context.bigHostVisibleVulkanBufferAndMore);
	if (context.device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(context.device, nullptr);
	}
	if (context.debugMessenger != VK_NULL_HANDLE)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT");
		if (destroy_debug_utils_messenger != nullptr)
		{
			destroy_debug_utils_messenger(context.instance, context.debugMessenger, nullptr);
		}
		else
		{
			std::cout << "Failed to get address to 'vkDestroyDebugUtilsMessengerEXT' function!\n";
		}
	}
	if (context.instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(context.instance, nullptr);
	}
}

/**********************************************************************/

double CalculateBinderCumulantGPU(cSetup* pTheSetup, const uint32_t isingL)
{
	const uint32_t isingN = isingL * isingL;
	const uint32_t numberOfElementsInTheSpinSumOutputBuffer = static_cast<uint32_t>(pTheSetup->context.spinSumOutputBufferByteSize / sizeof(int));
	double m4Sum = 0;
	double m2Sum = 0;
	int* pSpinSumOutputBuffer = reinterpret_cast<int*>(reinterpret_cast<char*>(pTheSetup->context.bigHostVisibleVulkanBufferAndMore.pVulkanBufferMemory)
		+ pTheSetup->context.spinSumOutputBufferByteOffsetIntoTheBigHostVisibleBuffer);
	
	for (uint32_t i = 0; i < numberOfElementsInTheSpinSumOutputBuffer; i++)
	{
		const double averageSpinPerSite = (double)(*pSpinSumOutputBuffer) / (double)isingN;						// The average spin per site
		m4Sum += std::pow(averageSpinPerSite, 4);
		m2Sum += std::pow(averageSpinPerSite, 2);
		pSpinSumOutputBuffer++;
	}

	// Calculate the Binder Cumulant
	const double m4Average = m4Sum / numberOfElementsInTheSpinSumOutputBuffer;									// The state average
	const double m2Average = m2Sum / numberOfElementsInTheSpinSumOutputBuffer;									// The state average
	const double binderCumulant = 1.0 - (m4Average / (3.0 * std::pow(m2Average, 2)));

	return binderCumulant;
}

/**********************************************************************/

void DoTheIsingGridSweepsCPU(uint32_t* pArraySpinBatches, int* pArraySpinSumOutputs, int& TheSpinSum, const uint32_t isingL, const double beta, const uint32_t numberOfSweepsPerTemperature,
	const uint32_t numberOfSweepsToWaitBeforeSpinSumSamplingStarts, const uint32_t sweepsPerSpinSumSample)
{
	// ------------------------------------------------------------------------------------------
	const uint32_t transitionProbability4 = (uint32_t)std::ceil(std::exp(-1.0 * beta * 4.0) * 100'000'000);			// The transition probability if deltaE = +4.
	const uint32_t transitionProbability8 = (uint32_t)std::ceil(std::exp(-1.0 * beta * 8.0) * 100'000'000);			// The transition probability if deltaE = +8.
	uint32_t spinSumOutputsIndex = 0;																				// Used to index into pArraySpinSumOutputs
	const uint32_t randomSeed = (uint32_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) % std::numeric_limits<uint32_t>::max();
	std::default_random_engine randomNumberGenerator(randomSeed);
	
	// ------------------------------------------------------------------------------------------

	for (uint32_t sweepNumber = 0; sweepNumber < numberOfSweepsPerTemperature; sweepNumber++)
	{
		uint32_t randomState = randomNumberGenerator();

		// One way to accomplish the checkerboard sweep pattern
		const uint32_t sweepPatternPhase = sweepNumber % 2;
		for (uint32_t rowNumber = 0; rowNumber < isingL; rowNumber++)
		{
			for (uint32_t columnNumber = (rowNumber + sweepPatternPhase) % 2; columnNumber < isingL; columnNumber += 2)
			{
				// The spins are set to 1 by default here and changed to -1 below if the spin is -1
				int centerSpinSpin = 1, rightSpinSpin = 1, leftSpinSpin = 1;
				int aboveSpinSpin = 1, belowSpinSpin = 1;

				const uint32_t centerSpinSpinBatch = (rowNumber * isingL + columnNumber) / 32;						// Integer division
				const uint32_t centerSpinSpinBatchBit = (rowNumber * isingL + columnNumber) % 32;
				if ((pArraySpinBatches[centerSpinSpinBatch] & (1 << (31 - centerSpinSpinBatchBit))) == 0)
				{
					centerSpinSpin = -1;
				}

				const uint32_t rightSpinSpinBatch = (((columnNumber + 1) % isingL) + rowNumber * isingL) / 32;
				const uint32_t rightSpinSpinBatchBit = (((columnNumber + 1) % isingL) + rowNumber * isingL) % 32;
				if ((pArraySpinBatches[rightSpinSpinBatch] & (1 << (31 - rightSpinSpinBatchBit))) == 0)
				{
					rightSpinSpin = -1;
				}

				const uint32_t leftSpinSpinBatch = (((columnNumber + isingL - 1) % isingL) + rowNumber * isingL) / 32;
				const uint32_t leftSpinSpinBatchBit = (((columnNumber + isingL - 1) % isingL) + rowNumber * isingL) % 32;
				if ((pArraySpinBatches[leftSpinSpinBatch] & (1 << (31 - leftSpinSpinBatchBit))) == 0)
				{
					leftSpinSpin = -1;
				}

				const uint32_t aboveSpinSpinBatch = (((rowNumber + isingL - 1) % isingL) * isingL + columnNumber) / 32;
				const uint32_t aboveSpinSpinBatchBit = (((rowNumber + isingL - 1) % isingL) * isingL + columnNumber) % 32;
				if ((pArraySpinBatches[aboveSpinSpinBatch] & (1 << (31 - aboveSpinSpinBatchBit))) == 0)
				{
					aboveSpinSpin = -1;
				}

				const uint32_t belowSpinSpinBatch = (((rowNumber + 1) % isingL) * isingL + columnNumber) / 32;
				const uint32_t belowSpinSpinBatchBit = (((rowNumber + 1) % isingL) * isingL + columnNumber) % 32;
				if ((pArraySpinBatches[belowSpinSpinBatch] & (1 << (31 - belowSpinSpinBatchBit))) == 0)
				{
					belowSpinSpin = -1;
				}
				
				const int deltaE = 2 * centerSpinSpin * (rightSpinSpin + leftSpinSpin
					+ aboveSpinSpin + belowSpinSpin);															// The change in energy if the spin is flipped

				bool bFlipSpin = false;
				if (deltaE <= 0)
				{
					bFlipSpin = true;
				}
				else
				{
					uint32_t randomNumber = XORShift(randomState);
					randomState = randomNumber;

					if (randomNumber % 100'000'000 < transitionProbability4 && deltaE == 4)
					{
						bFlipSpin = true;
					}
					else if (randomNumber % 100'000'000 < transitionProbability8 && deltaE == 8)
					{
						bFlipSpin = true;
					}
				}
			
				if (bFlipSpin == true)
				{
					if (centerSpinSpin == 1)
					{
						pArraySpinBatches[centerSpinSpinBatch] -= (1 << (31 - centerSpinSpinBatchBit));							// Flip the spin
					}
					else if (centerSpinSpin == -1)
					{
						pArraySpinBatches[centerSpinSpinBatch] += (1 << (31 - centerSpinSpinBatchBit));							// Flip the spin
					}
					TheSpinSum = TheSpinSum - 2 * centerSpinSpin;
				}
			}
		}

		// Save the spin sum
		if (sweepNumber >= numberOfSweepsToWaitBeforeSpinSumSamplingStarts
			&& (sweepNumber - numberOfSweepsToWaitBeforeSpinSumSamplingStarts) % sweepsPerSpinSumSample == 0)
		{
			pArraySpinSumOutputs[spinSumOutputsIndex] = TheSpinSum;
			spinSumOutputsIndex++;
		}
	}
}

/**********************************************************************/

double CalculateBinderCumulantCPU(int* pArraySpinSumOutputs, const uint32_t isingL, const uint32_t numberOfElementsInTheSpinSumOutputArray)
{
	const uint32_t isingN = isingL * isingL;
	double m4Sum = 0;
	double m2Sum = 0;

	for (uint32_t i = 0; i < numberOfElementsInTheSpinSumOutputArray; i++)
	{
		const double averageSpinPerSite = (double)(pArraySpinSumOutputs[i]) / (double)isingN;				// The average spin per site
		m4Sum += std::pow(averageSpinPerSite, 4);
		m2Sum += std::pow(averageSpinPerSite, 2);
	}

	// Calculate the Binder Cumulant
	const double m4Average = m4Sum / numberOfElementsInTheSpinSumOutputArray;									// The state average
	const double m2Average = m2Sum / numberOfElementsInTheSpinSumOutputArray;									// The state average
	const double binderCumulant = 1.0 - (m4Average / (3.0 * std::pow(m2Average, 2)));

	return binderCumulant;
}

/**********************************************************************/

uint32_t XORShift(uint32_t rngState)
{
	rngState ^= (rngState << 13);
	rngState ^= (rngState >> 17);
	rngState ^= (rngState << 5);
	return rngState;
}

/**********************************************************************/