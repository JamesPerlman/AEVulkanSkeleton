#include <assert.h>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>

#include "AEFX_SuiteHelper.h"
#include "AEUtils.hpp"
#include "AEVulkanUtils.hpp"
#include "Smart_Utils.h"
#include "VulkanComputeDataTypes.hpp"
#include "VulkanComputeProgram.hpp"

#include "VkSkeleton.hpp"

// MARK: - Globals

VulkanComputeProgram  computeProgram{};
std::string           resourcePath;

// MARK: - About

static PF_Err 
About (	
       PF_InData		*in_data,
       PF_OutData		*out_data,
       PF_ParamDef		*params[],
       PF_LayerDef		*output )
{
    AEGP_SuiteHandler suites(in_data->pica_basicP);
    
    suites.ANSICallbacksSuite1()->sprintf(out_data->return_msg,
                                          "%s v%d.%d\r%s",
                                          STR(StrID_Name),
                                          MAJOR_VERSION,
                                          MINOR_VERSION,
                                          STR(StrID_Description));
    return PF_Err_NONE;
}

// MARK: - GlobalSetup

static PF_Err
GlobalSetup (	
             PF_InData*     in_data,
             PF_OutData*    out_data,
             PF_ParamDef*   params[],
             PF_LayerDef*   output)
{
    out_data->my_version = PF_VERSION(MAJOR_VERSION,
                                      MINOR_VERSION,
                                      BUG_VERSION,
                                      STAGE_VERSION,
                                      BUILD_VERSION);
    
    out_data->out_flags = 	PF_OutFlag_DEEP_COLOR_AWARE;
    
    out_data->out_flags2
    = PF_OutFlag2_FLOAT_COLOR_AWARE
    | PF_OutFlag2_SUPPORTS_SMART_RENDER;
    // TODO: | PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    
    PF_Err err = PF_Err_NONE;
    try
    {
        resourcePath = AEUtils::getResourcePath(in_data);
        
        auto computeShaderPath = resourcePath + "shaders/invert.comp";
        
        computeProgram.setUp(computeShaderPath);
    }
    catch(PF_Err& thrown_err)
    {
        err = thrown_err;
    }
    catch (...)
    {
        err = PF_Err_OUT_OF_MEMORY;
    }
    
    return err;
}

// MARK: - GlobalSetdown

static PF_Err
GlobalSetdown (
               PF_InData        *in_data,
               PF_OutData        *out_data,
               PF_ParamDef        *params[],
               PF_LayerDef        *output )
{
    PF_Err err = PF_Err_NONE;
    
    computeProgram.tearDown();
    
    return err;
}

// MARK: - ParamsSetup

static PF_Err 
ParamsSetup (	
             PF_InData		*in_data,
             PF_OutData		*out_data,
             PF_ParamDef		*params[],
             PF_LayerDef		*output )
{
    PF_Err		err		= PF_Err_NONE;
    PF_ParamDef	def;
    
    AEFX_CLR_STRUCT(def);

    PF_ADD_FLOAT_SLIDERX(STR(StrID_Pivot_Param_Name),
                         VKSKELETON_SLIDER_MIN,
                         VKSKELETON_SLIDER_MAX,
                         VKSKELETON_SLIDER_MIN,
                         VKSKELETON_SLIDER_MAX,
                         VKSKELETON_SLIDER_DFLT,
                         PF_Precision_HUNDREDTHS,
                         PF_ValueDisplayFlag_NONE,
                         PF_ParamFlag_NONE,
                         VKSKELETON_SLIDER_DISK_ID);
    
    out_data->num_params = VKSKELETON_NUM_PARAMS;
    
    return err;
}

// MARK: - PreRender

static PF_Err
PreRender(PF_InData*            in_data,
          PF_OutData*           out_data,
          PF_PreRenderExtra*    extra)
{
    PF_Err	err = PF_Err_NONE,
    err2 = PF_Err_NONE;
    
    PF_ParamDef slider_param;
    
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;
    
    AEFX_CLR_STRUCT(slider_param);
    
    ERR(PF_CHECKOUT_PARAM(in_data,
                          VKSKELETON_SLIDER,
                          in_data->current_time,
                          in_data->time_step,
                          in_data->time_scale,
                          &slider_param));
    
    ERR(extra->cb->checkout_layer(in_data->effect_ref,
                                  VKSKELETON_INPUT,
                                  VKSKELETON_INPUT,
                                  &req,
                                  in_data->current_time,
                                  in_data->time_step,
                                  in_data->time_scale,
                                  &in_result));
    
    if (!err){
        UnionLRect(&in_result.result_rect, &extra->output->result_rect);
        UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
    }
    ERR2(PF_CHECKIN_PARAM(in_data, &slider_param));
    return err;
}

// MARK: - SmartRender

static PF_Err
SmartRender(PF_InData*			    in_data,
            PF_OutData*			    out_data,
            PF_SmartRenderExtra*	extra)
{
    PF_Err				err = PF_Err_NONE,
    err2 = PF_Err_NONE;
    
    PF_EffectWorld*     input_worldP = NULL;
    PF_EffectWorld*     output_worldP = NULL;
    PF_WorldSuite2*     wsP = NULL;
    PF_PixelFormat		pfPixelFormat = PF_PixelFormat_INVALID;
    PF_FpLong			sliderVal = 0;
    PF_ParamDef         slider_param;
    AEGP_SuiteHandler   suites(in_data->pica_basicP);
    
    ERR(AEFX_AcquireSuite(in_data,
                          out_data,
                          kPFWorldSuite,
                          kPFWorldSuiteVersion2,
                          "Couldn't load suite.",
                          (void**)&wsP));
    
    AEFX_CLR_STRUCT(slider_param);
    
    ERR(PF_CHECKOUT_PARAM(in_data,
                          VKSKELETON_SLIDER,
                          in_data->current_time,
                          in_data->time_step,
                          in_data->time_scale,
                          &slider_param));
    
    if (!err){
        sliderVal = slider_param.u.fd.value / 100.0f;
    }
    
    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, VKSKELETON_INPUT, &input_worldP));
    
    ERR(extra->cb->checkout_output(in_data->effect_ref, &output_worldP));
    
    ERR(AEFX_AcquireSuite(in_data,
                          out_data,
                          kPFWorldSuite,
                          kPFWorldSuiteVersion2,
                          "Couldn't load suite.",
                          (void**)&wsP));
    
    UniformBufferObject ubo {
        .pivot = static_cast<float>(slider_param.u.fs_d.value),
    };
    
    if (!err){
        try
        {
            CHECK(wsP->PF_GetPixelFormat(input_worldP, &pfPixelFormat));
            
            ImageInfo imageInfo{};
            imageInfo.width = input_worldP->width;
            imageInfo.height = input_worldP->height;
            imageInfo.pixelFormat = AEVulkanUtils::pixelFormatForPFPixelFormat(pfPixelFormat);
            
            auto copyInputWorldToBuffer = [&](void* buffer)
            {
                AEUtils::copyImageData(suites,
                                       in_data,
                                       input_worldP,
                                       output_worldP,
                                       AEUtils::CopyCommand::InputWorldToBuffer,
                                       pfPixelFormat,
                                       buffer);
            };
            
            auto copyBufferToOutputWorld = [&](void* buffer)
            {
                AEUtils::copyImageData(suites,
                                       in_data,
                                       input_worldP,
                                       output_worldP,
                                       AEUtils::CopyCommand::BufferToOutputWorld,
                                       pfPixelFormat,
                                       buffer);
            };
            
            computeProgram.process(imageInfo,
                                   ubo,
                                   copyInputWorldToBuffer,
                                   copyBufferToOutputWorld);
        }
        catch (PF_Err& thrown_err)
        {
            err = thrown_err;
        }
        catch (...)
        {
            err = PF_Err_OUT_OF_MEMORY;
        }
    }
    
    // If you have PF_ABORT or PF_PROG higher up, you must set
    // the AE context back before calling them, and then take it back again
    // if you want to call some more OpenGL.
    ERR(PF_ABORT(in_data));
    
    ERR2(AEFX_ReleaseSuite(in_data,
                           out_data,
                           kPFWorldSuite,
                           kPFWorldSuiteVersion2,
                           "Couldn't release suite."));
    ERR2(PF_CHECKIN_PARAM(in_data, &slider_param));
    ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, VKSKELETON_INPUT));
    
    return err;
}



// MARK: - EffectMain

PF_Err
EffectMain(PF_Cmd		cmd,
           PF_InData*	in_data,
           PF_OutData*	out_data,
           PF_ParamDef*	params[],
           PF_LayerDef*	output,
           void*        extra)
{
    PF_Err		err = PF_Err_NONE;
    
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                err = About(in_data,
                            out_data,
                            params,
                            output);
                break;
                
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_data,
                                  out_data,
                                  params,
                                  output);
                break;
                
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_data,
                                  out_data,
                                  params,
                                  output);
                break;
                
            case PF_Cmd_GLOBAL_SETDOWN:
                err = GlobalSetdown(in_data,
                                    out_data,
                                    params,
                                    output);
                break;
                
            case  PF_Cmd_SMART_PRE_RENDER:
                err = PreRender(in_data,
                                out_data,
                                reinterpret_cast<PF_PreRenderExtra*>(extra));
                break;
                
            case  PF_Cmd_SMART_RENDER:
                err = SmartRender(in_data,
                                  out_data,
                                  reinterpret_cast<PF_SmartRenderExtra*>(extra));
                break;
        }
    }
    catch(PF_Err &thrown_err){
        err = thrown_err;
    }
    return err;
}

// Mark: - PluginDataEntryFunction
extern "C" DllExport
PF_Err PluginDataEntryFunction(PF_PluginDataPtr inPtr,
                               PF_PluginDataCB  inPluginDataCallBackPtr,
                               SPBasicSuite*    inSPBasicSuitePtr,
                               const char*      inHostName,
                               const char*      inHostVersion)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;
    
    result = PF_REGISTER_EFFECT(inPtr,
                                inPluginDataCallBackPtr,
                                "VkSkeleton", // Name
                                "JPERL VkSkeleton", // Match Name
                                "jperl", // Category
                                AE_RESERVED_INFO); // Reserved Info
    
    return result;
}
