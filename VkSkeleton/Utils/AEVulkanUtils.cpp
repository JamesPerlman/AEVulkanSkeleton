//
//  AEVulkanUtils.cpp
//  VkSkeleton
//
//  Created by James Perlman on 11/2/21.
//

#include "AEVulkanUtils.hpp"

using namespace AEVulkanUtils;

PixelFormat AEVulkanUtils::pixelFormatForPFPixelFormat(PF_PixelFormat pixelFormat)
{
    
    switch (pixelFormat)
    {
        case PF_PixelFormat_ARGB128:
        {
            return PixelFormat::ARGB128;
        }
        case PF_PixelFormat_ARGB64:
        {
            return PixelFormat::ARGB64;
        }
        case PF_PixelFormat_ARGB32:
        {
            return PixelFormat::ARGB32;
        }
        default:
        {
            throw std::runtime_error("Unhandled PixelFormat, cannot convert to PF_PixelFormat.");
        }
    }
};

PF_PixelFormat AEVulkanUtils::pfPixelFormatForPixelFormat(PixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
        case PixelFormat::ARGB128:
        {
            return PF_PixelFormat_ARGB128;
        }
        case PixelFormat::ARGB64:
        {
            return PF_PixelFormat_ARGB64;
        }
        case PixelFormat::ARGB32:
        {
            return PF_PixelFormat_ARGB32;
        }
        default:
        {
            throw std::runtime_error("Unhandled PF_PixelFormat, cannot convert to PixelFormat.");
        }
    }
};
