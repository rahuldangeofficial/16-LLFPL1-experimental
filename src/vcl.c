#include "vcl.h"
#include <stdio.h>

#ifdef __x86_64__
#include <cpuid.h>
#endif

HardwareProfile vcl_discover(){
	HardwareProfile profile={16,64};

	#ifdef __x86_64__
		unsigned int eax, ebx, ecx, edx;
		if(__get_cpuid(1,&eax,&ebx,&ecx,&edx)){
			profile.cache_line_size=((ebx>>8)&0xFF)*8;
		}
	#endif
	
	printf("[VCL] PROBE: %u Registers | %u-byte Cache Alignment\n",
			profile.physical_regs,profile.cache_line_size);
	return profile;
}
