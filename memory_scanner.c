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

typedef struct {
    void* addr;
} Match;

Match matches[MAX_MATCHES];
int match_count = 0;
int scan_type = 0;
HANDLE process = NULL;
HWND hwndStatus, hwndMatches;

int compare_value(char* buffer, char* target, int type) {
    switch(type) {
        case 0: return *(int*)buffer == *(int*)target;
        case 1: return *(float*)buffer == *(float*)target;
        case 2: return *(double*)buffer == *(double*)target;
        case 3: return *(unsigned char*)buffer == *(unsigned char*)target;
    }
    return 0;
}

int get_type_size(int type) {
    switch(type) {
        case 0: return sizeof(int);
        case 1: return sizeof(float);
        case 2: return sizeof(double);
        case 3: return sizeof(unsigned char);
    }
    return 4;
}

void scan_memory(char* target) {
    MEMORY_BASIC_INFORMATION mbi;
    char* addr = 0;
    match_count = 0;
    int type_size = get_type_size(scan_type);
    
    while (VirtualQueryEx(process, addr, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && 
            (mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_EXECUTE_READWRITE)) {
            
            char* buffer = malloc(mbi.RegionSize);
            SIZE_T bytesRead;
            
            if (ReadProcessMemory(process, mbi.BaseAddress, buffer, mbi.RegionSize, &bytesRead)) {
                for (SIZE_T i = 0; i < bytesRead - type_size; i++) {
                    if (compare_value(buffer + i, target, scan_type) && match_count < MAX_MATCHES) {
                        matches[match_count].addr = (char*)mbi.BaseAddress + i;
                        match_count++;
                    }
                }
            }
            free(buffer);
        }
        addr = (char*)mbi.BaseAddress + mbi.RegionSize;
    }
}

void refine_scan(char* target) {
    int new_count = 0;
    int type_size = get_type_size(scan_type);
    
    for (int i = 0; i < match_count; i++) {
        char buffer[8];
        SIZE_T bytesRead;
        if (ReadProcessMemory(process, matches[i].addr, buffer, type_size, &bytesRead)) {
            if (compare_value(buffer, target, scan_type)) {
                matches[new_count++] = matches[i];
            }
        }
    }
    match_count = new_count;
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

void populate_process_list(HWND hwndList) {
    SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
    
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(snapshot, &pe)) {
        do {
            char text[512];
            sprintf(text, "%s (PID: %lu)", pe.szExeFile, pe.th32ProcessID);
            int index = SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)text);
            SendMessage(hwndList, LB_SETITEMDATA, index, pe.th32ProcessID);
        } while (Process32Next(snapshot, &pe));
    }
    
    CloseHandle(snapshot);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            InitCommonControls();
            
            CreateWindow("STATIC", "Select Process:", WS_VISIBLE | WS_CHILD, 10, 10, 120, 20, hwnd, NULL, NULL, NULL);
            HWND hwndProcessList = CreateWindow("LISTBOX", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY, 10, 35, 760, 150, hwnd, (HMENU)ID_PROCESS_LIST, NULL, NULL);
            populate_process_list(hwndProcessList);
            
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
            
            hwndStatus = CreateWindow("STATIC", "Status: Not attached", WS_VISIBLE | WS_CHILD | SS_SUNKEN, 0, 595, 780, 25, hwnd, (HMENU)ID_STATUS, NULL, NULL);
            
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
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
            }
            return 0;
        }
        
        case WM_DESTROY:
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
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 660,
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
