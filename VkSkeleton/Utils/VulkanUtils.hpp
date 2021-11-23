//
//  VulkanUtils.hpp
//  VkSkeleton
//
//  Created by James Perlman on 11/22/21.
//

#ifndef VulkanUtils_hpp
#define VulkanUtils_hpp

#include <vulkan/vulkan.h>
#include "VulkanComputeDataTypes.hpp"

#define VK_ASSERT_SUCCESS(result, message) if (result != VK_SUCCESS) { throw std::runtime_error(message); }

namespace VulkanUtils {

void createSampler(VkDevice logicalDevice, VkFilter filter, VkSampler& sampler);

void createBuffer(VkPhysicalDevice physicalDevice,
                  VkDevice logicalDevice,
                  VkDeviceSize bufferSize,
                  VkBufferUsageFlags usageFlags,
                  uint32_t queueFamilyIndex,
                  VkBuffer& buffer);

void allocateBufferMemory(VkPhysicalDevice physicalDevice,
                          VkDevice logicalDevice,
                          VkDeviceSize bufferSize,
                          VkMemoryPropertyFlags propertyFlags,
                          VkBuffer& buffer,
                          VkDeviceMemory& memory);

VkFormat getImageFormat(ImageInfo imageInfo);

VkExtent3D getImageExtent(ImageInfo imageInfo);

void createImage(VkDevice logicalDevice,
                 ImageInfo imageInfo,
                 VkImageUsageFlags usageFlags,
                 VkImage& image);

void allocateImageMemory(VkPhysicalDevice physicalDevice,
                         VkDevice logicalDevice,
                         ImageInfo imageInfo,
                         VkMemoryPropertyFlags propertyFlags,
                         VkImage& image,
                         VkDeviceMemory& memory);

void createImageView(VkDevice logicalDevice, VkFormat format, VkImage& image, VkImageView& imageView);

uint32_t potGTE(uint32_t x);

}

#endif /* VulkanUtils_hpp */
