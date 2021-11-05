//
//  VulkanDebugger.hpp
//  VkComputeTest
//
//  Created by James Perlman on 10/23/21.
//

#ifndef VulkanDebugUtils_hpp
#define VulkanDebugUtils_hpp

#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace VulkanDebugUtils
{

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

bool isValidationEnabled();

bool isValidationSupported();


VkResult createDebugUtilsMessengerEXT
(
 VkInstance instance,
 const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
 const VkAllocationCallbacks* pAllocator,
 VkDebugUtilsMessengerEXT* pDebugMessenger
 );

void destroyDebugUtilsMessengerEXT
(
 VkInstance instance,
 VkDebugUtilsMessengerEXT debugMessenger,
 const VkAllocationCallbacks* pAllocator
 );

VkDebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo();

};


#endif /* VulkanDebugger_hpp */
