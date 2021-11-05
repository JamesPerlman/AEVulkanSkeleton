//
//  FileUtils.cpp
//  VkComputeTest
//
//  Created by James Perlman on 10/23/21.
//

#include "AEUtils.hpp"
#include "FileUtils.hpp"

using namespace FileUtils;

std::vector<char> FileUtils::readFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file!");
    }
    
    size_t fileSize = (size_t)file.tellg();

    std::vector<char> buffer(fileSize);
    
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    
    return buffer;
}

