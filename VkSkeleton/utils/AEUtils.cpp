//
//  AEUtils.cpp
//  VkSkeleton
//
//  Created by James Perlman on 10/25/21.
//

#include "AEUtils.hpp"

using namespace AEUtils;

// MARK: - Get Resources Path

#ifdef AE_OS_WIN
std::string wcharToString(const wchar_t* pcs)
{
    int res = WideCharToMultiByte(CP_ACP, 0, pcs, -1, NULL, 0, NULL, NULL);
    
    std::auto_ptr<char> shared_pbuf(new char[res]);
    
    char* pbuf = shared_pbuf.get();
    
    res = WideCharToMultiByte(CP_ACP, 0, pcs, -1, pbuf, res, NULL, NULL);
    
    return std::string(pbuf);
}
#endif

std::string AEUtils::getResourcePath(PF_InData* in_data)
{
    std::string resourcePath;
    
    A_UTF16Char pluginFolderPath[AEFX_MAX_PATH];
    PF_GET_PLATFORM_DATA(PF_PlatData_EXE_FILE_PATH_W, &pluginFolderPath);
    
#ifdef AE_OS_WIN
    resourcePath = wcharToString((wchar_t*)pluginFolderPath);
    std::string::size_type pos;
    //delete the plugin name
    pos = resourcePath.rfind("\\", resourcePath.length());
    resourcePath = resourcePath.substr(0, pos) + "\\";
#endif
    
#ifdef AE_OS_MAC
    NSUInteger length = 0;
    A_UTF16Char* tmp = pluginFolderPath;
    while (*tmp++ != 0) {
        ++length;
    }
    NSString* newStr = [[NSString alloc] initWithCharacters:pluginFolderPath length : length];
    resourcePath = std::string([newStr UTF8String]);
    resourcePath += "/Contents/Resources/";
#endif
    
    return resourcePath;
}


// MARK: - RGBA32 Pixel Copy

struct CopyPixelFloat_t {
    PF_PixelFloat*  floatBufferP;
    PF_EffectWorld* input_worldP;
    PF_EffectWorld* output_worldP;
};

// In = into the refcon
// input_worldP -> refcon
PF_Err
CopyPixelFloatFromInputWorldToBuffer(void*          refcon,
                                     A_long         x,
                                     A_long         y,
                                     PF_PixelFloat* inP,
                                     PF_PixelFloat* )
{
    CopyPixelFloat_t*   info  = reinterpret_cast<CopyPixelFloat_t*>(refcon);
    PF_PixelFloat*      outP = info->floatBufferP + y * info->input_worldP->width + x;
    
    outP->red   = inP->red;
    outP->green = inP->green;
    outP->blue  = inP->blue;
    outP->alpha = inP->alpha;
    
    return PF_Err_NONE;
}

// Out = out to the outP (output_worldP)
// refcon -> output_worldP
PF_Err
CopyPixelFloatFromBufferToOutputWorld(void*             refcon,
                                      A_long            x,
                                      A_long            y,
                                      PF_PixelFloat*    ,
                                      PF_PixelFloat*    outP)
{
    CopyPixelFloat_t* info = reinterpret_cast<CopyPixelFloat_t*>(refcon);
    const PF_PixelFloat* inP = info->floatBufferP + y * info->output_worldP->width + x;
    
    outP->red   = inP->red;
    outP->green = inP->green;
    outP->blue  = inP->blue;
    outP->alpha = inP->alpha;
    
    return PF_Err_NONE;
}

// Image Data Copy Function
void AEUtils::copyImageData(AEGP_SuiteHandler&  suites,
                            PF_InData*          in_data,
                            PF_EffectWorld*     input_worldP,
                            PF_EffectWorld*     output_worldP,
                            CopyCommand         copyCommand,
                            PF_PixelFormat      pixelFormat,
                            void*               bufferP)
{
    // ARGB128 contains 32 bits per color component
    // This one is special since we need to use a custom float iterator
    switch (pixelFormat)
    {
        case PF_PixelFormat_ARGB128:
        {
            CopyPixelFloat_t refcon {
                .floatBufferP = reinterpret_cast<PF_PixelFloat*>(bufferP),
            };
            
            PF_IteratePixelFloatFunc copyFunction;
            
            switch (copyCommand)
            {
                case CopyCommand::InputWorldToBuffer:
                {
                    copyFunction = CopyPixelFloatFromInputWorldToBuffer;
                    refcon.input_worldP = input_worldP;
                    break;
                }
                case CopyCommand::BufferToOutputWorld:
                {
                    copyFunction = CopyPixelFloatFromBufferToOutputWorld;
                    refcon.output_worldP = output_worldP;
                    break;
                }
            }
            
            CHECK(suites.IterateFloatSuite1()->iterate(in_data,
                                                       0,
                                                       input_worldP->height,
                                                       input_worldP,
                                                       nullptr,
                                                       reinterpret_cast<void*>(&refcon),
                                                       copyFunction,
                                                       output_worldP));
            
            break;
        }
        case PF_PixelFormat_ARGB64:
        {
            PF_Pixel16* pixelDataStart = NULL;
            auto pixelSize = sizeof(PF_Pixel16);
            
            switch (copyCommand)
            {
                case CopyCommand::BufferToOutputWorld:
                {
                    PF_GET_PIXEL_DATA16(output_worldP, NULL, &pixelDataStart);
                    auto bufferSize = pixelSize * output_worldP->width * output_worldP->height;
                    memcpy(pixelDataStart, bufferP, bufferSize);
                    break;
                }
                case CopyCommand::InputWorldToBuffer:
                {
                    PF_GET_PIXEL_DATA16(input_worldP, NULL, &pixelDataStart);
                    auto bufferSize = pixelSize * input_worldP->width * input_worldP->height;
                    memcpy(bufferP, pixelDataStart, bufferSize);
                    break;
                }
            }
            break;
        }
            
        case PF_PixelFormat_ARGB32:
        {
            PF_Pixel8* pixelDataStart = NULL;
            auto pixelSize = sizeof(PF_Pixel8);
            
            switch (copyCommand)
            {
                case CopyCommand::BufferToOutputWorld:
                {
                    PF_GET_PIXEL_DATA8(output_worldP, NULL, &pixelDataStart);
                    auto bufferSize = pixelSize * output_worldP->width * output_worldP->height;
                    memcpy(pixelDataStart, bufferP, bufferSize);
                    break;
                }
                case CopyCommand::InputWorldToBuffer:
                {
                    PF_GET_PIXEL_DATA8(input_worldP, NULL, &pixelDataStart);
                    auto bufferSize = pixelSize * input_worldP->width * input_worldP->height;
                    memcpy(bufferP, pixelDataStart, bufferSize);
                    break;
                }
            }
            break;
        }
    }
}
