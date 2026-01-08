#pragma once

//#define PERFORMANCE_COUNTER
#ifdef PERFORMANCE_COUNTER
#include <linux/perf_event.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <unistd.h>

class PerformanceCounter {
        perf_event_attr pe = {};
        long long count = 0;
        int fd = -1;
    public:
        explicit PerformanceCounter(int cacheType = PERF_COUNT_HW_CACHE_L1D,
                           int cacheOp = PERF_COUNT_HW_CACHE_OP_READ,
                           int cacheResult = PERF_COUNT_HW_CACHE_RESULT_MISS) {
            memset(&pe, 0, sizeof(perf_event_attr));
            pe.type = PERF_TYPE_HW_CACHE;
            pe.size = sizeof(perf_event_attr);
            pe.config = cacheType | (cacheOp << 8) | (cacheResult << 16);
            pe.disabled = 1;
            pe.exclude_kernel = 1;
            pe.exclude_hv = 1; // Don't count hypervisor events.
            fd = syscall(__NR_perf_event_open, /*hw_event*/ &pe, /*pid*/ 0, /*cpu*/ -1,
                         /*group_fd*/ -1, /*flags*/ 0);
            if (fd == -1) {
                fprintf(stderr, "Error opening leader %llx\n", pe.config);
                exit(EXIT_FAILURE);
            }
        }

        void start() const {
            ioctl(fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        }

        void stop() {
            ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
            read(fd, &count, sizeof(long long));
            close(fd);
        }

        [[nodiscard]] long long getCount() const {
            return count;
        }
};
#else
/** Stub for architectures where this does not work reliably */
class PerformanceCounter {
    public:
        PerformanceCounter(int cacheType = 0, int cacheOp = 0, int cacheResult = 0) {
            (void) cacheType;
            (void) cacheOp;
            (void) cacheResult;
        }
        void start() { }
        void stop() { }
        long long getCount() const { return 0ull; }
};
#endif
