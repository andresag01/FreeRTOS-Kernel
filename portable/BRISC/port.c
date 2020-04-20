/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Port includes */
#include "utils.h"

/* Standard includes. */
#include <string.h>

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error configSUPPORT_DYNAMIC_ALLOCATION must be set to 1 to use this port.
#endif

/*-----------------------------------------------------------*/

/*
 * Critical section management.
 */
void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
}

void vPortExitCritical( void )
{
	portENABLE_INTERRUPTS();
}

/*-----------------------------------------------------------*/

/*
 * Disable/enable interrupts.
 */
void disableInterrupts( void )
{
	volatile InterruptConfig_t *icfg =
		(InterruptConfig_t *)io[INTERRUPT_REASON_ICFG];
	icfg->global = 0;
}

void enableInterrupts( void )
{
	volatile InterruptConfig_t *icfg =
	    (InterruptConfig_t *)io[INTERRUPT_REASON_ICFG];
	icfg->global = 1;
}

/*-----------------------------------------------------------*/

/*
 * Manual context switch called by portYIELD or taskYIELD.
 */
__attribute__((naked)) void vPortYield( void )
{
	__asm volatile(
		"ecallui " xstr(SYSCALL_TASK_YIELD) "	\n"
		"ret									\n"
	);
}

/*-----------------------------------------------------------*/

/*
 * Set up timer interrupt
 */

static void vPortSetupTimerInterrupt(uint32_t delay)
{
    volatile InterruptConfig_t *icfg =
		(InterruptConfig_t *)io[INTERRUPT_REASON_ICFG];
    volatile Timer_t *tmr = (Timer_t *)io[INTERRUPT_REASON_TMR0];

    assert(icfg->type == INTERRUPT_REASON_ICFG);
    assert(tmr->type == INTERRUPT_REASON_TMR0);

    /* Globally enable interrupts */
    icfg->global = 1;

    /* Enable individual interrupts */
    icfg->enbdev = 0xffffffff;

    /* Configure TMR0 (i.e. timer device) */
    tmr->curcnt = 0;
    tmr->reload = delay - 1;
    tmr->status = 0x00000003;
}

/*-----------------------------------------------------------*/

/*
 * Scheduling
 */

#define TIMER_INTERRUPT_INTERVAL 1000	/* Count 10000 instructions */

__attribute__((naked)) StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack,
														   TaskFunction_t pxCode,
														   void *pvParameters )
{
	__asm volatile(
		/* Get the stack base in case FreeRTOS does something funny */
		"ldab r0, r0		\n"

		/* The first word in the stack is used by the callee */
		"ldawfi r0, r0, 4	\n"

        /*
         * The follwoing 10 words are used for r0-r9. r0 contains a pointer
         * to the task arguments; skip over the other 9 words.
         */
        "stwfi r2, r0, 0    \n"
		"ldawfi r0, r0, 40	\n"

		/* Store pc */
		"stwfi r1, r0, 0	\n"

		/* Store ep */
		"ldawep r1, 0		\n"
		"stwfi r1, r0, 4	\n"

		/* Store wp */
		"ldab r1, r0		\n"
		"stwfi r1, r0, 8	\n"

		/* Store sr */
		"ldc r1, 64			\n"
		"stwfi r1, r0, 12	\n"

		/* Set the pxTopOfStack variable in TCB to the base of the object */
		"ldab r0, r0		\n"
		"ret				\n"
	);
}

__attribute__((naked)) void prvTaskSwitchContext( void )
{
	__asm volatile(
		/* Alloc stack for the parents wp and a return address */
		"newmwi r0, 8				\n"
		"entwp r0, 4				\n"

		/* Load the top of the stack value from the current TCB */
		"ldwep r0, pxCurrentTCB		\n"
		"ldwfi r0, r0, 0			\n"

		/* Move the top of the stack value to point to the register context */
		"ldawfi r0, r0, 4			\n"

		/* Save the registers */
		"ldc r1, 0					\n"
		"saveregs r1, r0			\n"

		/* Perform the context switch */
		"blr vTaskSwitchContext		\n"

		/* Load the top of the stack value for the next TCB */
		"ldwep r0, pxCurrentTCB		\n"
		"ldwfi r0, r0, 0			\n"

		/* Move the top of the stack value to point to the register context */
		"ldawfi r0, r0, 4			\n"

		/* Restore the registers */
		"ldc r1, 0					\n"
		"restoreregs r1, r0			\n"

        /* Set NORMAL bit in register mode */
        "ldc r0, 1                  \n"
        "setmode r0, r1             \n"

		/* Finish! */
		"extwp						\n"
		"ret						\n"
	);
}

__attribute__((naked)) void prvStartFirstTask( void )
{
	__asm volatile(
		/* Load the top of the stack value from the TCB */
		"ldwep r0, pxCurrentTCB		\n"
		"ldwfi r0, r0, 0			\n"

		/* Move the top of the stack value to point to the register context */
		"ldawfi r0, r0, 4			\n"

		/* Restore the registers */
		"ldc r1, 0					\n"
		"restoreregs r1, r0			\n"

        /* Set NORMAL bit in register mode */
        "ldc r0, 1                  \n"
        "setmode r0, r1             \n"

		/* Finish! */
		"ret						\n"
	);
}

BaseType_t xPortStartScheduler( void )
{
	/* Start the timer that generates the preemption call */
	vPortSetupTimerInterrupt(TIMER_INTERRUPT_INTERVAL);

	/* Start the first task. */
	prvStartFirstTask();

	/*
	 * On most ports, this function would not return because it is running on
	 * privileged mode and the scheduler has already been set off. However,
	 * in BeyondRISC exceptions and interrupts look like function calls, so
	 * its more natural to just return "out" of interrupt mode.
	 */
	return pdFALSE;
}

void vPortEndScheduler( void )
{
	/* Should never happen! */
	fail();
}
