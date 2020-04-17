#include "FreeRTOS.h"

#include <stddef.h>

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/*-----------------------------------------------------------*/

__attribute__((naked)) void *pvPortMalloc( size_t xWantedSize )
{
	__asm volatile(
		"newm8 r0, r0	\n"
		"ret			\n"
	);
}

/*-----------------------------------------------------------*/

__attribute__((naked)) void vPortFree( void *pv )
{
	// This function does nothing. It merely overwrites the input parameter
	// register to delete the reference to the "freed" memory from the roots
	__asm volatile(
		"ldc r0, 0	\n"
		"ret		\n"
	);
}
