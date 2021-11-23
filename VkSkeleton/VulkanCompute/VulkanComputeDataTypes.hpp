//
//  VulkanComputeDataTypes.h
//  VkSkeleton
//
//  Created by James Perlman on 10/31/21.
//

#ifndef VulkanComputeDataTypes_h
#define VulkanComputeDataTypes_h

#include <map>
#include <vulkan/vulkan.h>

enum PixelFormat : size_t {
    ARGB128 = 16,
    ARGB64  = 8,
    ARGB32  = 4,
};

struct ImageInfo {
    uint32_t width;
    uint32_t height;
    PixelFormat pixelFormat;
    
    size_t size() {
        return static_cast<size_t>(pixelFormat) * static_cast<size_t>(width * height);
    }
};

struct ImageLayoutTransitionInfo {
    VkImageLayout oldLayout, newLayout;
    VkAccessFlags srcAccessMask, dstAccessMask;
    VkPipelineStageFlags srcStageMask, dstStageMask;
};

struct UniformBufferObject {
    float pivot;
};

#endif /* VulkanComputeDataTypes_h */
