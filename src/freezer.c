#include "../include/freezer.h"
#include "../include/scanner.h"
#include <stdio.h>

// Global variables
FrozenAddress frozen[MAX_FROZEN];
int frozen_count = 0;
HANDLE freeze_thread = NULL;
int freeze_running = 0;

/*
* Thread which iterates through all frozen addresses
*/
DWORD WINAPI freeze_thread_func(LPVOID param) {
    (void)param;  // Unused
    int type_size;

    while (freeze_running) {
        // Iterate through all frozen addresses (Currently set to max MAX_FROZEN)
        for (int i = 0; i < frozen_count; i++) {

            // Only freeze actively frozen values (boolean which can be set through ui)
            if (frozen[i].active) {
                type_size = get_type_size(frozen[i].type);

                // Overwrite value in memory with our frozen value
                WriteProcessMemory(process, frozen[i].addr, frozen[i].value, type_size, NULL);
            }
        }

        // Time between overwrites. Could be adjusted to increase/decrease frequency (Add parameter?)
        Sleep(100);
    }
    return 0;
}

void start_freeze_thread(void) {
    if (!freeze_running) {
        freeze_running = 1;
        freeze_thread = CreateThread(NULL, 0, freeze_thread_func, NULL, 0, NULL);
    }
}

void stop_freeze_thread(void) {
    freeze_running = 0;
    if (freeze_thread) {
        WaitForSingleObject(freeze_thread, 1000);
        CloseHandle(freeze_thread);
        freeze_thread = NULL;
    }
}
