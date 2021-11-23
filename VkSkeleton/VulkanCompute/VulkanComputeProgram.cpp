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
#include "VulkanUtils.hpp"

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
    createUniformBuffer();
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
    destroyUniformBuffer();
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
                                   UniformBufferObject uniformBufferObject,
                                   std::function<void(void*)> writeInputPixels,
                                   std::function<void(void*)> readOutputPixels)
{
    textureReadWriteMutex.lock();
    
    regenerateImageBuffersIfNeeded(imageInfo);
    updateUniformBuffer(uniformBufferObject);
    
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

void VulkanComputeProgram::regenerateImageBuffersIfNeeded(ImageInfo imageInfo)
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
        destroyImageBufferMemory();
        destroyImageBuffers();
        
        // recreate objects
        createImageBuffers();
        createImageBufferMemory();
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

// MARK: - Update Uniform Buffer Object
void VulkanComputeProgram::updateUniformBuffer(UniformBufferObject uniformBufferObject)
{
    auto bufferSize = sizeof(uniformBufferObject);
    
    void* uniformBufferData;
    vkMapMemory(logicalDevice,
                uniformBufferMemory,
                0,
                bufferSize,
                0,
                &uniformBufferData);
    
    memcpy(uniformBufferData, &uniformBufferObject, bufferSize);
    
    vkUnmapMemory(logicalDevice, uniformBufferMemory);
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
    
    VkDescriptorPoolSize uniformPoolSize {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };
    
    VkDescriptorPoolSize poolSizes[3] = {
        inputPoolSize,
        outputPoolSize,
        uniformPoolSize,
    };
    
    VkDescriptorPoolCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = 3,
        .pPoolSizes = poolSizes,
    };
    
    VK_ASSERT_SUCCESS(vkCreateDescriptorPool(logicalDevice,
                                             &createInfo,
                                             nullptr,
                                             &descriptorPool),
                      "Failed to create descriptor pool!");
}

void VulkanComputeProgram::destroyDescriptorPool()
{
    vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
}

// MARK: - Samplers

void VulkanComputeProgram::createSamplers()
{
    VulkanUtils::createSampler(logicalDevice, VK_FILTER_LINEAR, inputSampler);
    VulkanUtils::createSampler(logicalDevice, VK_FILTER_NEAREST, outputSampler);
}

void VulkanComputeProgram::destroySamplers()
{
    vkDestroySampler(logicalDevice, inputSampler, nullptr);
    vkDestroySampler(logicalDevice, outputSampler, nullptr);
}

// MARK: - Uniform Buffers

void VulkanComputeProgram::createUniformBuffer()
{
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    
    VulkanUtils::createBuffer(physicalDevice,
                              logicalDevice,
                              bufferSize,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              computeQueueFamilyIndex,
                              uniformBuffer);
    
    VulkanUtils::allocateBufferMemory(physicalDevice,
                                      logicalDevice,
                                      bufferSize,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      uniformBuffer,
                                      uniformBufferMemory);
    
    vkBindBufferMemory(logicalDevice,
                       uniformBuffer,
                       uniformBufferMemory,
                       0);
}

void VulkanComputeProgram::destroyUniformBuffer()
{
    vkFreeMemory(logicalDevice,
                 uniformBufferMemory,
                 nullptr);
    
    vkDestroyBuffer(logicalDevice,
                    uniformBuffer,
                    nullptr);
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
    
    VK_ASSERT_SUCCESS(vkCreateShaderModule(logicalDevice,
                                           &createInfo,
                                           nullptr,
                                           &shaderModule),
                      "Failed to create shader module!");
}

void VulkanComputeProgram::destroyShaderModule()
{
    vkDestroyShaderModule(logicalDevice, shaderModule, nullptr);
}

// MARK: - Image Buffers

void VulkanComputeProgram::createImageBuffers()
{
    auto bufferSize = static_cast<VkDeviceSize>(imageInfo.size());
    
    VulkanUtils::createBuffer(physicalDevice,
                              logicalDevice,
                              bufferSize,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              computeQueueFamilyIndex,
                              inputBuffer);
    
    VulkanUtils::createBuffer(physicalDevice,
                              logicalDevice,
                              bufferSize,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              computeQueueFamilyIndex,
                              outputBuffer);
}

void VulkanComputeProgram::destroyImageBuffers()
{
    vkDestroyBuffer(logicalDevice, inputBuffer, nullptr);
    vkDestroyBuffer(logicalDevice, outputBuffer, nullptr);
}

// MARK: - Image Buffer Memory

void VulkanComputeProgram::createImageBufferMemory()
{
    auto memorySize = static_cast<VkDeviceSize>(imageInfo.size());
    
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    VulkanUtils::allocateBufferMemory(physicalDevice,
                                      logicalDevice,
                                      memorySize,
                                      memoryFlags,
                                      inputBuffer,
                                      inputBufferMemory);
    
    VulkanUtils::allocateBufferMemory(physicalDevice,
                                      logicalDevice,
                                      memorySize,
                                      memoryFlags,
                                      outputBuffer,
                                      outputBufferMemory);
}

void VulkanComputeProgram::destroyImageBufferMemory()
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

void VulkanComputeProgram::createImages()
{
    // create input image
    VulkanUtils::createImage(logicalDevice,
                             imageInfo,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             inputImage);
    
    // create output image
    VulkanUtils::createImage(logicalDevice,
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

void VulkanComputeProgram::createImageMemory()
{
    VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    VulkanUtils::allocateImageMemory(physicalDevice, logicalDevice, imageInfo, memoryFlags, inputImage, inputImageMemory);
    VulkanUtils::allocateImageMemory(physicalDevice, logicalDevice, imageInfo, memoryFlags, outputImage, outputImageMemory);
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

void VulkanComputeProgram::createImageViews()
{
    VkFormat format = VulkanUtils::getImageFormat(imageInfo);
    
    VulkanUtils::createImageView(logicalDevice,
                                 format,
                                 inputImage,
                                 inputImageView);
    
    VulkanUtils::createImageView(logicalDevice,
                                 format,
                                 outputImage,
                                 outputImageView);
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
    
    VkDescriptorSetLayoutBinding uniformLayoutBinding {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr,
    };
    
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[3] = {
        inputLayoutBinding,
        outputLayoutBinding,
        uniformLayoutBinding,
    };
    
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 3,
        .pBindings = descriptorSetLayoutBindings,
    };
    
    VK_ASSERT_SUCCESS(vkCreateDescriptorSetLayout(logicalDevice,
                                                  &descriptorSetLayoutCreateInfo,
                                                  nullptr,
                                                  &descriptorSetLayout),
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
    
    // Uniforms
    
    VkDescriptorBufferInfo uniformBufferInfo {
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeof(UniformBufferObject),
    };
    
    VkWriteDescriptorSet uniformWriteDescriptorSet {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &uniformBufferInfo,
        .pTexelBufferView = nullptr,
    };
    
    VkWriteDescriptorSet writeDescriptorSet[3] = {
        inputWriteDescriptorSet,
        outputWriteDescriptorSet,
        uniformWriteDescriptorSet,
    };
    
    vkUpdateDescriptorSets(logicalDevice, 3, writeDescriptorSet, 0, nullptr);
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
