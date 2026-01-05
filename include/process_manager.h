#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "types.h"

// External variables
extern ProcessInfo processes[1000];
extern int process_count;

// Process management functions
void populate_process_list(HWND hwndList, const char* filter);
int compare_processes(const void* a, const void* b);

#endif // PROCESS_MANAGER_H
