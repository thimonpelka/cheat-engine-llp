#include "../include/process_manager.h"
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Global variables
ProcessInfo processes[1000];
int process_count = 0;

/*
* Compares two processes by name. Returns less than 0 if a < b, 0 if a = b, and more than 0 if a > b
* Used for quick-sort algorithm for ordering
*/
int compare_processes(const void* a, const void* b) {
    return _stricmp(((ProcessInfo*)a)->name, ((ProcessInfo*)b)->name);
}

/*
* (Re-)poppulates the list of processes in the gui.
*/
void populate_process_list(HWND hwndList, const char* filter) {
    SendMessage(hwndList, LB_RESETCONTENT, 0, 0);

    // Get snapshot of current system processes
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    process_count = 0;
    if (Process32First(snapshot, &pe)) { // Retrieve first process in system snapshots
        do {
            strcpy(processes[process_count].name, pe.szExeFile);
            processes[process_count].pid = pe.th32ProcessID;
            process_count++;
        } while (Process32Next(snapshot, &pe) && process_count < 1000); // Retrieve the next process in snapshot
                                                                        // (until limit of 1000)
    }

    CloseHandle(snapshot);

    // (Quick-)Sort all retrived processes by name (using compare_processes func for evaluation)
    qsort(processes, process_count, sizeof(ProcessInfo), compare_processes);

    // Iterate through all processes and populate gui
    for (int i = 0; i < process_count; i++) {

        // Filter name of process IF filter is set
        if (filter && filter[0] != '\0') {
            char lower_name[256];
            char lower_filter[256];
            strcpy(lower_name, processes[i].name);
            strcpy(lower_filter, filter);
            _strlwr(lower_name);
            _strlwr(lower_filter);

            // Check if name fits in filter
            if (strstr(lower_name, lower_filter) == NULL) {
                continue;
            }
        }

        // Set text of list entry for current process
        char text[512];
        sprintf(text, "%s (PID: %lu)", processes[i].name, processes[i].pid);
        int index = SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)text);
        SendMessage(hwndList, LB_SETITEMDATA, index, processes[i].pid);
    }
}
