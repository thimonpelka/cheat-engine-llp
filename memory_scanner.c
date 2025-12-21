#include <windows.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")

#define MAX_MATCHES 10000
#define ID_PROCESS_LIST 101
#define ID_ATTACH_BTN 102
#define ID_TYPE_COMBO 103
#define ID_VALUE_EDIT 104
#define ID_SCAN_BTN 105
#define ID_REFINE_BTN 106
#define ID_MATCHES_LIST 107
#define ID_NEWVALUE_EDIT 108
#define ID_MODIFY_BTN 109
#define ID_STATUS 110
#define ID_FREEZE_BTN 111
#define ID_FROZEN_LIST 112
#define ID_SEARCH_EDIT 113

typedef struct {
    void* addr;
} Match;

typedef struct {
    void* addr;
    char value[8];
    int type;
    int active;
} FrozenAddress;

FrozenAddress frozen[100];
int frozen_count = 0;
HANDLE freeze_thread = NULL;
int freeze_running = 0;

Match matches[MAX_MATCHES];
int match_count = 0;
int scan_type = 0;
HANDLE process = NULL;
HWND hwndStatus, hwndMatches, hwndFrozen;

// Compare value of buffer to target value by casting values to expected types
int compare_value(char* buffer, char* target, int type) {
    switch(type) {
        case 0: return *(int*)buffer == *(int*)target;
        case 1: return *(float*)buffer == *(float*)target;
        case 2: return *(double*)buffer == *(double*)target;
        case 3: return *(unsigned char*)buffer == *(unsigned char*)target;
    }
    return 0;
}

// Get size of data type according to selected type
int get_type_size(int type) {
    switch(type) {
        case 0: return sizeof(int);
        case 1: return sizeof(float);
        case 2: return sizeof(double);
        case 3: return sizeof(unsigned char);
    }
    return 4;
}

/*
* Does the initial memory scan. Scans the entire memory of the process for a given target value.
* Stores all values it finds (until it reaches MAX_MATCHES)
*/
void scan_memory(char* target) {
    MEMORY_BASIC_INFORMATION mbi;
    char* addr = 0;
    match_count = 0;
    int type_size = get_type_size(scan_type);
    
    // VirtualQueryEx retrieves information about a range of pages within the virtual address space of a specified process.
    while (VirtualQueryEx(process, addr, &mbi, sizeof(mbi))) {
        // MEM_COMMIT = commited pages for which physical storage has been allocated (MEM_FREE, MEM_RESERVE are other possibilities)
        if (mbi.State == MEM_COMMIT && 
            // All pages with read/write access (or execute read/write access, this is apparently legacy tho)
            (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE)) {
            
            // Allocate buffer
            char* buffer = malloc(mbi.RegionSize);
            SIZE_T bytesRead;
            
            // Read region of memory and store in buffer
            if (ReadProcessMemory(process, mbi.BaseAddress, buffer, mbi.RegionSize, &bytesRead)) {
                // go through all read bytes until the last valid combination (e.g. 100 bytes, int size is 4 bytes => last valid start at 96 bytes)
                for (SIZE_T i = 0; i < bytesRead - type_size; i++) {
                    // Compare value of buffer to target; if match => store and increase counter
                    if (compare_value(buffer + i, target, scan_type) && match_count < MAX_MATCHES) {
                        matches[match_count].addr = (char*)mbi.BaseAddress + i;
                        match_count++;
                    }
                }
            }
            free(buffer);
        }

        // Get next address (region in memory) by adding regionsize to current base address
        addr = (char*)mbi.BaseAddress + mbi.RegionSize;
    }
}

/*
* Re-scans all stored matches and filters for a new given target
*/
void refine_scan(char* target) {
    int new_count = 0;
    int type_size = get_type_size(scan_type);
    
    // Iterate through all matches
    for (int i = 0; i < match_count; i++) {
        char buffer[8];
        SIZE_T bytesRead;

        // Again read memory of stored match
        if (ReadProcessMemory(process, matches[i].addr, buffer, type_size, &bytesRead)) {
            // Compare if the value now reflects the new target
            if (compare_value(buffer, target, scan_type)) {
                // Overwrite old matches with new index (again starting at 0). 
                // Old memory will stay the same but since we adjusted the match_count this will lead to no problem
                matches[new_count++] = matches[i];
            }
        }
    }
    match_count = new_count;
}

/*
* Thread which iterates through all frozen addresses
*/
DWORD WINAPI freeze_thread_func(LPVOID param) {
    int type_size;

    while (freeze_running) {
        // Iterate through all frozen addresses (Currently set to max 100)
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

// Updates frozen list in gui
void update_frozen_list() {
    // Reset list of frozen items in gui
    SendMessage(hwndFrozen, LB_RESETCONTENT, 0, 0);
    int type_size;
    
    for (int i = 0; i < frozen_count; i++) {
        char text[256];
        type_size = get_type_size(frozen[i].type);
        
        const char* status = frozen[i].active ? "[FROZEN]" : "[PAUSED]";
        
        switch(frozen[i].type) {
            case 0: sprintf(text, "%s 0x%p = %d", status, frozen[i].addr, *(int*)frozen[i].value); break;
            case 1: sprintf(text, "%s 0x%p = %f", status, frozen[i].addr, *(float*)frozen[i].value); break;
            case 2: sprintf(text, "%s 0x%p = %lf", status, frozen[i].addr, *(double*)frozen[i].value); break;
            case 3: sprintf(text, "%s 0x%p = %u", status, frozen[i].addr, *(unsigned char*)frozen[i].value); break;
        }

        // Add new text to list of frozen parameters
        SendMessage(hwndFrozen, LB_ADDSTRING, 0, (LPARAM)text);
    }
}

void update_matches_list() {
    SendMessage(hwndMatches, LB_RESETCONTENT, 0, 0);
    int type_size = get_type_size(scan_type);
    int show = match_count < 100 ? match_count : 100;
    
    for (int i = 0; i < show; i++) {
        char buffer[8];
        char text[256];
        SIZE_T bytesRead;
        ReadProcessMemory(process, matches[i].addr, buffer, type_size, &bytesRead);
        
        switch(scan_type) {
            case 0: sprintf(text, "[%d] 0x%p = %d", i, matches[i].addr, *(int*)buffer); break;
            case 1: sprintf(text, "[%d] 0x%p = %f", i, matches[i].addr, *(float*)buffer); break;
            case 2: sprintf(text, "[%d] 0x%p = %lf", i, matches[i].addr, *(double*)buffer); break;
            case 3: sprintf(text, "[%d] 0x%p = %u", i, matches[i].addr, *(unsigned char*)buffer); break;
        }
        SendMessage(hwndMatches, LB_ADDSTRING, 0, (LPARAM)text);
    }
    
    if (match_count > 100) {
        char text[256];
        sprintf(text, "... and %d more matches", match_count - 100);
        SendMessage(hwndMatches, LB_ADDSTRING, 0, (LPARAM)text);
    }
}

typedef struct {
    char name[256];
    DWORD pid;
} ProcessInfo;

ProcessInfo processes[1000];
int process_count = 0;

int compare_processes(const void* a, const void* b) {
    return _stricmp(((ProcessInfo*)a)->name, ((ProcessInfo*)b)->name);
}

void populate_process_list(HWND hwndList, const char* filter) {
    SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    
    process_count = 0;
    if (Process32First(snapshot, &pe)) {
        do {
            strcpy(processes[process_count].name, pe.szExeFile);
            processes[process_count].pid = pe.th32ProcessID;
            process_count++;
        } while (Process32Next(snapshot, &pe) && process_count < 1000);
    }
    
    CloseHandle(snapshot);
    
    qsort(processes, process_count, sizeof(ProcessInfo), compare_processes);
    
    for (int i = 0; i < process_count; i++) {
        if (filter && filter[0] != '\0') {
            if (strstr(_strlwr(processes[i].name), _strlwr((char*)filter)) == NULL) {
                continue;
            }
        }
        
        char text[512];
        sprintf(text, "%s (PID: %lu)", processes[i].name, processes[i].pid);
        int index = SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)text);
        SendMessage(hwndList, LB_SETITEMDATA, index, processes[i].pid);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            InitCommonControls();
            
            CreateWindow("STATIC", "Select Process:", WS_VISIBLE | WS_CHILD, 10, 10, 120, 20, hwnd, NULL, NULL, NULL);
            CreateWindow("STATIC", "Search:", WS_VISIBLE | WS_CHILD, 140, 10, 50, 20, hwnd, NULL, NULL, NULL);
            CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 195, 7, 200, 23, hwnd, (HMENU)ID_SEARCH_EDIT, NULL, NULL);
            
            HWND hwndProcessList = CreateWindow("LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 10, 35, 760, 150, hwnd, (HMENU)ID_PROCESS_LIST, NULL, NULL);
            populate_process_list(hwndProcessList, NULL);
            
            CreateWindow("BUTTON", "Attach to Process", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 10, 195, 150, 30, hwnd, (HMENU)ID_ATTACH_BTN, NULL, NULL);
            
            CreateWindow("STATIC", "Scan Type:", WS_VISIBLE | WS_CHILD, 10, 240, 80, 20, hwnd, NULL, NULL, NULL);
            HWND hwndCombo = CreateWindow("COMBOBOX", NULL, WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST, 100, 237, 120, 200, hwnd, (HMENU)ID_TYPE_COMBO, NULL, NULL);
            SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)"Integer (int)");
            SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)"Float");
            SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)"Double");
            SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)"Byte");
            SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
            
            CreateWindow("STATIC", "Value to Search:", WS_VISIBLE | WS_CHILD, 10, 275, 120, 20, hwnd, NULL, NULL, NULL);
            CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 140, 272, 150, 25, hwnd, (HMENU)ID_VALUE_EDIT, NULL, NULL);
            
            CreateWindow("BUTTON", "Initial Scan", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 300, 270, 100, 30, hwnd, (HMENU)ID_SCAN_BTN, NULL, NULL);
            CreateWindow("BUTTON", "Refine Scan", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 410, 270, 100, 30, hwnd, (HMENU)ID_REFINE_BTN, NULL, NULL);
            
            CreateWindow("STATIC", "Matches:", WS_VISIBLE | WS_CHILD, 10, 315, 80, 20, hwnd, NULL, NULL, NULL);
            hwndMatches = CreateWindow("LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 10, 340, 760, 200, hwnd, (HMENU)ID_MATCHES_LIST, NULL, NULL);
            
            CreateWindow("STATIC", "New Value:", WS_VISIBLE | WS_CHILD, 10, 555, 80, 20, hwnd, NULL, NULL, NULL);
            CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 100, 552, 150, 25, hwnd, (HMENU)ID_NEWVALUE_EDIT, NULL, NULL);
            CreateWindow("BUTTON", "Modify Selected", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 260, 550, 130, 30, hwnd, (HMENU)ID_MODIFY_BTN, NULL, NULL);
            CreateWindow("BUTTON", "Freeze Value", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 400, 550, 110, 30, hwnd, (HMENU)ID_FREEZE_BTN, NULL, NULL);
            
            CreateWindow("STATIC", "Frozen Addresses:", WS_VISIBLE | WS_CHILD, 10, 595, 120, 20, hwnd, NULL, NULL, NULL);
            hwndFrozen = CreateWindow("LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 10, 620, 760, 100, hwnd, (HMENU)ID_FROZEN_LIST, NULL, NULL);
            
            hwndStatus = CreateWindow("STATIC", "Status: Not attached", WS_VISIBLE | WS_CHILD | SS_SUNKEN, 0, 730, 780, 25, hwnd, (HMENU)ID_STATUS, NULL, NULL);
            
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_SEARCH_EDIT: {
                    if (HIWORD(wParam) == EN_CHANGE) {
                        char search[256];
                        GetDlgItemText(hwnd, ID_SEARCH_EDIT, search, 256);
                        HWND hwndList = GetDlgItem(hwnd, ID_PROCESS_LIST);
                        populate_process_list(hwndList, search);
                    }
                    break;
                }
                
                case ID_ATTACH_BTN: {
                    HWND hwndList = GetDlgItem(hwnd, ID_PROCESS_LIST);
                    int index = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (index == LB_ERR) {
                        MessageBox(hwnd, "Please select a process", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    DWORD pid = SendMessage(hwndList, LB_GETITEMDATA, index, 0);
                    if (process) CloseHandle(process);
                    process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
                    
                    if (process) {
                        char status[256];
                        sprintf(status, "Status: Attached to PID %lu", pid);
                        SetWindowText(hwndStatus, status);
                        
                        if (!freeze_running) {
                            freeze_running = 1;
                            freeze_thread = CreateThread(NULL, 0, freeze_thread_func, NULL, 0, NULL);
                        }
                    } else {
                        SetWindowText(hwndStatus, "Status: Failed to attach (try running as Administrator)");
                    }
                    break;
                }
                
                case ID_TYPE_COMBO: {
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        scan_type = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                    }
                    break;
                }
                
                case ID_SCAN_BTN: {
                    if (!process) {
                        MessageBox(hwnd, "Please attach to a process first", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    char valueText[256];
                    GetDlgItemText(hwnd, ID_VALUE_EDIT, valueText, 256);
                    
                    char target[8];
                    switch(scan_type) {
                        case 0: { int v = atoi(valueText); memcpy(target, &v, sizeof(int)); break; }
                        case 1: { float v = atof(valueText); memcpy(target, &v, sizeof(float)); break; }
                        case 2: { double v = atof(valueText); memcpy(target, &v, sizeof(double)); break; }
                        case 3: { unsigned char v = atoi(valueText); target[0] = v; break; }
                    }
                    
                    SetWindowText(hwndStatus, "Status: Scanning...");
                    scan_memory(target);
                    update_matches_list();
                    
                    char status[256];
                    sprintf(status, "Status: Found %d matches", match_count);
                    SetWindowText(hwndStatus, status);
                    break;
                }
                
                case ID_REFINE_BTN: {
                    if (!process || match_count == 0) {
                        MessageBox(hwnd, "Please do an initial scan first", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    char valueText[256];
                    GetDlgItemText(hwnd, ID_VALUE_EDIT, valueText, 256);
                    
                    char target[8];
                    switch(scan_type) {
                        case 0: { int v = atoi(valueText); memcpy(target, &v, sizeof(int)); break; }
                        case 1: { float v = atof(valueText); memcpy(target, &v, sizeof(float)); break; }
                        case 2: { double v = atof(valueText); memcpy(target, &v, sizeof(double)); break; }
                        case 3: { unsigned char v = atoi(valueText); target[0] = v; break; }
                    }
                    
                    SetWindowText(hwndStatus, "Status: Refining...");
                    refine_scan(target);
                    update_matches_list();
                    
                    char status[256];
                    sprintf(status, "Status: Refined to %d matches", match_count);
                    SetWindowText(hwndStatus, status);
                    break;
                }
                
                case ID_MODIFY_BTN: {
                    if (!process) {
                        MessageBox(hwnd, "Please attach to a process first", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    int index = SendMessage(hwndMatches, LB_GETCURSEL, 0, 0);
                    if (index == LB_ERR || index >= match_count) {
                        MessageBox(hwnd, "Please select a match to modify", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    char valueText[256];
                    GetDlgItemText(hwnd, ID_NEWVALUE_EDIT, valueText, 256);
                    
                    char value[8];
                    int type_size = get_type_size(scan_type);
                    switch(scan_type) {
                        case 0: { int v = atoi(valueText); memcpy(value, &v, sizeof(int)); break; }
                        case 1: { float v = atof(valueText); memcpy(value, &v, sizeof(float)); break; }
                        case 2: { double v = atof(valueText); memcpy(value, &v, sizeof(double)); break; }
                        case 3: { unsigned char v = atoi(valueText); value[0] = v; break; }
                    }
                    
                    SIZE_T written;
                    if (WriteProcessMemory(process, matches[index].addr, value, type_size, &written)) {
                        SetWindowText(hwndStatus, "Status: Value modified successfully");
                        update_matches_list();
                    } else {
                        SetWindowText(hwndStatus, "Status: Failed to write memory");
                    }
                    break;
                }
                
                case ID_FREEZE_BTN: {
                    if (!process) {
                        MessageBox(hwnd, "Please attach to a process first", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    int index = SendMessage(hwndMatches, LB_GETCURSEL, 0, 0);
                    if (index == LB_ERR || index >= match_count) {
                        MessageBox(hwnd, "Please select a match to freeze", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    if (frozen_count >= 100) {
                        MessageBox(hwnd, "Maximum frozen addresses reached", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    char valueText[256];
                    GetDlgItemText(hwnd, ID_NEWVALUE_EDIT, valueText, 256);
                    
                    frozen[frozen_count].addr = matches[index].addr;
                    frozen[frozen_count].type = scan_type;
                    frozen[frozen_count].active = 1;
                    
                    switch(scan_type) {
                        case 0: { int v = atoi(valueText); memcpy(frozen[frozen_count].value, &v, sizeof(int)); break; }
                        case 1: { float v = atof(valueText); memcpy(frozen[frozen_count].value, &v, sizeof(float)); break; }
                        case 2: { double v = atof(valueText); memcpy(frozen[frozen_count].value, &v, sizeof(double)); break; }
                        case 3: { unsigned char v = atoi(valueText); frozen[frozen_count].value[0] = v; break; }
                    }
                    
                    frozen_count++;
                    update_frozen_list();
                    SetWindowText(hwndStatus, "Status: Address frozen");
                    break;
                }
                
                case ID_FROZEN_LIST: {
                    if (HIWORD(wParam) == LBN_DBLCLK) {
                        int index = SendMessage(hwndFrozen, LB_GETCURSEL, 0, 0);
                        if (index != LB_ERR && index < frozen_count) {
                            frozen[index].active = !frozen[index].active;
                            update_frozen_list();
                        }
                    }
                    break;
                }
            }
            return 0;
        }
        
        case WM_DESTROY:
            freeze_running = 0;
            if (freeze_thread) {
                WaitForSingleObject(freeze_thread, 1000);
                CloseHandle(freeze_thread);
            }
            if (process) CloseHandle(process);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "MemoryScannerClass";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClass(&wc);
    
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Memory Scanner", 
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 800,
        NULL, NULL, hInstance, NULL);
    
    if (hwnd == NULL) return 0;
    
    ShowWindow(hwnd, nCmdShow);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
