//
//  AEUtils.hpp
//  VkSkeleton
//
//  Created by James Perlman on 10/25/21.
//

#ifndef AEUtils_hpp
#define AEUtils_hpp

#include <string>

#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AEFX_SuiteHelper.h"
#include "AEGP_SuiteHandler.h"

// Error checking macro
#define CHECK(err) { PF_Err err1 = err; if (err1 != PF_Err_NONE) { throw PF_Err(err1); } }

namespace AEUtils
{

std::string getResourcePath(PF_InData* in_data);

enum CopyCommand {
    InputWorldToBuffer,
    BufferToOutputWorld,
};

void copyImageData(AEGP_SuiteHandler&   suites,
                   PF_InData*           in_data,
                   PF_PixelFormat       pixelFormat,
                   PF_EffectWorld*      input_worldP,
                   PF_EffectWorld*      output_worldP,
                   CopyCommand          copyCommand,
                   void*                bufferP);


}

#endif /* AEUtils_hpp */
