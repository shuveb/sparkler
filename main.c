#include <err.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <cpuid.h>
#include <termios.h>
#include "fetchnparse.h"

/* Port definitions for the devices we emulate */
#define SERIAL_PORT                     0x3f8
#define TWITTER_DEVICE                  0x100
#define WEATHER_DEVICE_CHENNAI          0x101
#define WEATHER_DEVICE_DELHI            0x102
#define WEATHER_DEVICE_LONDON           0x103
#define WEATHER_DEVICE_CHICAGO          0x104
#define WEATHER_DEVICE_SFO              0x105
#define WEATHER_DEVICE_NY               0x106
#define AIR_QUALITY_DEVICE_CHENNAI      0x201
#define AIR_QUALITY_DEVICE_DELHI        0x202
#define AIR_QUALITY_DEVICE_LONDON       0x203
#define AIR_QUALITY_DEVICE_CHICAGO      0x204
#define AIR_QUALITY_DEVICE_SFO          0x205
#define AIR_QUALITY_DEVICE_NY           0x206

/*
 * There is no getch() under Linux, so we need to roll our own:
 * Credits to:
 * https://stackoverflow.com/questions/7469139/what-is-the-equivalent-to-getch-getche-in-linux
 * */

static struct termios old, current;

/* Initialize new terminal i/o settings */
void initTermios(int echo)
{
    tcgetattr(0, &old); /* grab old terminal i/o settings */
    current = old; /* make new settings same as old settings */
    current.c_lflag &= ~ICANON; /* disable buffered i/o */
    if (echo) {
        current.c_lflag |= ECHO; /* set echo mode */
    } else {
        current.c_lflag &= ~ECHO; /* set no echo mode */
    }
    tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void)
{
    tcsetattr(0, TCSANOW, &old);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo)
{
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

/* Read 1 character without echo */
char getch(void)
{
    return getch_(0);
}

/* Read 1 character with echo */
char getche(void)
{
    return getch_(1);
}

int main(void)
{
    int kvm, vmfd, vcpufd, ret;
    uint8_t stub[512];

    uint8_t *mem;
    struct kvm_sregs sregs;
    size_t mmap_size;
    struct kvm_run *run;

    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm == -1)
        err(1, "/dev/kvm");

    vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long)0);
    if (vmfd == -1)
        err(1, "KVM_CREATE_VM");

    /* Allocate one aligned page of guest memory to hold the code. */
    mem = mmap(NULL, 0x8000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!mem)
        err(1, "allocating guest memory");

    /* Read our monitor program into RAM */
    int fd = open("monitor", O_RDONLY);
    if (fd == -1)
        err(1, "Unable to open stub");
    struct stat st;
    fstat(fd, &st);
    read(fd, mem, st.st_size);

    struct kvm_userspace_memory_region region = {
            .slot = 0,
            .guest_phys_addr = 0x1000,
            .memory_size = 0x8000,
            .userspace_addr = (uint64_t)mem,
    };
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        err(1, "KVM_SET_USER_MEMORY_REGION");

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long)0);
    if (vcpufd == -1)
        err(1, "KVM_CREATE_VCPU");

    /* Map the shared kvm_run structure and following data. */
    ret = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        err(1, "KVM_GET_VCPU_MMAP_SIZE");
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        errx(1, "KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    run = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
    if (!run)
        err(1, "mmap vcpu");

    /* Set CPUID */
    struct kvm_cpuid2 *cpuid;
    int nent = 40;
    unsigned long size = sizeof(*cpuid) + nent * sizeof(*cpuid->entries);
    cpuid = (struct kvm_cpuid2*) malloc(size);
    bzero(cpuid, size);
    cpuid->nent = nent;

    ret = ioctl(kvm, KVM_GET_SUPPORTED_CPUID, cpuid);
    if (ret < 0) {
        free(cpuid);
        err(1, "KVM_GET_SUPPORTED_CPUID");
    }

    for (int i = 0; i < cpuid->nent; i++) {
        if (cpuid->entries[i].function == 0x80000002)
            __get_cpuid(0x80000002, &cpuid->entries[i].eax, &cpuid->entries[i].ebx, &cpuid->entries[i].ecx, &cpuid->entries[i].edx);
        if (cpuid->entries[i].function == 0x80000003)
            __get_cpuid(0x80000003, &cpuid->entries[i].eax, &cpuid->entries[i].ebx, &cpuid->entries[i].ecx, &cpuid->entries[i].edx);
        if (cpuid->entries[i].function == 0x80000004)
            __get_cpuid(0x80000004, &cpuid->entries[i].eax, &cpuid->entries[i].ebx, &cpuid->entries[i].ecx, &cpuid->entries[i].edx);
    }

    ret = ioctl(vcpufd, KVM_SET_CPUID2, cpuid);
    if (ret < 0) {
        free(cpuid);
        err(1, "KVM_SET_CPUID2");
    }
    free(cpuid);

    /* Initialize CS to point at 0, via a read-modify-write of sregs. */
    ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_GET_SREGS");
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        err(1, "KVM_SET_SREGS");

    /* Initialize registers: instruction pointer for our code, addends, and
     * initial flags required by x86 architecture. */
    struct kvm_regs regs = {
            .rip = 0x1000,
            .rflags = 0x2,
    };
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        err(1, "KVM_SET_REGS");

    char *latest_tweet      = NULL;
    char *weather_forecast  = NULL;
    char *aq_report         = NULL;
    int tweet_str_idx       = 0;
    int weather_str_idx     = 0;
    int aq_str_idx          = 0;

    /* Run the VM while handling any exits for device emulation */
    while (1) {
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret == -1)
            err(1, "KVM_RUN");
        switch (run->exit_reason) {
            case KVM_EXIT_HLT:
                puts("KVM_EXIT_HLT");
                return 0;
            case KVM_EXIT_IO:
                if (run->io.direction == KVM_EXIT_IO_OUT) {
                    switch (run->io.port) {
                        case SERIAL_PORT:
                            putchar(*(((char *)run) + run->io.data_offset));
                            break;
                        default:
                            printf("Port: 0x%x\n", run->io.port);
                            errx(1, "unhandled KVM_EXIT_IO");
                    }
                } else {
                    /* KVM_EXIT_IO_IN */
                    switch (run->io.port) {
                        case SERIAL_PORT:
                            *(((char *)run) + run->io.data_offset) = getche();
                            break;
                        case TWITTER_DEVICE:
                            if (latest_tweet == NULL)
                                latest_tweet = fetch_latest_tweet();
                            char tweet_chr = *(latest_tweet + tweet_str_idx);
                            *(((char *)run) + run->io.data_offset) = tweet_chr;
                            tweet_str_idx++;
                            if (tweet_chr == '\0') {
                                free(latest_tweet);
                                latest_tweet = NULL;
                                tweet_str_idx = 0;
                            }
                            break;
                        case WEATHER_DEVICE_CHENNAI:
                        case WEATHER_DEVICE_DELHI:
                        case WEATHER_DEVICE_LONDON:
                        case WEATHER_DEVICE_CHICAGO:
                        case WEATHER_DEVICE_SFO:
                        case WEATHER_DEVICE_NY:
                            if (weather_forecast == NULL) {
                                char city[64];
                                if (run->io.port == WEATHER_DEVICE_CHENNAI)
                                    strncpy(city, "Chennai", sizeof(city));
                                else if (run->io.port == WEATHER_DEVICE_DELHI)
                                    strncpy(city, "New%20Delhi", sizeof(city));
                                else if (run->io.port == WEATHER_DEVICE_LONDON)
                                    strncpy(city, "London", sizeof(city));
                                else if (run->io.port == WEATHER_DEVICE_CHICAGO)
                                    strncpy(city, "Chicago", sizeof(city));
                                else if (run->io.port == WEATHER_DEVICE_SFO)
                                    strncpy(city, "San%20Francisco", sizeof(city));
                                else if (run->io.port == WEATHER_DEVICE_NY)
                                    strncpy(city, "New%20York", sizeof(city));

                                weather_forecast = fetch_weather(city);
                            }
                            char weather_chr = *(weather_forecast + weather_str_idx);
                            *(((char *)run) + run->io.data_offset) = weather_chr;
                            weather_str_idx++;
                            if (weather_chr == '\0') {
                                free(weather_forecast);
                                weather_forecast = NULL;
                                weather_str_idx = 0;
                            }
                            break;
                        case AIR_QUALITY_DEVICE_CHENNAI:
                        case AIR_QUALITY_DEVICE_DELHI:
                        case AIR_QUALITY_DEVICE_LONDON:
                        case AIR_QUALITY_DEVICE_CHICAGO:
                        case AIR_QUALITY_DEVICE_SFO:
                        case AIR_QUALITY_DEVICE_NY:
                            if (aq_report == NULL) {
                                char city[64];
                                char country[3];
                                if (run->io.port == AIR_QUALITY_DEVICE_CHENNAI) {
                                    strncpy(city, "Chennai", sizeof(city));
                                    strncpy(country, "IN", sizeof(country));
                                }
                                else if (run->io.port == AIR_QUALITY_DEVICE_DELHI) {
                                    strncpy(city, "Delhi", sizeof(city));
                                    strncpy(country, "IN", sizeof(country));
                                }
                                else if (run->io.port == AIR_QUALITY_DEVICE_LONDON) {
                                    strncpy(city, "London", sizeof(city));
                                    strncpy(country, "GB", sizeof(country));
                                }
                                else if (run->io.port == AIR_QUALITY_DEVICE_CHICAGO) {
                                    strncpy(city, "Chicago-Naperville-Joliet", sizeof(city));
                                    strncpy(country, "US", sizeof(country));
                                }
                                else if (run->io.port == AIR_QUALITY_DEVICE_SFO) {
                                    strncpy(city, "San%20Francisco-Oakland-Fremont", sizeof(city));
                                    strncpy(country, "US", sizeof(country));
                                }
                                else if (run->io.port == AIR_QUALITY_DEVICE_NY) {
                                    strncpy(city, "New%20York-Northern%20New%20Jersey-Long%20Island", sizeof(city));
                                    strncpy(country, "US", sizeof(country));
                                }
                                aq_report = fetch_air_quality(country, city);
                            }
                            char aq_chr = *(aq_report + aq_str_idx);
                            *(((char *)run) + run->io.data_offset) = aq_chr;
                            aq_str_idx++;
                            if (aq_chr == '\0') {
                                free(aq_report);
                                aq_report = NULL;
                                aq_str_idx = 0;
                            }
                            break;
                        default:
                            printf("Port: 0x%x\n", run->io.port);
                            errx(1, "unhandled KVM_EXIT_IO");
                    }
                }

                break;
            case KVM_EXIT_FAIL_ENTRY:
                errx(1, "KVM_EXIT_FAIL_ENTRY: hardware_entry_failure_reason = 0x%llx",
                     (unsigned long long)run->fail_entry.hardware_entry_failure_reason);
            case KVM_EXIT_INTERNAL_ERROR:
                errx(1, "KVM_EXIT_INTERNAL_ERROR: suberror = 0x%x", run->internal.suberror);
            default:
                errx(1, "exit_reason = 0x%x", run->exit_reason);
        }
    }
}

