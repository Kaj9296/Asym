#pragma once

#define CONFIG_PMM_LAZY true
#define CONFIG_TIME_SLICE (NANOSECONDS_PER_SECOND / 2)
#define CONFIG_SCHED_HZ 1024
#define CONFIG_KERNEL_STACK (PAGE_SIZE)
#define CONFIG_USER_STACK (PAGE_SIZE * 4)
#define CONFIG_MAX_PATH 256
#define CONFIG_MAX_NAME 32
#define CONFIG_FILE_AMOUNT 64
