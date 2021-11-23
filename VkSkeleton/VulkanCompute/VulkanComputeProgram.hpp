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
                 UniformBufferObject uniformBufferObject,
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
    VkSampler                   inputSampler;
    VkSampler                   outputSampler;
    VkBuffer                    uniformBuffer               = VK_NULL_HANDLE;
    VkDeviceMemory              uniformBufferMemory         = VK_NULL_HANDLE;
    
    // Ephemeral objects
    VkBuffer                    inputBuffer                 = VK_NULL_HANDLE;
    VkDeviceMemory              inputBufferMemory           = VK_NULL_HANDLE;
    VkImage                     inputImage                  = VK_NULL_HANDLE;
    VkDeviceMemory              inputImageMemory            = VK_NULL_HANDLE;
    VkImageView                 inputImageView              = VK_NULL_HANDLE;
    
    VkBuffer                    outputBuffer                = VK_NULL_HANDLE;
    VkDeviceMemory              outputBufferMemory          = VK_NULL_HANDLE;
    VkImage                     outputImage                 = VK_NULL_HANDLE;
    VkDeviceMemory              outputImageMemory           = VK_NULL_HANDLE;
    VkImageView                 outputImageView             = VK_NULL_HANDLE;
    
    VkDescriptorSetLayout       descriptorSetLayout         = VK_NULL_HANDLE;
    VkPipelineLayout            pipelineLayout              = VK_NULL_HANDLE;
    VkPipeline                  pipeline                    = VK_NULL_HANDLE;
    VkDescriptorSet             descriptorSet               = VK_NULL_HANDLE;
    
    // Synchronization
    std::mutex textureReadWriteMutex;
    
    // Compute info
    ImageInfo imageInfo;
    
    // Convenience methods
    void regenerateImageBuffersIfNeeded(ImageInfo imageInfo);
    void updateUniformBuffer(UniformBufferObject uniformBufferObject);
    
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
    
    void createSamplers();
    void destroySamplers();
    
    void createUniformBuffer();
    void destroyUniformBuffer();
    
    void createImageBuffers();
    void destroyImageBuffers();
    
    void createImageBufferMemory();
    void destroyImageBufferMemory();
    
    void bindBufferMemory();
    
    void createImages();
    void destroyImages();
    
    void createImageMemory();
    void destroyImageMemory();
    
    void bindImageMemory();
    
    void createImageViews();
    void destroyImageViews();
    
    void createDescriptorSetLayout();
    void destroyDescriptorSetLayout();
    
    void createDescriptorSet();
    void destroyDescriptorSet();
    
    void createPipelineLayout();
    void destroyPipelineLayout();
    
    void createPipeline();
    void destroyPipeline();
    
    void updateDescriptorSet();
    
    void transitionImageLayout(VkImage& image, ImageLayoutTransitionInfo transitionInfo);
    void transitionImageLayouts();
    
    void copyInputBufferToImage();
    void copyOutputImageToBuffer();
    
    void submitComputeQueue(std::function<void(VkCommandBuffer&)> recordCommands);
    
    void executeShader();
    
};


#endif /* VulkanComputeProgram_hpp */
