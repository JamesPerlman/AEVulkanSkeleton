//
//  VulkanComputeDataTypes.h
//  VkSkeleton
//
//  Created by James Perlman on 10/31/21.
//

#ifndef VulkanComputeDataTypes_h
#define VulkanComputeDataTypes_h

enum PixelFormat : size_t {
    ARGB32 = 32,
    ARGB16 = 16,
    ARGB8  = 8,
};

struct ImageData {
    uint32_t width;
    uint32_t height;
    PixelFormat pixelFormat;
    void* data;
    
    size_t size() {
        return static_cast<size_t>(pixelFormat) * static_cast<size_t>(width * height);
    }
};

#endif /* VulkanComputeDataTypes_h */
