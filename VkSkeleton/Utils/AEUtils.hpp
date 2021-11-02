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
#include "AEGP_SuiteHandler.h"

namespace AEUtils
{

std::string getResourcePath(PF_InData* in_data);

void fetchARGB32Pixels(AEGP_SuiteHandler&   suites,
                       PF_InData*           in_data,
                       PF_EffectWorld*      input_worldP,
                       PF_EffectWorld*      output_worldP,
                       PF_PixelFloat*       floatBufferP);

enum CopyIO {
    InputWorldToBuffer,
    BufferToOutputWorld,
};

}

#endif /* AEUtils_hpp */
