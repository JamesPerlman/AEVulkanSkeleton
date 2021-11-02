#include "VkSkeleton.hpp"


typedef struct {
	unsigned long	index;
	char			str[256];
} TableString;


TableString g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"VkSkeleton",
	StrID_Description,				"A basic Vulkan Compute program",
	StrID_Pivot_Param_Name,			"Pivot",
};


char* GetStringPtr(int strNum)
{
	return g_strs[strNum].str;
}

	
