//
//  AEVulkanUtils.hpp
//  VkSkeleton
//
//  Created by James Perlman on 11/2/21.
//

#ifndef AEVulkanUtils_hpp
#define AEVulkanUtils_hpp

#include "AE_EffectPixelFormat.h"
#include "VulkanComputeDataTypes.hpp"

namespace AEVulkanUtils
{

PF_PixelFormat pfPixelFormatForPixelFormat(PixelFormat pixelFormat);

PixelFormat pixelFormatForPFPixelFormat(PF_PixelFormat pixelFormat);

}

#endif /* AEVulkanUtils_hpp */
