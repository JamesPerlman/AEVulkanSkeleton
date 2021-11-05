//
//  VulkanComputeDataTypes.h
//  VkSkeleton
//
//  Created by James Perlman on 10/31/21.
//

#ifndef VulkanComputeDataTypes_h
#define VulkanComputeDataTypes_h

#include <map>

enum PixelFormat : size_t {
    ARGB128 = 32,
    ARGB64  = 16,
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

#endif /* VulkanComputeDataTypes_h */
