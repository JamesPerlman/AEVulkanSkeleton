//
//  VulkanComputeProgram.cpp
//  VkComputeTest
//
//  Created by James Perlman on 10/23/21.
//

#include <set>

#include "VulkanComputeProgram.hpp"

#include "FileUtils.hpp"
#include "VulkanDebugUtils.hpp"

#define VK_ASSERT_SUCCESS(result, message) if (result != VK_SUCCESS) { throw std::runtime_error(message); }

// MARK: - Constructor

void VulkanComputeProgram::setUp(std::string shaderFilePath)
{
    this->shaderFilePath = shaderFilePath;
    createVulkanInstance();
    createDebugMessenger();
    assignPhysicalDevice();
    createLogicalDevice();
    createShaderModule();
    createCommandPool();
    createDescriptorPool();
}

// MARK: - Destructor

void VulkanComputeProgram::tearDown()
{
    destroyDescriptorPool();
    destroyPipeline();
    destroyPipelineLayout();
    destroyDescriptorSetLayout();
    destroyStorageBuffers();
    destroyCommandBuffer();
    destroyCommandPool();
    destroyShaderModule();
    destroyLogicalDevice();
    destroyDebugMessenger();
    destroyVulkanInstance();
}

// MARK: - Run
static int count = 0;
void VulkanComputeProgram::process(ImageInfo imageInfo,
                                   std::function<void(void*)> writeInputPixels,
                                   std::function<void(void*)> readOutputPixels)
{
    // TODO: Actual thread safety...
    if (count > 0) {
        return;
    }
    count++;
    
    resizeBuffersIfNeeded(imageInfo.size());
    // map input buffer memory and write pixels
    void* inputPixels;
    vkMapMemory(logicalDevice, inputMemory, 0, bufferSize, 0, &inputPixels);
    writeInputPixels(inputPixels);
    vkUnmapMemory(logicalDevice, inputMemory);
    
    // submit the compute queue and run the shader
    submitComputeQueue();
    
    // map outbut buffer memory and read pixels
    void* outputPixels;
    vkMapMemory(logicalDevice, outputMemory, 0, bufferSize, 0, &outputPixels);
    readOutputPixels(outputPixels);
    vkUnmapMemory(logicalDevice, outputMemory);
    
}

// Set or reset GPU memory if needed

void VulkanComputeProgram::resizeBuffersIfNeeded(size_t newBufferSize)
{
    if (newBufferSize != bufferSize)
    {
        bufferSize = newBufferSize;
        deviceMemorySize = 2 * bufferSize;
        
        // destroy objects that need to be recreated
        destroyCommandBuffer();
        destroyPipeline();
        destroyPipelineLayout();
        destroyDescriptorSetLayout();
        destroyStorageBuffers();
        
        // recreate objects
        createStorageBuffers();
        createDescriptorSetLayout();
        createPipelineLayout();
        createPipeline();
        createCommandBuffer();
        
        // prepare for computations
        updateDescriptorSet();
        recordCommandBuffer();
    }
}

// MARK: - Vulkan Instance

const std::vector<const char*> baseInstanceExtensions = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
};

std::vector<const char*> getRequiredInstanceExtensionNames()
{
    std::vector<const char*> extensions(baseInstanceExtensions.begin(), baseInstanceExtensions.end());
    
    if (VulkanDebugUtils::isValidationEnabled())
    {
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    return extensions;
}

void VulkanComputeProgram::createVulkanInstance()
{
    if (VulkanDebugUtils::isValidationEnabled() && !VulkanDebugUtils::isValidationSupported())
    {
        throw std::runtime_error("Validation layers requested, but not available!");
    }
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    auto requiredExtensionNames = getRequiredInstanceExtensionNames();
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensionNames.size());
    createInfo.ppEnabledExtensionNames = requiredExtensionNames.data();
    
    // Add a debugger to the instance, if enabled.
    auto debugCreateInfo = VulkanDebugUtils::getDebugMessengerCreateInfo();
    
    if (VulkanDebugUtils::isValidationEnabled())
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VulkanDebugUtils::validationLayers.size());
        createInfo.ppEnabledLayerNames = VulkanDebugUtils::validationLayers.data();
        createInfo.pNext = &debugCreateInfo;
    } else
    {
        createInfo.enabledLayerCount = 0;
    }
    
    // Create the instance!
    VK_ASSERT_SUCCESS(vkCreateInstance(&createInfo, nullptr, &instance),
                      "failed to create Vulkan instance!");
}

void VulkanComputeProgram::destroyVulkanInstance()
{
    vkDestroyInstance(instance, nullptr);
}


// MARK: - Debug Messenger
void VulkanComputeProgram::createDebugMessenger()
{
    if (!VulkanDebugUtils::isValidationEnabled())
    {
        return;
    }
    
    auto createInfo = VulkanDebugUtils::getDebugMessengerCreateInfo();
    VK_ASSERT_SUCCESS(VulkanDebugUtils::createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger),
                      "Failed to set up debug messenger!");
}

void VulkanComputeProgram::destroyDebugMessenger()
{
    if (!VulkanDebugUtils::isValidationEnabled())
    {
        return;
    }
    
    VulkanDebugUtils::destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
}

// MARK: - Physical Device

const std::vector<const char*> deviceExtensions = {
    "VK_KHR_portability_subset",
};

const std::vector<const char*> requiredDeviceExtensions = {
    // No special extensions for compute
};

bool isPhysicalDeviceExtensionSupportAdequate(VkPhysicalDevice device)
{
    if (requiredDeviceExtensions.size() == 0)
    {
        return true;
    }
    
    // Fetch all device extension properties.
    uint32_t extensionPropertiesCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropertiesCount, nullptr);
    
    std::vector<VkExtensionProperties> deviceExtensionProperties(extensionPropertiesCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionPropertiesCount, deviceExtensionProperties.data());
    
    // Create a set from the requiredExtensionNames vector.
    std::set<const char*> requiredExtensionSet(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
    
    // Iterate through the deviceExtensionProperties, removing device extension names from the requiredExtensionNamesSet.
    for (const auto& extension : deviceExtensionProperties)
    {
        requiredExtensionSet.erase(extension.extensionName);
    }
    
    // If requiredExtensionNamesSet is empty, then the device fully supports all required extensions.
    return requiredExtensionSet.empty();
}

std::optional<uint32_t> getComputeQueueFamilyIndex(VkPhysicalDevice physicalDevice)
{
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties.data());
    
    for (uint32_t i = 0; i < queueFamilyPropertiesCount; ++i)
    {
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            return i;
        }
        
        ++i;
    }
    
    return NULL;
}

bool isPhysicalDeviceSuitable(VkPhysicalDevice device)
{
    // TODO: Check for optimal device, not just the first one with the Compute capability.
     VkPhysicalDeviceProperties properties;
     vkGetPhysicalDeviceProperties(device, &properties);
     
     VkPhysicalDeviceFeatures features;
     vkGetPhysicalDeviceFeatures(device, &features);
  
    auto allRequiredExtensionsSupported = isPhysicalDeviceExtensionSupportAdequate(device);
    
    auto computeQueueFamilyIndex = getComputeQueueFamilyIndex(device);
    
    return allRequiredExtensionsSupported && computeQueueFamilyIndex.has_value();
}

void VulkanComputeProgram::assignPhysicalDevice()
{
    // Find out how many devices there are.
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    
    // No devices? Throw an error. :(
    if (physicalDeviceCount == 0)
    {
        throw std::runtime_error("Failed to find any GPUs with Vulkan support!");
    }
    
    // Fetch all of the physical devices
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
    
    for (const auto& device : physicalDevices)
    {
        
        // TODO: Find the most suitable device, not just the first
        if (isPhysicalDeviceSuitable(device))
        {
            physicalDevice = device;
            break;
        }
    }
    
    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
    computeQueueFamilyIndex = getComputeQueueFamilyIndex(physicalDevice).value();
}

// MARK: - Logical Device

void VulkanComputeProgram::createLogicalDevice()
{
    float queuePriority = 1.0f;
    
    VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
    
    VkPhysicalDeviceFeatures deviceFeatures{};
    
    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    
    if (VulkanDebugUtils::isValidationEnabled())
    {
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VulkanDebugUtils::validationLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = VulkanDebugUtils::validationLayers.data();
    } else
    {
        deviceCreateInfo.enabledLayerCount = 0;
    }
    
    VK_ASSERT_SUCCESS(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice),
                      "Failed to create logical device!");
    
    vkGetDeviceQueue(logicalDevice, computeQueueFamilyIndex, 0, &computeQueue);
}

void VulkanComputeProgram::destroyLogicalDevice()
{
    vkDestroyDevice(logicalDevice, nullptr);
}

// MARK: - Command Pool

void VulkanComputeProgram::createCommandPool()
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.queueFamilyIndex = computeQueueFamilyIndex;
    
    VK_ASSERT_SUCCESS(vkCreateCommandPool(logicalDevice, &createInfo, nullptr, &commandPool),
                      "Failed to create command pool!");
}

void VulkanComputeProgram::destroyCommandPool()
{
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
}

// MARK: - Command Buffers

void VulkanComputeProgram::createCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VK_ASSERT_SUCCESS(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer),
                      "Failed to allocate command buffer!");
}

void VulkanComputeProgram::destroyCommandBuffer()
{
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

// MARK: - Descriptor Pools
void VulkanComputeProgram::createDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;
    
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.maxSets = 1;
    createInfo.poolSizeCount = 1;
    createInfo.pPoolSizes = &poolSize;
    
    VK_ASSERT_SUCCESS(vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool),
                      "Failed to create descriptor pool!");
}

void VulkanComputeProgram::destroyDescriptorPool()
{
    vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
}

// MARK: - --- EPHEMERAL OBJECTS ---

// MARK: - Shader Module
void VulkanComputeProgram::createShaderModule()
{
    auto computeShaderCode = FileUtils::readFile(shaderFilePath);
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = computeShaderCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(computeShaderCode.data());
    
    VK_ASSERT_SUCCESS(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule),
                      "Failed to create shader module!");
}

void VulkanComputeProgram::destroyShaderModule()
{
    vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
}

// MARK: - Device Memory

void createBufferAndMemory(VkPhysicalDevice physicalDevice,
                           VkDevice logicalDevice,
                           VkDeviceSize bufferSize,
                           uint32_t queueFamilyIndex,
                           VkBuffer& buffer,
                           VkDeviceMemory& memory)
{
    // Create buffer
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.size = bufferSize;
    createInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &queueFamilyIndex;
    
    VK_ASSERT_SUCCESS(vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer),
                      "Failed to create input buffer!");
    
    // Fetch device memory properties.
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    VkDeviceSize memorySize;
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        auto memoryType = memoryProperties.memoryTypes[i];
        auto memoryHeap = memoryProperties.memoryHeaps[memoryType.heapIndex];
        
        if (memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            && memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            && bufferSize <= memoryHeap.size)
        {
            memoryTypeIndex = i;
            memorySize = memoryHeap.size;
        }
    }
    
    if (memoryTypeIndex == VK_MAX_MEMORY_TYPES)
    {
        throw std::runtime_error("Failed to find suitable memory!");
    }
    
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    VK_ASSERT_SUCCESS(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory),
                      "Failed to allocate device memory!");
    
    vkBindBufferMemory(logicalDevice, buffer, memory, 0);
}

void destroyBufferAndMemory(VkDevice logicalDevice, VkBuffer buffer, VkDeviceMemory memory)
{
    vkDestroyBuffer(logicalDevice, buffer, nullptr);
    vkFreeMemory(logicalDevice, memory, nullptr);
}

// MARK: - Buffers

void VulkanComputeProgram::createStorageBuffers()
{
    createBufferAndMemory(physicalDevice, logicalDevice, bufferSize, computeQueueFamilyIndex, inputBuffer, inputMemory);
    
    createBufferAndMemory(physicalDevice, logicalDevice, bufferSize, computeQueueFamilyIndex, outputBuffer, outputMemory);
}

void VulkanComputeProgram::destroyStorageBuffers()
{
    destroyBufferAndMemory(logicalDevice, inputBuffer, inputMemory);
    
    destroyBufferAndMemory(logicalDevice, outputBuffer, outputMemory);
}

// MARK: - Descriptor Set Layout
void VulkanComputeProgram::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding inputLayoutBinding{};
    inputLayoutBinding.binding = 0;
    inputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputLayoutBinding.descriptorCount = 1;
    inputLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    inputLayoutBinding.pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutBinding outputLayoutBinding{};
    outputLayoutBinding.binding = 1;
    outputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputLayoutBinding.descriptorCount = 1;
    outputLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    outputLayoutBinding.pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        inputLayoutBinding,
        outputLayoutBinding,
    };
    
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = 2;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;
    
    VK_ASSERT_SUCCESS(vkCreateDescriptorSetLayout(logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout),
                      "Failed to create descriptor set layout!");
}

void VulkanComputeProgram::destroyDescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
}

// MARK: - Pipeline Layout

void VulkanComputeProgram::createPipelineLayout()
{
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    
    VK_ASSERT_SUCCESS(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout),
                      "Failed to create pipeline layout!");
}

void VulkanComputeProgram::destroyPipelineLayout()
{
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
}

// MARK: - Compute Pipeline

void VulkanComputeProgram::createPipeline()
{
    // Create shader stage
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo{};
    pipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderStageCreateInfo.pNext = nullptr;
    pipelineShaderStageCreateInfo.flags = 0;
    pipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineShaderStageCreateInfo.module = shaderModule;
    pipelineShaderStageCreateInfo.pName = "main";
    pipelineShaderStageCreateInfo.pSpecializationInfo = nullptr;
    
    // Create pipeline
    VkComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stage = pipelineShaderStageCreateInfo;
    pipelineCreateInfo.layout = pipelineLayout;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = 0;
    
    VK_ASSERT_SUCCESS(vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline),
                      "Failed to create compute pipeline!");
}

void VulkanComputeProgram::destroyPipeline()
{
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
}

// MARK: - Descriptor Sets
void VulkanComputeProgram::updateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    
    VK_ASSERT_SUCCESS(vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet),
                      "Failed to allocate descriptor set!");
    
    // Now we need to update the descriptor sets with input/output buffer info
    
    // Input
    
    VkDescriptorBufferInfo inputBufferInfo{};
    inputBufferInfo.buffer = inputBuffer;
    inputBufferInfo.offset = 0;
    inputBufferInfo.range = VK_WHOLE_SIZE;
    
    VkWriteDescriptorSet inputWriteDescriptorSet{};
    inputWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inputWriteDescriptorSet.pNext = nullptr;
    inputWriteDescriptorSet.dstSet = descriptorSet;
    inputWriteDescriptorSet.dstBinding = 0;
    inputWriteDescriptorSet.dstArrayElement = 0;
    inputWriteDescriptorSet.descriptorCount = 1;
    inputWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inputWriteDescriptorSet.pImageInfo = nullptr;
    inputWriteDescriptorSet.pBufferInfo = &inputBufferInfo;
    inputWriteDescriptorSet.pTexelBufferView = nullptr;
    
    // Output
    
    VkDescriptorBufferInfo outputBufferInfo{};
    outputBufferInfo.buffer = outputBuffer;
    outputBufferInfo.offset = 0;
    outputBufferInfo.range = VK_WHOLE_SIZE;
    
    VkWriteDescriptorSet outputWriteDescriptorSet{};
    outputWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputWriteDescriptorSet.pNext = nullptr;
    outputWriteDescriptorSet.dstSet = descriptorSet;
    outputWriteDescriptorSet.dstBinding = 1;
    outputWriteDescriptorSet.dstArrayElement = 0;
    outputWriteDescriptorSet.descriptorCount = 1;
    outputWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputWriteDescriptorSet.pImageInfo = nullptr;
    outputWriteDescriptorSet.pBufferInfo = &outputBufferInfo;
    outputWriteDescriptorSet.pTexelBufferView = nullptr;
    
    VkWriteDescriptorSet writeDescriptorSet[2] = {
        inputWriteDescriptorSet,
        outputWriteDescriptorSet,
    };
    
    vkUpdateDescriptorSets(logicalDevice, 2, writeDescriptorSet, 0, nullptr);
}

// MARK: - Record Command Buffer

void VulkanComputeProgram::recordCommandBuffer()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    
    VK_ASSERT_SUCCESS(vkBeginCommandBuffer(commandBuffer, &beginInfo),
                      "Failed to begin command buffer!");
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
    vkCmdDispatch(commandBuffer, 32, 32, 1);
    
    VK_ASSERT_SUCCESS(vkEndCommandBuffer(commandBuffer),
                      "Failed to end command buffer!");
}

// MARK: - Submit Compute Queue

void VulkanComputeProgram::submitComputeQueue()
{
    // TODO: Use semaphores and fences
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;
    
    VK_ASSERT_SUCCESS(vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE),
                      "Failed to submit compute queue!");
    
    VK_ASSERT_SUCCESS(vkQueueWaitIdle(computeQueue),
                      "Failed to wait for compute queue idle!");
}
