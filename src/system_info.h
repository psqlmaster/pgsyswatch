// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Alexander Scheglov
#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include "postgres.h"
#include "fmgr.h"

typedef struct SystemSwapInfo {
    float total_swap; 
    float used_swap; 
    float free_swap; 
} SystemSwapInfo;

typedef struct CpuFrequencyInfo {
    int core_id; 
    float frequency_mhz;
} CpuFrequencyInfo;

// Функции
SystemSwapInfo get_system_swap_info();
int get_cpu_cores();
CpuFrequencyInfo* get_cpu_frequencies(int *num_cores);

#endif
