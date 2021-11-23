//
//  VulkanUtils.cpp
//  VkSkeleton
//
//  Created by James Perlman on 11/22/21.
//

#include "VulkanUtils.hpp"

using namespace VulkanUtils;

// MARK: - Sampler

void VulkanUtils::createSampler(VkDevice logicalDevice, VkFilter filter, VkSampler& sampler)
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

// MARK: - Buffers

void VulkanUtils::createBuffer(VkPhysicalDevice physicalDevice,
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

// MARK: - Buffer Memory

void VulkanUtils::allocateBufferMemory(VkPhysicalDevice physicalDevice,
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

// MARK: - Images

VkFormat VulkanUtils::getImageFormat(ImageInfo imageInfo)
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

VkExtent3D VulkanUtils::getImageExtent(ImageInfo imageInfo)
{
    return {
        .width = imageInfo.width,
        .height = imageInfo.height,
        .depth = 1,
    };
}

void VulkanUtils::createImage(VkDevice logicalDevice,
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

// MARK: - Image Memory

void VulkanUtils::allocateImageMemory(VkPhysicalDevice physicalDevice,
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

// MARK: - Image Views

void VulkanUtils::createImageView(VkDevice logicalDevice, VkFormat format, VkImage& image, VkImageView& imageView)
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

// Returns the nearest power-of-two greater than or equal to x
uint32_t potGTE(uint32_t x)
{
    for (char i = 0; i < 32; ++i)
    {
        // if (x shifted right by i) is the rightmost bit...
        if ((x >> i) == 1)
        {
            // then we know i is the greatest power of two less than or equal to x
            if (1 << i == x) {
                // this covers the equal-to case
                return x;
            } else {
                // if ((1 << i) != x), we can assume (1 << i) is less than x
                // so we just return the next power of two
                return 1 << (i + 1);
            }
        }
    }
    
    // x must be zero
    return 1;
}
