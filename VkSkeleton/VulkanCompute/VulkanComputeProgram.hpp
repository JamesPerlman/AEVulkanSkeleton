//
//  VulkanComputeProgram.hpp
//  VkComputeTest
//
//  Created by James Perlman on 10/23/21.
//

#ifndef VulkanComputeProgram_hpp
#define VulkanComputeProgram_hpp

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "VulkanComputeDataTypes.hpp"

class VulkanComputeProgram
{
public:
    
    void setUp(std::string shaderFilePath);
    void tearDown();
    
    void process(ImageInfo imageInfo,
                 std::function<void(void*)> writeInputPixels,
                 std::function<void(void*)> readOutputPixels);
    
private:
    // Persisted objects
    VkInstance                  instance;
    VkDebugUtilsMessengerEXT    debugMessenger;
    uint32_t                    computeQueueFamilyIndex;
    VkPhysicalDevice            physicalDevice              = VK_NULL_HANDLE;
    VkDevice                    logicalDevice;
    VkQueue                     computeQueue;
    std::string                 shaderFilePath;
    VkShaderModule              shaderModule;
    VkCommandPool               commandPool;
    VkDescriptorPool            descriptorPool;
    
    // Ephemeral objects
    size_t                      bufferSize                  = 0;
    VkBuffer                    inputBuffer                 = VK_NULL_HANDLE;
    VkDeviceMemory              inputMemory                 = VK_NULL_HANDLE;
    VkBuffer                    outputBuffer                = VK_NULL_HANDLE;
    VkDeviceMemory              outputMemory                = VK_NULL_HANDLE;
    VkDescriptorSetLayout       descriptorSetLayout         = VK_NULL_HANDLE;
    VkPipelineLayout            pipelineLayout              = VK_NULL_HANDLE;
    VkPipeline                  pipeline                    = VK_NULL_HANDLE;
    VkCommandBuffer             commandBuffer               = VK_NULL_HANDLE;
    VkDescriptorSet             descriptorSet               = VK_NULL_HANDLE;
    
    // Synchronization
    std::mutex textureReadWriteMutex;
    
    // Convenience methods
    void regenerateBuffersIfNeeded(ImageInfo imageInfo);
    
    // Object management methods
    void createVulkanInstance();
    void destroyVulkanInstance();
    
    void createDebugMessenger();
    void destroyDebugMessenger();
    
    void assignPhysicalDevice();
    
    void createLogicalDevice();
    void destroyLogicalDevice();
    
    void createShaderModule();
    void destroyShaderModule();
    
    void createCommandPool();
    void destroyCommandPool();
    
    void createDescriptorPool();
    void destroyDescriptorPool();
    
    void createStorageBuffers();
    void destroyStorageBuffers();
    
    void createDescriptorSetLayout();
    void destroyDescriptorSetLayout();
    
    void createDescriptorSet();
    void destroyDescriptorSet();
    
    void createPipelineLayout();
    void destroyPipelineLayout();
    
    void createPipeline();
    void destroyPipeline();
    
    void createCommandBuffer();
    void destroyCommandBuffer();
    
    void updateDescriptorSet();
    void recordCommandBuffer();
    
    void submitComputeQueue();
    
};


#endif /* VulkanComputeProgram_hpp */
