/*
 * FreeRTOS Kernel V10.6.1
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
	extern "C" {
#endif

/* BSP includes. */
#include "xil_types.h"

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the given hardware
 * and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR        char
#define portFLOAT       float
#define portDOUBLE      double
#define portLONG        long
#define portSHORT       short
#define portSTACK_TYPE  uint32_t
#define portBASE_TYPE   long

typedef portSTACK_TYPE StackType_t;
typedef long BaseType_t;
typedef unsigned long UBaseType_t;

typedef uint32_t TickType_t;
#define portMAX_DELAY ( TickType_t ) 0xffffffffUL

/*-----------------------------------------------------------*/

/* Hardware specifics. */
#define portSTACK_GROWTH            ( -1 )
#define portTICK_PERIOD_MS          ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT          8

/*-----------------------------------------------------------*/

/* Task utilities. */

/* Called at the end of an ISR that can cause a context switch. */
extern volatile uint32_t ulPortYieldRequired[2];
#define portEND_SWITCHING_ISR( xSwitchRequired )\
{												\
												\
	if( xSwitchRequired != pdFALSE )			\
	{											\
		ulPortYieldRequired[portGET_CORE_ID()] = pdTRUE;			\
	}											\
}

#define portYIELD_FROM_ISR( x ) portEND_SWITCHING_ISR( x )
#define portYIELD() __asm volatile ( "SWI 0" ::: "memory" );

/*-----------------------------------------------------------*/

/* Multi-core */
#define portMAX_CORE_COUNT    2

static inline unsigned int get_core_num()
{
    uint32_t reg_value;
    //MRC p15,0,<Rd>,c0,c0,5; read Multiprocessor ID register
    __asm__ volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r"(reg_value));
    //returns either 0 for core0 or 1 for core1
    return reg_value & 3U;
}

/* FreeRTOS core id is always zero based, so always 0 if we're running on only one core */
#if configNUMBER_OF_CORES == portMAX_CORE_COUNT
    #define portGET_CORE_ID()    get_core_num()
#else
    #define portGET_CORE_ID()    0
#endif

#define portRTOS_SPINLOCK_COUNT 2

//align to cache line
struct __attribute__((aligned(32))) spin_lock_t
{
    uint32_t ucLock;
    uint8_t ucOwnedByCore[ portMAX_CORE_COUNT ];
    uint8_t ucRecursionCountByLock;
};

static inline int spin_try_lock_unsafe(struct spin_lock_t * pxSpinLock)
{
    uint32_t zero = 0;            
    if (__atomic_compare_exchange_n(&pxSpinLock->ucLock, &zero, 1, FALSE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    {
        configASSERT( pxSpinLock->ucRecursionCountByLock == 0 );
        return 1;
    }
    return 0;
}

static inline void spin_lock_unsafe_blocking(struct spin_lock_t * lock)
{
    while (1)
    {
        uint32_t zero = 0;            
        if (__atomic_compare_exchange_n(&lock->ucLock, &zero, 1, TRUE, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
        {
            configASSERT( lock->ucRecursionCountByLock == 0 );
            break;
        }
    }
}

static inline void spin_unlock_unsafe(struct spin_lock_t * lock)
{
    __atomic_store_n(&lock->ucLock, 0, __ATOMIC_RELEASE);
}

/* Note this is a single method with uxAcquire parameter since we have
 * static vars, the method is always called with a compile time constant for
 * uxAcquire, and the compiler should do the right thing! */
static inline void vPortRecursiveLock(BaseType_t xCoreId, uint32_t ulLockNum, BaseType_t uxAcquire)
{
    static struct spin_lock_t xSpinLocks[portRTOS_SPINLOCK_COUNT];
    
    configASSERT( ulLockNum < portRTOS_SPINLOCK_COUNT );

    if( uxAcquire )
    {
        if (!spin_try_lock_unsafe(&xSpinLocks[ulLockNum])) {
            if( xSpinLocks[ulLockNum].ucOwnedByCore[xCoreId] )
            {
                configASSERT( xSpinLocks[ulLockNum].ucRecursionCountByLock != 255u );
                xSpinLocks[ulLockNum].ucRecursionCountByLock = xSpinLocks[ulLockNum].ucRecursionCountByLock + 1;
                return;
            }
            spin_lock_unsafe_blocking(&xSpinLocks[ulLockNum]);
        }
        configASSERT( xSpinLocks[ulLockNum].ucRecursionCountByLock == 0 );
        xSpinLocks[ulLockNum].ucRecursionCountByLock = 1;
        xSpinLocks[ulLockNum].ucOwnedByCore[xCoreId] = 1;
    }
    else
    {
        configASSERT( ( xSpinLocks[ulLockNum].ucOwnedByCore[xCoreId] != 0 ));
        configASSERT( xSpinLocks[ulLockNum].ucRecursionCountByLock != 0 );

        xSpinLocks[ulLockNum].ucRecursionCountByLock = xSpinLocks[ulLockNum].ucRecursionCountByLock - 1;
        if( xSpinLocks[ulLockNum].ucRecursionCountByLock == 0U )
        {
            xSpinLocks[ulLockNum].ucOwnedByCore[xCoreId] = 0;
            spin_unlock_unsafe(&xSpinLocks[ulLockNum]);
        }
    }
}

#if ( configNUMBER_OF_CORES == 1 )
    #define portGET_ISR_LOCK( xCoreID )
    #define portRELEASE_ISR_LOCK( xCoreID )
    #define portGET_TASK_LOCK( xCoreID )
    #define portRELEASE_TASK_LOCK( xCoreID )
#else
    #define portGET_ISR_LOCK( xCoreID )         vPortRecursiveLock( ( xCoreID ), 0, pdTRUE )
    #define portRELEASE_ISR_LOCK( xCoreID )     vPortRecursiveLock( ( xCoreID ), 0, pdFALSE )
    #define portGET_TASK_LOCK( xCoreID )        vPortRecursiveLock( ( xCoreID ), 1, pdTRUE )
    #define portRELEASE_TASK_LOCK( xCoreID )    vPortRecursiveLock( ( xCoreID ), 1, pdFALSE )
#endif

extern void vYieldCore( int xCoreID );
#define portYIELD_CORE( a )                  vYieldCore( a )

/*-----------------------------------------------------------
 * Critical section control
 *----------------------------------------------------------*/

extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );
extern uint32_t ulPortSetInterruptMask( void );
extern void vPortClearInterruptMask( uint32_t ulNewMaskValue );
extern void vPortInstallFreeRTOSVectorTable( void );
extern void vPortEnableInterrupts();
extern void vPortDisableInterrupts();
extern void vTaskEnterCritical( void );
extern void vTaskExitCritical( void );
extern UBaseType_t vTaskEnterCriticalFromISR( void );
extern void vTaskExitCriticalFromISR( UBaseType_t uxSavedInterruptStatus );

/* These macros do not globally disable/enable interrupts.  They do mask off
interrupts that have a priority below configMAX_API_CALL_INTERRUPT_PRIORITY. */
#define portENTER_CRITICAL()        vTaskEnterCritical();
#define portEXIT_CRITICAL()         vTaskExitCritical();
#define portDISABLE_INTERRUPTS()    vPortDisableInterrupts()
#define portENABLE_INTERRUPTS()     vPortEnableInterrupts()
#define portENTER_CRITICAL_FROM_ISR() vTaskEnterCriticalFromISR()
#define portEXIT_CRITICAL_FROM_ISR( x ) vTaskExitCriticalFromISR( x )

#define portSET_INTERRUPT_MASK()    ulPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK( ulState ) vPortClearInterruptMask( ulState )

#define portHAS_NESTED_INTERRUPTS               1
#define portSET_INTERRUPT_MASK_FROM_ISR()       ulPortSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)    vPortClearInterruptMask(x)

/*-----------------------------------------------------------*/

/* Critical nesting count management. */
#define portCRITICAL_NESTING_IN_TCB    0

extern volatile uint32_t ulCriticalNestings[ 2 ];
#define portGET_CRITICAL_NESTING_COUNT( xCoreID )          ( ulCriticalNestings[ ( xCoreID ) ] )
#define portSET_CRITICAL_NESTING_COUNT( xCoreID, x )       ( ulCriticalNestings[ ( xCoreID ) ] = ( x ) )
#define portINCREMENT_CRITICAL_NESTING_COUNT( xCoreID )    ( ulCriticalNestings[ ( xCoreID ) ]++ )
#define portDECREMENT_CRITICAL_NESTING_COUNT( xCoreID )    ( ulCriticalNestings[ ( xCoreID ) ]-- )

/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site.  These are
not required for this port but included in case common demo code that uses these
macros is used. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )  void vFunction( void *pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )    void vFunction( void *pvParameters )

/* Prototype of the FreeRTOS tick handler.  This must be installed as the
handler for whichever peripheral is used to generate the RTOS tick. */
void FreeRTOS_Tick_Handler( void );

int Setup_Software_Intr( void );

/*
 * Installs pxHandler as the interrupt handler for the peripheral specified by
 * the ucInterruptID parameter.
 *
 * ucInterruptID:
 *
 * The ID of the peripheral that will have pxHandler assigned as its interrupt
 * handler.  Peripheral IDs are defined in the xparameters.h header file, which
 * is itself part of the BSP project.
 *
 * pxHandler:
 *
 * A pointer to the interrupt handler function itself.  This must be a void
 * function that takes a (void *) parameter.
 *
 * pvCallBackRef:
 *
 * The parameter passed into the handler function.  In many cases this will not
 * be used and can be NULL.  Some times it is used to pass in a reference to
 * the peripheral instance variable, so it can be accessed from inside the
 * handler function.
 *
 * pdPASS is returned if the function executes successfully.  Any other value
 * being returned indicates that the function did not execute correctly.
 */
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
BaseType_t xPortInstallInterruptHandler( uint8_t ucInterruptID, XInterruptHandler pxHandler, void *pvCallBackRef );
#else
BaseType_t xPortInstallInterruptHandler( uint16_t ucInterruptID, void *pxHandler, void *pvCallBackRef );
#endif
/*
 * Enables the interrupt, within the interrupt controller, for the peripheral
 * specified by the ucInterruptID parameter.
 *
 * ucInterruptID:
 *
 * The ID of the peripheral that will have its interrupt enabled in the
 * interrupt controller.  Peripheral IDs are defined in the xparameters.h header
 * file, which is itself part of the BSP project.
 */
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
void vPortEnableInterrupt( uint8_t ucInterruptID );
#else
void vPortEnableInterrupt( uint16_t ucInterruptID );
#endif
/*
 * Disables the interrupt, within the interrupt controller, for the peripheral
 * specified by the ucInterruptID parameter.
 *
 * ucInterruptID:
 *
 * The ID of the peripheral that will have its interrupt disabled in the
 * interrupt controller.  Peripheral IDs are defined in the xparameters.h header
 * file, which is itself part of the BSP project.
 */
#if !defined(XPAR_XILTIMER_ENABLED) && !defined(SDT)
void vPortDisableInterrupt( uint8_t ucInterruptID );
#else
void vPortDisableInterrupt( uint16_t ucInterruptID );
#endif
/* If configUSE_TASK_FPU_SUPPORT is set to 1 (or left undefined) then tasks are
created without an FPU context and must call vPortTaskUsesFPU() to give
themselves an FPU context before using any FPU instructions.  If
configUSE_TASK_FPU_SUPPORT is set to 2 then all tasks will have an FPU context
by default. */
#if( configUSE_TASK_FPU_SUPPORT != 2 )
    void vPortTaskUsesFPU( void );
#else
    /* Each task has an FPU context already, so define this function away to
    nothing to prevent it being called accidentally. */
    #define vPortTaskUsesFPU()
#endif
#define portTASK_USES_FLOATING_POINT() vPortTaskUsesFPU()

#define portLOWEST_INTERRUPT_PRIORITY ( ( ( uint32_t ) configUNIQUE_INTERRUPT_PRIORITIES ) - 1UL )
#define portLOWEST_USABLE_INTERRUPT_PRIORITY ( portLOWEST_INTERRUPT_PRIORITY - 1UL )

/* Architecture specific optimisations. */
#ifndef configUSE_PORT_OPTIMISED_TASK_SELECTION
    #define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#endif

#if configUSE_PORT_OPTIMISED_TASK_SELECTION == 1

    /* Store/clear the ready priorities in a bit map. */
    #define portRECORD_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) |= ( 1UL << ( uxPriority ) )
    #define portRESET_READY_PRIORITY( uxPriority, uxReadyPriorities ) ( uxReadyPriorities ) &= ~( 1UL << ( uxPriority ) )

    /*-----------------------------------------------------------*/

    #define portGET_HIGHEST_PRIORITY( uxTopPriority, uxReadyPriorities ) uxTopPriority = ( 31UL - ( uint32_t ) __builtin_clz( uxReadyPriorities ) )

#endif /* configUSE_PORT_OPTIMISED_TASK_SELECTION */

#ifdef configASSERT
    void vPortValidateInterruptPriority( void );
    #define portASSERT_IF_INTERRUPT_PRIORITY_INVALID()  vPortValidateInterruptPriority()
#endif /* configASSERT */

#define portNOP() __asm volatile( "NOP" )
#define portINLINE __inline

/* The number of bits to shift for an interrupt priority is dependent on the
number of bits implemented by the interrupt controller. */
#if configUNIQUE_INTERRUPT_PRIORITIES == 16
    #define portPRIORITY_SHIFT 4
    #define portMAX_BINARY_POINT_VALUE  3
#elif configUNIQUE_INTERRUPT_PRIORITIES == 32
    #define portPRIORITY_SHIFT 3
    #define portMAX_BINARY_POINT_VALUE  2
#elif configUNIQUE_INTERRUPT_PRIORITIES == 64
    #define portPRIORITY_SHIFT 2
    #define portMAX_BINARY_POINT_VALUE  1
#elif configUNIQUE_INTERRUPT_PRIORITIES == 128
    #define portPRIORITY_SHIFT 1
    #define portMAX_BINARY_POINT_VALUE  0
#elif configUNIQUE_INTERRUPT_PRIORITIES == 256
    #define portPRIORITY_SHIFT 0
    #define portMAX_BINARY_POINT_VALUE  0
#else
    #error Invalid configUNIQUE_INTERRUPT_PRIORITIES setting.  configUNIQUE_INTERRUPT_PRIORITIES must be set to the number of unique priorities implemented by the target hardware
#endif

/* Interrupt controller access addresses. */
#define portICCPMR_PRIORITY_MASK_OFFSET                         ( 0x04 )
#define portICCIAR_INTERRUPT_ACKNOWLEDGE_OFFSET                 ( 0x0C )
#define portICCEOIR_END_OF_INTERRUPT_OFFSET                     ( 0x10 )
#define portICCBPR_BINARY_POINT_OFFSET                          ( 0x08 )
#define portICCRPR_RUNNING_PRIORITY_OFFSET                      ( 0x14 )

#define portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS      ( configINTERRUPT_CONTROLLER_BASE_ADDRESS + configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET )
#define portICCPMR_PRIORITY_MASK_REGISTER                   ( *( ( volatile uint32_t * ) ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCPMR_PRIORITY_MASK_OFFSET ) ) )
#define portICCIAR_INTERRUPT_ACKNOWLEDGE_REGISTER_ADDRESS   ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCIAR_INTERRUPT_ACKNOWLEDGE_OFFSET )
#define portICCEOIR_END_OF_INTERRUPT_REGISTER_ADDRESS       ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCEOIR_END_OF_INTERRUPT_OFFSET )
#define portICCPMR_PRIORITY_MASK_REGISTER_ADDRESS           ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCPMR_PRIORITY_MASK_OFFSET )
#define portICCBPR_BINARY_POINT_REGISTER                    ( *( ( const volatile uint32_t * ) ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCBPR_BINARY_POINT_OFFSET ) ) )
#define portICCRPR_RUNNING_PRIORITY_REGISTER                ( *( ( const volatile uint32_t * ) ( portINTERRUPT_CONTROLLER_CPU_INTERFACE_ADDRESS + portICCRPR_RUNNING_PRIORITY_OFFSET ) ) )

#define portMEMORY_BARRIER() __asm volatile( "" ::: "memory" )

#ifndef porttraceSGI_HANDLER
    #define porttraceSGI_HANDLER(yieldCount, handleCount)
#endif

#ifndef portENABLE_FPU_SAFE_IRQ_HANDLER
    #define portENABLE_FPU_SAFE_IRQ_HANDLER 0
#endif

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* PORTMACRO_H */
