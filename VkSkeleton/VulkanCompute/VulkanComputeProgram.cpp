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
    destroySamplers();
    destroyDescriptorPool();
    destroyCommandPool();
    destroyShaderModule();
    destroyLogicalDevice();
    destroyDebugMessenger();
    destroyVulkanInstance();
}

// MARK: - Run

// TODO: Multi-thread the crap outta this.

void VulkanComputeProgram::process(ImageInfo imageInfo,
                                   std::function<void(void*)> writeInputPixels,
                                   std::function<void(void*)> readOutputPixels)
{
    textureReadWriteMutex.lock();
    
    regenerateBuffersIfNeeded(imageInfo);
    
    auto imageSize = imageInfo.size();
    
    // write input image memory
    void* inputPixels;
    vkMapMemory(logicalDevice, inputBufferMemory, 0, imageSize, 0, &inputPixels);
    writeInputPixels(inputPixels);
    vkUnmapMemory(logicalDevice, inputBufferMemory);
    
    copyInputBufferToImage();
    
    // submit the compute queue and run the shader
    executeShader();
    
    copyOutputImageToBuffer();
    
    // map outbut buffer memory and read pixels
    void* outputPixels;
    vkMapMemory(logicalDevice, outputBufferMemory, 0, imageSize, 0, &outputPixels);
    readOutputPixels(outputPixels);
    vkUnmapMemory(logicalDevice, outputBufferMemory);
    
    textureReadWriteMutex.unlock();
}

// Set or reset GPU memory if needed

void VulkanComputeProgram::regenerateBuffersIfNeeded(ImageInfo imageInfo)
{
    if (imageInfo.width != this->imageInfo.width
        || imageInfo.height != this->imageInfo.height
        || imageInfo.pixelFormat != this->imageInfo.pixelFormat)
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
        destroyBufferMemory();
        destroyBuffers();
        
        
        // recreate objects
        createBuffers();
        createBufferMemory();
        bindBufferMemory();
        createImages();
        createImageMemory();
        bindImageMemory();
        createImageViews();
        createDescriptorSetLayout();
        createDescriptorSet();
        createPipelineLayout();
        createPipeline();
        
        // prepare for computations
        transitionImageLayouts();
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
    
    VkApplicationInfo appInfo {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };
    
    auto requiredExtensionNames = getRequiredInstanceExtensionNames();
    
    VkInstanceCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredExtensionNames.size()),
        .ppEnabledExtensionNames = requiredExtensionNames.data(),
    };
    
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
    
    VkDeviceQueueCreateInfo deviceQueueCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = computeQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    
    VkPhysicalDeviceFeatures deviceFeatures {
        .shaderStorageImageWriteWithoutFormat = VK_TRUE,
    };
    
    VkDeviceCreateInfo deviceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .queueCreateInfoCount = 1,
        .pEnabledFeatures = &deviceFeatures,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };
    
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
    VkCommandPoolCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = computeQueueFamilyIndex,
    };
    
    VK_ASSERT_SUCCESS(vkCreateCommandPool(logicalDevice, &createInfo, nullptr, &commandPool),
                      "Failed to create command pool!");
}

void VulkanComputeProgram::destroyCommandPool()
{
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
}

// MARK: - Descriptor Pools
void VulkanComputeProgram::createDescriptorPool()
{
    VkDescriptorPoolSize inputPoolSize {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    
    VkDescriptorPoolSize outputPoolSize {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
    };
    
    VkDescriptorPoolSize poolSizes[2] = {
        inputPoolSize,
        outputPoolSize,
    };
    
    VkDescriptorPoolCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes,
    };
    
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
    VkSamplerCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };
    
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
    VkShaderModuleCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = computeShaderCode.size(),
        .pCode = reinterpret_cast<const uint32_t*>(computeShaderCode.data()),
    };
    
    VK_ASSERT_SUCCESS(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule),
                      "Failed to create shader module!");
}

void VulkanComputeProgram::destroyShaderModule()
{
    vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
}

// MARK: - Buffers

void createBuffer(VkPhysicalDevice physicalDevice,
                  VkDevice logicalDevice,
                  VkDeviceSize bufferSize,
                  VkBufferUsageFlags usageFlags,
                  uint32_t queueFamilyIndex,
                  VkBuffer& buffer)
{
    // Create buffer
    VkBufferCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = bufferSize,
        .usage = usageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueFamilyIndex,
    };
    
    VK_ASSERT_SUCCESS(vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer),
                      "Failed to create buffer!");
}

void VulkanComputeProgram::createBuffers()
{
    auto bufferSize = static_cast<VkDeviceSize>(imageInfo.size());
    
    createBuffer(physicalDevice,
                 logicalDevice,
                 bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 computeQueueFamilyIndex,
                 inputBuffer);
    
    createBuffer(physicalDevice,
                 logicalDevice,
                 bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 computeQueueFamilyIndex,
                 outputBuffer);
}

void VulkanComputeProgram::destroyBuffers()
{
    vkDestroyBuffer(logicalDevice, inputBuffer, nullptr);
    vkDestroyBuffer(logicalDevice, outputBuffer, nullptr);
}

// MARK: - Buffer Memory

void allocateBufferMemory(VkPhysicalDevice physicalDevice,
                          VkDevice logicalDevice,
                          VkDeviceSize bufferSize,
                          VkMemoryPropertyFlags propertyFlags,
                          VkBuffer& buffer,
                          VkDeviceMemory& memory)
{
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);
    
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
            && bufferSize <= memoryHeap.size)
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
    
    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    
    VK_ASSERT_SUCCESS(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory),
                      "Failed to allocate device memory!");
}

void VulkanComputeProgram::createBufferMemory()
{
    auto memorySize = static_cast<VkDeviceSize>(imageInfo.size());
    
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    allocateBufferMemory(physicalDevice,
                         logicalDevice,
                         memorySize,
                         memoryFlags,
                         inputBuffer,
                         inputBufferMemory);
    
    allocateBufferMemory(physicalDevice,
                         logicalDevice,
                         memorySize,
                         memoryFlags,
                         outputBuffer,
                         outputBufferMemory);
}

void VulkanComputeProgram::destroyBufferMemory()
{
    vkFreeMemory(logicalDevice, inputBufferMemory, nullptr);
    vkFreeMemory(logicalDevice, outputBufferMemory, nullptr);
}

// MARK: - Bind Buffer Memory

void VulkanComputeProgram::bindBufferMemory()
{
    vkBindBufferMemory(logicalDevice, inputBuffer, inputBufferMemory, 0);
    vkBindBufferMemory(logicalDevice, outputBuffer, outputBufferMemory, 0);
}

// MARK: - Images

VkFormat getImageFormat(ImageInfo imageInfo)
{
    // TODO: sRGB or uint?
    switch (imageInfo.pixelFormat)
    {
        case PixelFormat::ARGB32:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::ARGB64:
            return VK_FORMAT_R16G16B16A16_UNORM;
        case PixelFormat::ARGB128:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

VkExtent3D getImageExtent(ImageInfo imageInfo)
{
    return {
        .width = imageInfo.width,
        .height = imageInfo.height,
        .depth = 1,
    };
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
    VkImageCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = imageFormat,
        .extent = imageExtent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    
    VK_ASSERT_SUCCESS(vkCreateImage(logicalDevice, &createInfo, nullptr, &image),
                      "Failed to create image!");
}

void VulkanComputeProgram::createImages()
{
    // create input image
    createImage(logicalDevice,
                imageInfo,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                inputImage);
    
    // create output image
    createImage(logicalDevice,
                imageInfo,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                outputImage);
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
    
    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    
    VK_ASSERT_SUCCESS(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &memory),
                      "Failed to allocate device memory!");
}

void VulkanComputeProgram::createImageMemory()
{
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
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
    VkImageViewCreateInfo createInfo {
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        createInfo.pNext = nullptr,
        createInfo.flags = 0,
        createInfo.image = image,
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D,
        createInfo.format = format,
        createInfo.components = {
            // TODO: We might need to swizzle
            .a = VK_COMPONENT_SWIZZLE_A,
            .r = VK_COMPONENT_SWIZZLE_R,
            .g = VK_COMPONENT_SWIZZLE_G,
            .b = VK_COMPONENT_SWIZZLE_B,
        },
        createInfo.subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };
    
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
    VkDescriptorSetLayoutBinding inputLayoutBinding {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr,
    };
    
    VkDescriptorSetLayoutBinding outputLayoutBinding {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr,
    };
    
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
        inputLayoutBinding,
        outputLayoutBinding,
    };
    
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 2,
        .pBindings = descriptorSetLayoutBindings,
    };
    
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
    VkDescriptorSetAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };
    
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
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    
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
    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };
    
    // Create pipeline
    VkComputePipelineCreateInfo pipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = pipelineShaderStageCreateInfo,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    
    VK_ASSERT_SUCCESS(vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline),
                      "Failed to create compute pipeline!");
}

void VulkanComputeProgram::destroyPipeline()
{
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
}

// MARK: - Transition Image Layouts

void VulkanComputeProgram::transitionImageLayout(VkImage& image, ImageLayoutTransitionInfo transitionInfo)
{
    submitComputeQueue([&](VkCommandBuffer& commandBuffer) {
        VkImageMemoryBarrier barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = transitionInfo.srcAccessMask,
            .dstAccessMask = transitionInfo.dstAccessMask,
            .oldLayout = transitionInfo.oldLayout,
            .newLayout = transitionInfo.newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        
        vkCmdPipelineBarrier(commandBuffer,
                             transitionInfo.srcStageMask,
                             transitionInfo.dstStageMask,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
    });
}

void VulkanComputeProgram::transitionImageLayouts()
{
    // transition input image to shader readable
    transitionImageLayout(inputImage, {
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_NONE_KHR,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    });
    
    // transition output image to shader writeable
    transitionImageLayout(outputImage, {
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_NONE_KHR,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    });
}

// MARK: - Copy buffer to image
void VulkanComputeProgram::copyInputBufferToImage()
{
    transitionImageLayout(inputImage, {
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    });
    
    submitComputeQueue([&](VkCommandBuffer& commandBuffer) {
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {
                imageInfo.width,
                imageInfo.height,
                1,
            },
        };
        
        vkCmdCopyBufferToImage(commandBuffer,
                               inputBuffer,
                               inputImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &region);
        
    });
    
    transitionImageLayout(inputImage, {
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    });
}
// MARK: - Copy image to buffer

void VulkanComputeProgram::copyOutputImageToBuffer()
{
    transitionImageLayout(outputImage, {
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    });
    
    submitComputeQueue([&](VkCommandBuffer& commandBuffer) {
        VkBufferImageCopy region = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {
                imageInfo.width,
                imageInfo.height,
                1,
            },
        };
        
        vkCmdCopyImageToBuffer(commandBuffer,
                               outputImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               outputBuffer,
                               1,
                               &region);
        
    });
    
    transitionImageLayout(outputImage, {
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
    });
}
// MARK: - Update Descriptor Set

void VulkanComputeProgram::updateDescriptorSet()
{
    // update the descriptor sets with input/output buffer info
    
    // Input
    
    VkDescriptorImageInfo inputImageInfo {
        .sampler = inputSampler,
        .imageView = inputImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    
    VkWriteDescriptorSet inputWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &inputImageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    
    // Output
    
    VkDescriptorImageInfo outputImageInfo {
        .sampler = outputSampler,
        .imageView = outputImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    
    VkWriteDescriptorSet outputWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &outputImageInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };
    
    VkWriteDescriptorSet writeDescriptorSet[2] = {
        inputWriteDescriptorSet,
        outputWriteDescriptorSet,
    };
    
    vkUpdateDescriptorSets(logicalDevice, 2, writeDescriptorSet, 0, nullptr);
}


// MARK: - Submit Compute Queue

VkCommandBuffer createCommandBuffer(VkDevice& logicalDevice, VkCommandPool& commandPool)
{
    VkCommandBufferAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    
    VkCommandBuffer commandBuffer;
    VK_ASSERT_SUCCESS(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer),
                      "Failed to allocate command buffer!");
    
    return commandBuffer;
}

void destroyCommandBuffer(VkDevice& logicalDevice, VkCommandPool& commandPool, VkCommandBuffer& commandBuffer)
{
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void VulkanComputeProgram::submitComputeQueue(std::function<void(VkCommandBuffer&)> recordCommands)
{
    auto commandBuffer = createCommandBuffer(logicalDevice, commandPool);
    
    // Begin, record, and end command buffer.
    
    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    
    VK_ASSERT_SUCCESS(vkBeginCommandBuffer(commandBuffer, &beginInfo),
                      "Failed to begin command buffer!");
    
    recordCommands(commandBuffer);
    
    VK_ASSERT_SUCCESS(vkEndCommandBuffer(commandBuffer),
                      "Failed to end command buffer!");
    
    // Submit compute queue
    
    // TODO: Use semaphores and fences
    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr,
    };
    
    VK_ASSERT_SUCCESS(vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE),
                      "Failed to submit compute queue!");
    
    VK_ASSERT_SUCCESS(vkQueueWaitIdle(computeQueue),
                      "Failed to wait for compute queue idle!");
    
    // Cleanup
    destroyCommandBuffer(logicalDevice, commandPool, commandBuffer);
}

// MARK: - Execute Shader

void VulkanComputeProgram::executeShader()
{
    submitComputeQueue([&](VkCommandBuffer& commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, imageInfo.width, imageInfo.height, 1);
    });
}
