
#ifndef PORTDEFAULTS_H
#define PORTDEFAULTS_H

#ifdef __cplusplus
	extern "C" {
#endif

#ifndef portENABLE_FPU_SAFE_IRQ_HANDLER
    #define portENABLE_FPU_SAFE_IRQ_HANDLER 0
#endif

#ifndef portENABLE_CHECK_FPU_SAFE_IRQ_IS_NEEDED
    #define portENABLE_CHECK_FPU_SAFE_IRQ_IS_NEEDED 0
#endif

#if ( ( portENABLE_CHECK_FPU_SAFE_IRQ_IS_NEEDED == 1 ) && ( portENABLE_FPU_SAFE_IRQ_HANDLER == 0 ) )
    #error portENABLE_FPU_SAFE_IRQ_HANDLER must be 1 in order to use portENABLE_CHECK_FPU_SAFE_IRQ_IS_NEEDED
#endif

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* PORTDEFAULTS_H */
