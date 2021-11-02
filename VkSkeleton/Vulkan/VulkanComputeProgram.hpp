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
    
    enum PixelFormat : size_t {
        ARGB32 = 32,
        ARGB16 = 16,
        ARGB8  = 8,
    };

    struct ImageInfo {
        uint32_t imageWidth;
        uint32_t imageHeight;
        
        PixelFormat pixelFormat;
        
        size_t size() {
            return static_cast<size_t>(pixelFormat) * static_cast<size_t>(imageWidth * imageHeight);
        }
    };
    
    void setUp(std::string shaderFilePath);
    void tearDown();
    
    void initializeShader(std::string filePath);
    void process(ImageInfo imageInfo,
                 std::function<void(void*)>& writeInputPixels,
                 std::function<void(void*)>& readOutputPixels);
    
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
    VkDeviceMemory              deviceMemory                = VK_NULL_HANDLE;
    VkBuffer                    inputBuffer                 = VK_NULL_HANDLE;
    VkBuffer                    outputBuffer                = VK_NULL_HANDLE;
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
    
    void createDeviceMemory();
    void destroyDeviceMemory();
    
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
