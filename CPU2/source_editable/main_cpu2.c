
//
// Included Files
//
#include "../CPU2/include_editable/initial_header.h"
#include "../CPU2/include_editable/intercore_cpu2.h"

#define CPU2_TICK_DELAY_US 100000U

//
// Main
//
void main(void)
{
    Device_init();
    intercore_cpu2_init();

    /*
     * No easyDSP kernel on CPU2. CPU2 is monitored through the SINGLE CPU1
     * SCI-A easyDSP module: intercore_cpu2_service() mirrors CPU2 state into
     * g_cpu2_monitor (CPU2->CPU1 MSGRAM), which CPU1's easyDSP kernel reads.
     * The SCI-B module/header is no longer needed. (The CPU2 easy28x kernel
     * files can be excluded from this build.)
     */
    for (;;)
    {
        intercore_cpu2_service();
        DEVICE_DELAY_US(CPU2_TICK_DELAY_US);
    }
}

//
// End of File
//
