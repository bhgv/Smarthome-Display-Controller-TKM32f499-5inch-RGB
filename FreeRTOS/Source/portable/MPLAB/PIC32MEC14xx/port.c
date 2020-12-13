/*
    FreeRTOS V9.0.0rc2 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the PIC32MEC14xx  port.
 *----------------------------------------------------------*/

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"

/* Microchip includes. */
#include <xc.h>
#include <cp0defs.h>

#if !defined(__MEC__)
	#error This port is designed to work with XC32 on MEC14xx.  Please update your C compiler version or settings.
#endif

#if( ( configMAX_SYSCALL_INTERRUPT_PRIORITY >= 0x7 ) || ( configMAX_SYSCALL_INTERRUPT_PRIORITY == 0 ) )
	#error configMAX_SYSCALL_INTERRUPT_PRIORITY must be less than 7 and greater than 0
#endif

/* Bits within various registers. */
#define portIE_BIT					( 0x00000001 )
#define portEXL_BIT					( 0x00000002 )

/* The EXL bit is set to ensure interrupts do not occur while the context of
the first task is being restored.  MEC14xx does not have DSP HW. */
#define portINITIAL_SR				( portIE_BIT | portEXL_BIT )

/* MEC14xx RTOS Timer MMCR's. */
#define portMMCR_RTMR_PRELOAD	*((volatile uint32_t *)(0xA0007404ul))
#define portMMCR_RTMR_CONTROL	*((volatile uint32_t *)(0xA0007408ul))

/* MEC14xx JTVIC external interrupt controller is mapped to M14K closely-coupled
peripheral space. */
#define portGIRQ23_RTOS_TIMER_BITPOS	( 4 )
#define portGIRQ23_RTOS_TIMER_MASK		( 1ul << ( portGIRQ23_RTOS_TIMER_BITPOS ) )
#define portMMCR_JTVIC_GIRQ23_SRC		*((volatile uint32_t *)(0xBFFFC0F0ul))
#define portMMCR_JTVIC_GIRQ23_SETEN		*((volatile uint32_t *)(0xBFFFC0F4ul))
#define portMMCR_JTVIC_GIRQ23_PRIA		*((volatile uint32_t *)(0xBFFFC3F0ul))

/* MIPS Software Interrupts are routed through JTVIC GIRQ24 */
#define portGIRQ24_M14K_SOFTIRQ0_BITPOS	( 1 )
#define portGIRQ24_M14K_SOFTIRQ0_MASK	( 1ul << ( portGIRQ24_M14K_SOFTIRQ0_BITPOS ) )
#define portMMCR_JTVIC_GIRQ24_SRC		*((volatile uint32_t *)(0xBFFFC100ul))
#define portMMCR_JTVIC_GIRQ24_SETEN		*((volatile uint32_t *)(0xBFFFC104ul))
#define portMMCR_JTVIC_GIRQ24_PRIA		*((volatile uint32_t *)(0xBFFFC400ul))

/*
By default port.c generates its tick interrupt from the RTOS timer.  The user
can override this behaviour by:
	1: Providing their own implementation of vApplicationSetupTickTimerInterrupt(),
	   which is the function that configures the timer.  The function is defined
	   as a weak symbol in this file so if the same function name is used in the
	   application code then the version in the application code will be linked
	   into the application in preference to the version defined in this file.
	2: Provide a vector implementation in port_asm.S that overrides the default
	   behaviour for the specified interrupt vector.
	3: Specify the correct bit to clear the interrupt during the timer interrupt
	   handler.
*/
#ifndef configTICK_INTERRUPT_VECTOR
	#define configTICK_INTERRUPT_VECTOR girq23_b4
	#define configCLEAR_TICK_TIMER_INTERRUPT() portMMCR_JTVIC_GIRQ23_SRC = portGIRQ23_RTOS_TIMER_MASK
#else
	#ifndef configCLEAR_TICK_TIMER_INTERRUPT
		#error If configTICK_INTERRUPT_VECTOR is defined in application code then configCLEAR_TICK_TIMER_INTERRUPT must also be defined in application code.
	#endif
#endif

/* Let the user override the pre-loading of the initial RA with the address of
prvTaskExitError() in case it messes up unwinding of the stack in the debugger -
in which case configTASK_RETURN_ADDRESS can be defined as 0 (NULL). */
#ifdef configTASK_RETURN_ADDRESS
	#define portTASK_RETURN_ADDRESS	configTASK_RETURN_ADDRESS
#else
	#define portTASK_RETURN_ADDRESS	prvTaskExitError
#endif

/* Set configCHECK_FOR_STACK_OVERFLOW to 3 to add ISR stack checking to task
stack checking.  A problem in the ISR stack will trigger an assert, not call the
stack overflow hook function (because the stack overflow hook is specific to a
task stack, not the ISR stack). */
#if( configCHECK_FOR_STACK_OVERFLOW > 2 )

	/* Don't use 0xa5 as the stack fill bytes as that is used by the kernel for
	the task stacks, and so will legitimately appear in many positions within
	the ISR stack. */
    #define portISR_STACK_FILL_BYTE	0xee

	static const uint8_t ucExpectedStackBytes[] = {
							portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,		\
							portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,		\
							portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,		\
							portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,		\
							portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE };	\

	#define portCHECK_ISR_STACK() configASSERT( ( memcmp( ( void * ) xISRStack, ( void * ) ucExpectedStackBytes, sizeof( ucExpectedStackBytes ) ) == 0 ) )
#else
	/* Define the function away. */
	#define portCHECK_ISR_STACK()
#endif /* configCHECK_FOR_STACK_OVERFLOW > 2 */


/*-----------------------------------------------------------*/

/*
 * Used to catch tasks that attempt to return from their implementing function.
 */
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

/* Records the interrupt nesting depth.  This is initialised to one as it is
decremented to 0 when the first task starts. */
volatile UBaseType_t uxInterruptNesting = 0x01;

/* Stores the task stack pointer when a switch is made to use the system stack. */
UBaseType_t uxSavedTaskStackPointer = 0;

/* The stack used by interrupt service routines that cause a context switch. */
StackType_t xISRStack[ configISR_STACK_SIZE ] = { 0 };

/* The top of stack value ensures there is enough space to store 6 registers on
the callers stack, as some functions seem to want to do this. */
const StackType_t * const xISRStackTop = &( xISRStack[ configISR_STACK_SIZE - 7 ] );

/*-----------------------------------------------------------*/

/*
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* Ensure byte alignment is maintained when leaving this function. */
	pxTopOfStack--;

	*pxTopOfStack = (StackType_t) 0xDEADBEEF;
	pxTopOfStack--;

	*pxTopOfStack = (StackType_t) 0x12345678;	/* Word to which the stack pointer will be left pointing after context restore. */
	pxTopOfStack--;

	*pxTopOfStack = (StackType_t) ulPortGetCP0Cause();
	pxTopOfStack--;

	*pxTopOfStack = (StackType_t) portINITIAL_SR;	/* CP0_STATUS */
	pxTopOfStack--;

	*pxTopOfStack = (StackType_t) pxCode; 		/* CP0_EPC */
	pxTopOfStack--;

	*pxTopOfStack = (StackType_t) portTASK_RETURN_ADDRESS;	/* ra */
	pxTopOfStack -= 15;

	*pxTopOfStack = (StackType_t) pvParameters; /* Parameters to pass in. */
	pxTopOfStack -= 15;

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

static __inline uint32_t prvDisableInterrupt( void )
{
uint32_t prev_state;

	__asm volatile( "di %0; ehb" : "=r" ( prev_state ) :: "memory" );
	return prev_state;
}
/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
	/* A function that implements a task must not exit or attempt to return to
	its caller as there is nothing to return to.  If a task wants to exit it
	should instead call vTaskDelete( NULL ).

	Artificially force an assert() to be triggered if configASSERT() is
	defined, then stop here so application writers can catch the error. */
	configASSERT( uxSavedTaskStackPointer == 0UL );
	portDISABLE_INTERRUPTS();
	for( ;; );
}
/*-----------------------------------------------------------*/

/*
 * Setup a timer for a regular tick.  This function uses the RTOS timer.
 * The function is declared weak so an application writer can use a different
 * timer by redefining this implementation.  If a different timer is used then
 * configTICK_INTERRUPT_VECTOR must also be defined in FreeRTOSConfig.h to
 * ensure the RTOS provided tick interrupt handler is installed on the correct
 * vector number.
 */
__attribute__(( weak )) void vApplicationSetupTickTimerInterrupt( void )
{
/* MEC14xx RTOS Timer whose input clock is 32KHz. */
const uint32_t ulPreload = ( 32768ul / ( configTICK_RATE_HZ ) );

	configASSERT( ulPreload != 0UL );

	/* Configure the RTOS timer. */
	portMMCR_RTMR_CONTROL = 0ul;
	portMMCR_RTMR_PRELOAD = ulPreload;

	/* Configure interrupts from the RTOS timer. */
	portMMCR_JTVIC_GIRQ23_SRC = ( portGIRQ23_RTOS_TIMER_MASK );
	portMMCR_JTVIC_GIRQ23_PRIA &= ~( 0x0Ful << 16 );
	portMMCR_JTVIC_GIRQ23_PRIA |= ( ( portIPL_TO_CODE( configKERNEL_INTERRUPT_PRIORITY ) ) << 16 );
	portMMCR_JTVIC_GIRQ23_SETEN = ( portGIRQ23_RTOS_TIMER_MASK );

	/* Enable the RTOS timer. */
	portMMCR_RTMR_CONTROL = 0x0Fu;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler(void)
{
	/* Not implemented in ports where there is nothing to return to.
	Artificially force an assert. */
	configASSERT( uxInterruptNesting == 1000UL );
}
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
extern void vPortStartFirstTask( void );
extern void *pxCurrentTCB;

	#if ( configCHECK_FOR_STACK_OVERFLOW > 2 )
	{
		/* Fill the ISR stack to make it easy to asses how much is being used. */
		memset( ( void * ) xISRStack, portISR_STACK_FILL_BYTE, sizeof( xISRStack ) );
	}
	#endif /* configCHECK_FOR_STACK_OVERFLOW > 2 */

	/* Clear the software interrupt flag. */
	portMMCR_JTVIC_GIRQ24_SRC = (portGIRQ24_M14K_SOFTIRQ0_MASK);

	/* Set software timer priority.  Each GIRQn has one nibble containing its
	priority */
	portMMCR_JTVIC_GIRQ24_PRIA &= ~(0xF0ul);
	portMMCR_JTVIC_GIRQ24_PRIA |= ( portIPL_TO_CODE( configKERNEL_INTERRUPT_PRIORITY ) << 4 );

	/* Enable software interrupt. */
	portMMCR_JTVIC_GIRQ24_SETEN = ( portGIRQ24_M14K_SOFTIRQ0_MASK );

	/* Setup the timer to generate the tick.  Interrupts will have been disabled
	by the time we get here. */
	vApplicationSetupTickTimerInterrupt();

	/* Start the highest priority task that has been created so far.  Its stack
	location is loaded into uxSavedTaskStackPointer. */
	uxSavedTaskStackPointer = *( UBaseType_t * ) pxCurrentTCB;
	vPortStartFirstTask();

	/* Should never get here as the tasks will now be executing!  Call the task
	exit error function to prevent compiler warnings about a static function
	not being called in the case that the application writer overrides this
	functionality by defining configTASK_RETURN_ADDRESS. */
	prvTaskExitError();

	return pdFALSE;
}
/*-----------------------------------------------------------*/

void vPortIncrementTick( void )
{
UBaseType_t uxSavedStatus;
uint32_t ulCause;

	uxSavedStatus = uxPortSetInterruptMaskFromISR();
	{
		if( xTaskIncrementTick() != pdFALSE )
		{
			/* Pend a context switch. */
			ulCause = ulPortGetCP0Cause();
			ulCause |= ( 1ul << 8UL );
			vPortSetCP0Cause( ulCause );
		}
	}
	vPortClearInterruptMaskFromISR( uxSavedStatus );

	/* Look for the ISR stack getting near or past its limit. */
	portCHECK_ISR_STACK();

	/* Clear timer interrupt. */
	configCLEAR_TICK_TIMER_INTERRUPT();
}
/*-----------------------------------------------------------*/

UBaseType_t uxPortSetInterruptMaskFromISR( void )
{
UBaseType_t uxSavedStatusRegister;

	prvDisableInterrupt();
	uxSavedStatusRegister = ulPortGetCP0Status() | 0x01;

	/* This clears the IPL bits, then sets them to
	configMAX_SYSCALL_INTERRUPT_PRIORITY.  This function should not be called
	from an interrupt that has a priority above
	configMAX_SYSCALL_INTERRUPT_PRIORITY so, when used correctly, the action
	can only result in the IPL being unchanged or raised, and therefore never
	lowered. */
	vPortSetCP0Status( ( ( uxSavedStatusRegister & ( ~portALL_IPL_BITS ) ) ) | ( configMAX_SYSCALL_INTERRUPT_PRIORITY << portIPL_SHIFT ) );

	return uxSavedStatusRegister;
}
/*-----------------------------------------------------------*/

void vPortClearInterruptMaskFromISR( UBaseType_t uxSavedStatusRegister )
{
	vPortSetCP0Status( uxSavedStatusRegister );
}
/*-----------------------------------------------------------*/





