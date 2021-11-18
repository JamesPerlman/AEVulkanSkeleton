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
    createSamplers();
}

// MARK: - Destructor

void VulkanComputeProgram::tearDown()
{
    destroyDescriptorSet();
    destroyPipeline();
    destroyPipelineLayout();
    destroyDescriptorSetLayout();
    destroyImageViews();
    destroyImageMemory();
    destroyImages();
    destroyCommandBuffer();
    destroySamplers();
    destroyDescriptorPool();
    destroyCommandPool();
    destroyShaderModule();
    destroyLogicalDevice();
    destroyDebugMessenger();
    destroyVulkanInstance();
}

// MARK: - Run

/*
 A good algoritm is going to be multi-threaded.
 
 The VulkanComputeProgram has multiple distinct sets of structures.
 
 Once-per-instance - Logical Device, Physical Device, Vulkan Instance, Debug Messenger, Shader Module
 
 Once-per-thread - Command Pool, Descriptor Pool
 
 Recreate on buffer size change - Pipeline, descriptors, buffers / images, command buffer
 
 computeChains {
    "${width}x${height}" : [{
        pipeline,
        descriptors,
        buffers,
        lastUsed: date (ns)
    }]
 }
 
 When running the `process` method,
 1. query computeChains by width x height
 2.
 
 */
void VulkanComputeProgram::process(ImageInfo imageInfo,
                                   std::function<void(void*)> writeInputPixels,
                                   std::function<void(void*)> readOutputPixels)
{
    textureReadWriteMutex.lock();
    
    regenerateBuffersIfNeeded(imageInfo);
    
    auto imageSize = imageInfo.size();
    
    // map input buffer memory and write pixels
    void* inputPixels;
    vkMapMemory(logicalDevice, inputImageMemory, 0, imageSize, 0, &inputPixels);
    writeInputPixels(inputPixels);
    vkUnmapMemory(logicalDevice, inputImageMemory);
    
    // submit the compute queue and run the shader
    createCommandBuffer();
    recordCommandBuffer();
    submitComputeQueue();
    destroyCommandBuffer();
    
    // map outbut buffer memory and read pixels
    void* outputPixels;
    vkMapMemory(logicalDevice, outputImageMemory, 0, imageSize, 0, &outputPixels);
    readOutputPixels(outputPixels);
    vkUnmapMemory(logicalDevice, outputImageMemory);
    
    textureReadWriteMutex.unlock();
}

// Set or reset GPU memory if needed

void VulkanComputeProgram::regenerateBuffersIfNeeded(ImageInfo imageInfo)
{
    if (imageInfo.width != this->imageInfo.width || imageInfo.height != this->imageInfo.height)
    {
        this->imageInfo = imageInfo;
        
        // destroy objects that need to be recreated
        destroyPipeline();
        destroyPipelineLayout();
        destroyDescriptorSet();
        destroyDescriptorSetLayout();
        destroyImageViews();
        destroyImageMemory();
        destroyImages();
        
        
        // recreate objects
        createImages();
        createImageMemory();
        bindImageMemory();
        createImageViews();
        createDescriptorSetLayout();
        createDescriptorSet();
        createPipelineLayout();
        createPipeline();
        
        // prepare for computations
        updateDescriptorSet();
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
    VkDescriptorPoolSize inputPoolSize{};
    inputPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    inputPoolSize.descriptorCount = 1;
    
    VkDescriptorPoolSize outputPoolSize{};
    outputPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputPoolSize.descriptorCount = 1;
    
    VkDescriptorPoolSize poolSizes[2] = {
        inputPoolSize,
        outputPoolSize,
    };
    
    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    createInfo.maxSets = 1;
    createInfo.poolSizeCount = 2;
    createInfo.pPoolSizes = poolSizes;
    
    VK_ASSERT_SUCCESS(vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool),
                      "Failed to create descriptor pool!");
}

void VulkanComputeProgram::destroyDescriptorPool()
{
    vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
}

// MARK: - Samplers

void createSampler(VkDevice logicalDevice, VkFilter filter, VkSampler& sampler)
{
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.magFilter = filter;
    createInfo.minFilter = filter;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    createInfo.mipLodBias = 0.0f;
    createInfo.anisotropyEnable = VK_FALSE;
    createInfo.maxAnisotropy = 1.0f;
    createInfo.compareEnable = VK_FALSE;
    createInfo.compareOp = VK_COMPARE_OP_NEVER;
    createInfo.minLod = 0.0f;
    createInfo.maxLod = 0.0f;
    createInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    createInfo.unnormalizedCoordinates = VK_FALSE;
    
    VK_ASSERT_SUCCESS(vkCreateSampler(logicalDevice, &createInfo, nullptr, &sampler),
                      "Failed to create sampler!");
}

void VulkanComputeProgram::createSamplers()
{
    createSampler(logicalDevice, VK_FILTER_LINEAR, inputSampler);
    createSampler(logicalDevice, VK_FILTER_NEAREST, outputSampler);
}

void VulkanComputeProgram::destroySamplers()
{
    vkDestroySampler(logicalDevice, inputSampler, nullptr);
    vkDestroySampler(logicalDevice, outputSampler, nullptr);
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
                           VkBufferUsageFlags usageFlags,
                           VkMemoryPropertyFlags memoryFlags,
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
    createInfo.usage = usageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 1;
    createInfo.pQueueFamilyIndices = &queueFamilyIndex;
    
    VK_ASSERT_SUCCESS(vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer),
                      "Failed to create input buffer!");
    
    // Fetch device memory properties.
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    VkDeviceSize availableMemorySize;
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        auto memoryType = memoryProperties.memoryTypes[i];
        auto memoryHeap = memoryProperties.memoryHeaps[memoryType.heapIndex];
        
        if ((memoryType.propertyFlags & memoryFlags) == memoryFlags
            && bufferSize <= memoryHeap.size)
        {
            memoryTypeIndex = i;
            availableMemorySize = memoryHeap.size;
        }
    }
    
    if (memoryTypeIndex == VK_MAX_MEMORY_TYPES)
    {
        throw std::runtime_error("Failed to find suitable memory!");
    }
    
    // TODO: Here we should check if availableMemorySize is greater than the amount allocated so far plus the amount we're about to allocate.
    // If there is not enough memory, we will free up some resources
    
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

// MARK: - Image & Memory

VkFormat getImageFormat(ImageInfo imageInfo)
{
    // TODO: sRGB or uint?
    switch (imageInfo.pixelFormat)
    {
        case PixelFormat::ARGB32:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case PixelFormat::ARGB64:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case PixelFormat::ARGB128:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

VkExtent3D getImageExtent(ImageInfo imageInfo)
{
    VkExtent3D extent{};
    extent.width = imageInfo.width;
    extent.height = imageInfo.height;
    extent.depth = 1;
    
    return extent;
}

void createImage(VkDevice logicalDevice,
                 ImageInfo imageInfo,
                 VkImageUsageFlags usageFlags,
                 VkImage& image)
{
    // fetch some image properties
    auto imageFormat = getImageFormat(imageInfo);
    auto imageExtent = getImageExtent(imageInfo);
    
    // Create buffer
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.format = imageFormat;
    createInfo.extent = imageExtent;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = VK_IMAGE_TILING_LINEAR;
    createInfo.usage = usageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    
    VK_ASSERT_SUCCESS(vkCreateImage(logicalDevice, &createInfo, nullptr, &image),
                      "Failed to create image!");
}

// MARK: - Images

void VulkanComputeProgram::createImages()
{
    // create input image
    createImage(logicalDevice, imageInfo, VK_IMAGE_USAGE_SAMPLED_BIT, inputImage);
    
    // create output image
    createImage(logicalDevice, imageInfo, VK_IMAGE_USAGE_STORAGE_BIT, outputImage);
}

void VulkanComputeProgram::destroyImages()
{
    vkDestroyImage(logicalDevice, inputImage, nullptr);
    vkDestroyImage(logicalDevice, outputImage, nullptr);
}

// MARK: - Image Memory


void allocateImageMemory(VkPhysicalDevice physicalDevice,
                         VkDevice logicalDevice,
                         ImageInfo imageInfo,
                         VkMemoryPropertyFlags propertyFlags,
                         VkImage& image,
                         VkDeviceMemory& memory)
{
    auto imageSize = static_cast<VkDeviceSize>(imageInfo.size());
    
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);
    
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    VkDeviceSize availableMemorySize;
    uint32_t memoryTypeIndex = VK_MAX_MEMORY_TYPES;
    
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        auto memoryType = memoryProperties.memoryTypes[i];
        auto memoryHeap = memoryProperties.memoryHeaps[memoryType.heapIndex];
        
        if ((memoryRequirements.memoryTypeBits & (1 << i))
            && (memoryType.propertyFlags & propertyFlags) == propertyFlags
            && imageSize <= memoryHeap.size)
        {
            memoryTypeIndex = i;
            availableMemorySize = memoryHeap.size;
            break;
        }
    }
    
    if (memoryTypeIndex == VK_MAX_MEMORY_TYPES)
    {
        throw std::runtime_error("Failed to find suitable memory!");
    }
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    VK_ASSERT_SUCCESS(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory),
                      "Failed to allocate device memory!");
}

void VulkanComputeProgram::createImageMemory()
{
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    
    allocateImageMemory(physicalDevice, logicalDevice, imageInfo, memoryFlags, inputImage, inputImageMemory);
    allocateImageMemory(physicalDevice, logicalDevice, imageInfo, memoryFlags, outputImage, outputImageMemory);
}

void VulkanComputeProgram::destroyImageMemory()
{
    vkFreeMemory(logicalDevice, inputImageMemory, nullptr);
    vkFreeMemory(logicalDevice, outputImageMemory, nullptr);
}



// MARK: - Bind Image Memory

void VulkanComputeProgram::bindImageMemory()
{
    vkBindImageMemory(logicalDevice, inputImage, inputImageMemory, 0);
    vkBindImageMemory(logicalDevice, outputImage, outputImageMemory, 0);
}

// MARK: - Image Views
void createImageView(VkDevice logicalDevice, VkFormat format, VkImage& image, VkImageView& imageView)
{
    // TODO: We probably need to swizzle this because AE might use ARGB while Vulkan uses RGBA
    VkComponentMapping componentMapping{};
    componentMapping.a = VK_COMPONENT_SWIZZLE_A;
    componentMapping.r = VK_COMPONENT_SWIZZLE_R;
    componentMapping.g = VK_COMPONENT_SWIZZLE_G;
    componentMapping.b = VK_COMPONENT_SWIZZLE_B;
    
    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel   = 0;
    subresourceRange.levelCount     = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount     = 1;
    
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components = componentMapping;
    createInfo.subresourceRange = subresourceRange;
    
    VK_ASSERT_SUCCESS(vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageView),
                      "Failed to create image view!");
}

void VulkanComputeProgram::createImageViews()
{
    VkFormat format = getImageFormat(imageInfo);
    
    createImageView(logicalDevice, format, inputImage, inputImageView);
    createImageView(logicalDevice, format, outputImage, outputImageView);
}

void VulkanComputeProgram::destroyImageViews()
{
    vkDestroyImageView(logicalDevice, inputImageView, nullptr);
    vkDestroyImageView(logicalDevice, outputImageView, nullptr);
}

// MARK: - Descriptor Set Layout
void VulkanComputeProgram::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding inputLayoutBinding{};
    inputLayoutBinding.binding = 0;
    inputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    inputLayoutBinding.descriptorCount = 1;
    inputLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    inputLayoutBinding.pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutBinding outputLayoutBinding{};
    outputLayoutBinding.binding = 1;
    outputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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

// MARK: - Descriptor Set

void VulkanComputeProgram::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    
    VK_ASSERT_SUCCESS(vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet),
                      "Failed to allocate descriptor set!");
}

void VulkanComputeProgram::destroyDescriptorSet()
{
    vkFreeDescriptorSets(logicalDevice, descriptorPool, 1, &descriptorSet);
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

// MARK: - Update Descriptor Set

void VulkanComputeProgram::updateDescriptorSet()
{
    
    // update the descriptor sets with input/output buffer info
    
    // Input
    
    VkDescriptorImageInfo inputImageInfo{};
    inputImageInfo.sampler = inputSampler;
    inputImageInfo.imageView = inputImageView;
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    
    VkWriteDescriptorSet inputWriteDescriptorSet{};
    inputWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inputWriteDescriptorSet.pNext = nullptr;
    inputWriteDescriptorSet.dstSet = descriptorSet;
    inputWriteDescriptorSet.dstBinding = 0;
    inputWriteDescriptorSet.dstArrayElement = 0;
    inputWriteDescriptorSet.descriptorCount = 1;
    inputWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    inputWriteDescriptorSet.pImageInfo = &inputImageInfo;
    inputWriteDescriptorSet.pBufferInfo = nullptr;
    inputWriteDescriptorSet.pTexelBufferView = nullptr;
    
    // Output
    
    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.sampler = outputSampler;
    outputImageInfo.imageView = outputImageView;
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    
    VkWriteDescriptorSet outputWriteDescriptorSet{};
    outputWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputWriteDescriptorSet.pNext = nullptr;
    outputWriteDescriptorSet.dstSet = descriptorSet;
    outputWriteDescriptorSet.dstBinding = 1;
    outputWriteDescriptorSet.dstArrayElement = 0;
    outputWriteDescriptorSet.descriptorCount = 1;
    outputWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputWriteDescriptorSet.pImageInfo = &outputImageInfo;
    outputWriteDescriptorSet.pBufferInfo = nullptr;
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
    
    vkCmdDispatch(commandBuffer, imageInfo.width, imageInfo.height, 1);
    
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
