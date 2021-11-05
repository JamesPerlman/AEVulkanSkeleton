//
//  VulkanComputeProgram.hpp
//  VkComputeTest
//
//  Created by James Perlman on 10/23/21.
//

#ifndef VulkanComputeProgram_hpp
#define VulkanComputeProgram_hpp

#include <functional>
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
    VkCommandBuffer             commandBuffer;
    VkDescriptorPool            descriptorPool;
    
    // Ephemeral objects
    size_t                      bufferSize                  = 0;
    size_t                      deviceMemorySize            = 0;
    VkBuffer                    inputBuffer                 = VK_NULL_HANDLE;
    VkDeviceMemory              inputMemory                 = VK_NULL_HANDLE;
    VkBuffer                    outputBuffer                = VK_NULL_HANDLE;
    VkDeviceMemory              outputMemory                = VK_NULL_HANDLE;
    VkDescriptorSetLayout       descriptorSetLayout         = VK_NULL_HANDLE;
    VkPipelineLayout            pipelineLayout              = VK_NULL_HANDLE;
    VkPipeline                  pipeline                    = VK_NULL_HANDLE;
    VkDescriptorSet             descriptorSet;
    
    // Convenience methods
    void resizeBuffersIfNeeded(size_t newBufferSize);
    
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
    
    void createCommandBuffer();
    void destroyCommandBuffer();
    
    void createStorageBuffers();
    void destroyStorageBuffers();
    
    void createDescriptorSetLayout();
    void destroyDescriptorSetLayout();
    
    void createPipelineLayout();
    void destroyPipelineLayout();
    
    void createPipeline();
    void destroyPipeline();
    
    void createDescriptorPool();
    void destroyDescriptorPool();
    
    void updateDescriptorSet();
    void recordCommandBuffer();
    
    void submitComputeQueue();
    
};


#endif /* VulkanComputeProgram_hpp */
