#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include "rapl-read2.h"

#define MSR_RAPL_POWER_UNIT 0x606

/*
 * Platform specific RAPL Domains.
 * Note that PP1 RAPL Domain is supported on 062A only
 * And DRAM RAPL Domain is supported on 062D only
 */
/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT 0x610
#define MSR_PKG_ENERGY_STATUS    0x611
#define MSR_PKG_PERF_STATUS      0x613
#define MSR_PKG_POWER_INFO       0x614

/* DRAM RAPL Domain */
#define MSR_DRAM_ENERGY_STATUS   0x619


/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT   0x638
#define MSR_PP0_ENERGY_STATUS 0x639
#define MSR_PP0_POLICY        0x63A
#define MSR_PP0_PERF_STATUS   0x63B

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET 0
#define POWER_UNIT_MASK   0x0F

#define ENERGY_UNIT_OFFSET 0x08
#define ENERGY_UNIT_MASK   0x1F00

#define TIME_UNIT_OFFSET 0x10
#define TIME_UNIT_MASK   0xF000

#define CPU_SANDYBRIDGE    42
#define CPU_SANDYBRIDGE_EP 45
#define CPU_IVYBRIDGE      58
#define CPU_IVYBRIDGE_EP   62
#define CPU_HASWELL        60


static int core;
static int cpu_model;
static int fd;
static double power_units, energy_units, time_units;


static int open_msr(int core) {
    char msr_filename[BUFSIZ];
    int fd;

    sprintf(msr_filename, "/dev/cpu/%d/msr", core);
    fd = open(msr_filename, O_RDONLY);
    if (fd < 0) {
        if (errno == ENXIO) {
            fprintf(stderr, "rdmsr: No CPU %d\n", core);
        } else if ( errno == EIO ) {
            fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
        } else {
            perror("rdmsr:open");
            fprintf(stderr,"Trying to open %s\n",msr_filename);
        }

        return -1;
    }

    return fd;
}

static long long read_msr(int fd, int which) {
    uint64_t data;

    if (pread(fd, &data, sizeof data, which) != sizeof data) {
        perror("rdmsr:pread");
        exit(127);
    }

    return (long long) data;
}

static int detect_cpu(void) {
    FILE *fff;

    int family, model = -1;
    char buffer[BUFSIZ],*result;
    char vendor[BUFSIZ];

    fff = fopen("/proc/cpuinfo","r");
    if (fff == NULL)
        return -1;

    while (1) {
        result = fgets(buffer, BUFSIZ, fff);
        if (result == NULL)
            break;

        if (!strncmp(result, "vendor_id", 8)) {
            sscanf(result, "%*s%*s%s", vendor);

            if (strncmp(vendor, "GenuineIntel", 12)) {
                printf("%s not an Intel chip\n",vendor);
                return -1;
            }
        }

        if (!strncmp(result, "cpu family", 10)) {
            sscanf(result, "%*s%*s%*s%d", &family);
            if (family != 6) {
                printf("Wrong CPU family %d\n",family);
                return -1;
            }
        }

        if (!strncmp(result, "model", 5)) {
            sscanf(result, "%*s%*s%d", &model);
        }
    }

    fclose(fff);
    return model;
}

int init_rapl_read(void) {
    core = 0;

    cpu_model = detect_cpu();
    if (cpu_model < 0) {
        printf("Unsupported CPU type\n");
        return -1;
    }

    fd = open_msr(core);
    if (fd == -1)
        return -1;

    long long result = read_msr(fd, MSR_RAPL_POWER_UNIT);

    power_units = pow(0.5, (double)(result & 0xf));
    energy_units = pow(0.5, (double)((result >> 8) & 0x1f));
    time_units = pow(0.5, (double)((result >>16) & 0xf));

    return 0;
}

double get_package_energy(void) {
    if (fd == -1)
        return -1;

    long long result = read_msr(fd, MSR_PKG_ENERGY_STATUS);
    return (double) result * energy_units;
}

double get_dram_energy(void) {
    if (fd == -1)
        return -1;

    long long result = read_msr(fd, MSR_DRAM_ENERGY_STATUS);
    return (double) result * energy_units;
}

double get_pp0_energy(void) {
    if (fd == -1)
        return -1;

    long long result = read_msr(fd, MSR_PP0_ENERGY_STATUS);
    return (double) result * energy_units;
}

int get_pp0_policy(void) {
    if (fd == -1)
        return -1;

    long long result = read_msr(fd, MSR_PP0_POLICY);
    return (int) result & 0x001f;
}

/*int main(int argc, char **argv) {
    double package_before, package_after;
    double pp0_before, pp0_after;

    printf("\n");

    init_rapl_read();

    package_before = get_package_energy();
    printf("Package energy before: %.6fJ\n", package_before);

    pp0_before = get_pp0_energy();
    printf("PowerPlane0 (core) for core %d energy before: %.6fJ\n", core, pp0_before);

    int pp0_policy = get_pp0_policy();
    printf("PowerPlane0 (core) for core %d policy: %d\n", core, pp0_policy);


    printf("\nSleeping 2 second\n\n");
    sleep(2);

    package_after = get_package_energy();
    printf("Package energy after: %.6f  (%.6fJ consumed)\n",
       package_after,package_after-package_before);

    pp0_after = get_pp0_energy();
    printf("PowerPlane0 (core) for core %d energy after: %.6f  (%.6fJ consumed)\n",
       core,pp0_after,pp0_after-pp0_before);

    printf("\n");
    printf("Note: the energy measurements can overflow in 60s or so\n");
    printf("      so try to sample the counters more often than that.\n\n");
    close(fd);

    return 0;
}*/
