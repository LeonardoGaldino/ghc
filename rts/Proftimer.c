/* -----------------------------------------------------------------------------
 *
 * (c) The GHC Team, 1998-1999
 *
 * Profiling interval timer
 *
 * ---------------------------------------------------------------------------*/

#include "PosixSource.h"
#include "Rts.h"

#include "Profiling.h"
#include "Proftimer.h"
#include "Capability.h"

 #include "rapl-read2.h"

#ifdef PROFILING
static rtsBool do_prof_ticks = rtsFalse;       // enable profiling ticks
static CostCentreStack *previous_ccs[10];
#endif

static rtsBool do_heap_prof_ticks = rtsFalse;  // enable heap profiling ticks

// Number of ticks until next heap census
static int ticks_to_heap_profile;

// Time for a heap profile on the next context switch
rtsBool performHeapProfile;

void
stopProfTimer( void )
{
#ifdef PROFILING
    do_prof_ticks = rtsFalse;
#endif
}

void
startProfTimer( void )
{
#ifdef PROFILING
    do_prof_ticks = rtsTrue;
#endif
}

void
stopHeapProfTimer( void )
{
    do_heap_prof_ticks = rtsFalse;
}

void
startHeapProfTimer( void )
{
    if (RtsFlags.ProfFlags.doHeapProfile &&
        RtsFlags.ProfFlags.heapProfileIntervalTicks > 0) {
        do_heap_prof_ticks = rtsTrue;
    }
}

void
initProfTimer( void )
{
    performHeapProfile = rtsFalse;

    ticks_to_heap_profile = RtsFlags.ProfFlags.heapProfileIntervalTicks;

    startHeapProfTimer();

    init_rapl_read();
}

nat total_ticks = 0;
StgDouble last_energy_pkg = 0.0;
StgDouble last_energy_dram = 0.0;

void
handleProfTick(void)
{
#ifdef PROFILING
    total_ticks++;
    if (do_prof_ticks) {
        StgDouble current_energy_pkg  = get_package_energy();
        StgDouble current_energy_dram = get_dram_energy();
        StgDouble e_diff_pkg = total_ticks == 1 ? 0 : current_energy_pkg - last_energy_pkg;
        StgDouble e_diff_dram = total_ticks == 1 ? 0 : current_energy_dram - last_energy_dram;
        last_energy_pkg = current_energy_pkg;
        last_energy_dram = current_energy_dram;

        nat n;
        CostCentreStack *ccs, *prev_ccs;
        for (n=0; n < n_capabilities; n++) {
            ccs = capabilities[n]->r.rCCCS;
            ccs->time_ticks++;

            // There is no previous cost centre in the first tick
            if (total_ticks == 1) {
                previous_ccs[n] = ccs;
                continue;
            }

            prev_ccs = previous_ccs[n];
            prev_ccs->e_counter_pkg += e_diff_pkg;
            prev_ccs->e_counter_dram += e_diff_dram;
            previous_ccs[n] = ccs;
        }
    }
#endif

    if (do_heap_prof_ticks) {
        ticks_to_heap_profile--;
        if (ticks_to_heap_profile <= 0) {
            ticks_to_heap_profile = RtsFlags.ProfFlags.heapProfileIntervalTicks;
            performHeapProfile = rtsTrue;
        }
    }
}
