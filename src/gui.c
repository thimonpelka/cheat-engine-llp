#include "../include/gui.h"
#include "../include/scanner.h"
#include "../include/freezer.h"
#include "../include/process_manager.h"
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "comctl32.lib")

// Global window handles
HWND hwndStatus, hwndMatches, hwndFrozen;

// Updates frozen list in gui
void update_frozen_list(void) {
    // Reset list of frozen items in gui
    SendMessage(hwndFrozen, LB_RESETCONTENT, 0, 0);
    
    for (int i = 0; i < frozen_count; i++) {
        char text[256];
        
        const char* status = frozen[i].active ? "[FROZEN]" : "[PAUSED]";
        
        switch(frozen[i].type) {
            case SCAN_TYPE_INT: sprintf(text, "%s 0x%p = %d", status, frozen[i].addr, *(int*)frozen[i].value); break;
            case SCAN_TYPE_FLOAT: sprintf(text, "%s 0x%p = %f", status, frozen[i].addr, *(float*)frozen[i].value); break;
            case SCAN_TYPE_DOUBLE: sprintf(text, "%s 0x%p = %lf", status, frozen[i].addr, *(double*)frozen[i].value); break;
            case SCAN_TYPE_BYTE: sprintf(text, "%s 0x%p = %u", status, frozen[i].addr, *(unsigned char*)frozen[i].value); break;
        }

        // Add new text to list of frozen parameters
        SendMessage(hwndFrozen, LB_ADDSTRING, 0, (LPARAM)text);
    }
}

void update_matches_list(void) {
    ListView_DeleteAllItems(hwndMatches);

    int show = match_count < 100 ? match_count : 100;
    int type_size = get_type_size(scan_type);

    for (int i = 0; i < show; i++) {
        char valueText[64] = "";
        char addrText[32];
        char ptrText[8];
        char pointsToText[32] = "";

        char buffer[8];
        SIZE_T bytesRead;

        ReadProcessMemory(process, matches[i].addr, buffer, type_size, &bytesRead);

        sprintf(addrText, "0x%p", matches[i].addr);
        sprintf(ptrText, matches[i].is_pointer ? "YES" : "NO");

        if (matches[i].is_pointer) {
            sprintf(pointsToText, "0x%p", matches[i].points_to);
        }

        switch (scan_type) {
            case SCAN_TYPE_INT: sprintf(valueText, "%d", *(int*)buffer); break;
            case SCAN_TYPE_FLOAT: sprintf(valueText, "%f", *(float*)buffer); break;
            case SCAN_TYPE_DOUBLE: sprintf(valueText, "%lf", *(double*)buffer); break;
            case SCAN_TYPE_BYTE: sprintf(valueText, "%u", *(unsigned char*)buffer); break;
        }

        LVITEM item = {0};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = i;
        item.pszText = "";
        item.lParam = i;

        ListView_InsertItem(hwndMatches, &item);

        char indexText[16];
        sprintf(indexText, "%d", i);

        ListView_SetItemText(hwndMatches, i, 0, indexText);
        ListView_SetItemText(hwndMatches, i, 1, addrText);
        ListView_SetItemText(hwndMatches, i, 2, valueText);
        ListView_SetItemText(hwndMatches, i, 3, ptrText);
        ListView_SetItemText(hwndMatches, i, 4, pointsToText);
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
            hwndMatches = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL, 10, 340, 760, 200, hwnd, (HMENU)ID_MATCHES_LIST, NULL, NULL);
            ListView_SetExtendedListViewStyle(
                hwndMatches,
                LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES
            );
            
            LVCOLUMN col = {0};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            col.pszText = "Index";      col.cx = 60;  ListView_InsertColumn(hwndMatches, 0, &col);
            col.pszText = "Address";    col.cx = 140; ListView_InsertColumn(hwndMatches, 1, &col);
            col.pszText = "Value";      col.cx = 120; ListView_InsertColumn(hwndMatches, 2, &col);
            col.pszText = "Pointer?";   col.cx = 80;  ListView_InsertColumn(hwndMatches, 3, &col);
            col.pszText = "Points To";  col.cx = 140; ListView_InsertColumn(hwndMatches, 4, &col);
            
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
                        
                        start_freeze_thread();
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
                        case SCAN_TYPE_INT: { int v = atoi(valueText); memcpy(target, &v, sizeof(int)); break; }
                        case SCAN_TYPE_FLOAT: { float v = atof(valueText); memcpy(target, &v, sizeof(float)); break; }
                        case SCAN_TYPE_DOUBLE: { double v = atof(valueText); memcpy(target, &v, sizeof(double)); break; }
                        case SCAN_TYPE_BYTE: { unsigned char v = atoi(valueText); target[0] = v; break; }
                    }
                    
                    SetWindowText(hwndStatus, "Status: Scanning...");
                    scan_memory(target);
                    classify_matches();
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
                        case SCAN_TYPE_INT: { int v = atoi(valueText); memcpy(target, &v, sizeof(int)); break; }
                        case SCAN_TYPE_FLOAT: { float v = atof(valueText); memcpy(target, &v, sizeof(float)); break; }
                        case SCAN_TYPE_DOUBLE: { double v = atof(valueText); memcpy(target, &v, sizeof(double)); break; }
                        case SCAN_TYPE_BYTE: { unsigned char v = atoi(valueText); target[0] = v; break; }
                    }
                    
                    SetWindowText(hwndStatus, "Status: Refining...");
                    refine_scan(target);
                    classify_matches();
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
                    
                    int index = ListView_GetNextItem(hwndMatches, -1, LVNI_SELECTED);
                    if (index == LB_ERR || index >= match_count) {
                        MessageBox(hwnd, "Please select a match to modify", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    char valueText[256];
                    GetDlgItemText(hwnd, ID_NEWVALUE_EDIT, valueText, 256);
                    
                    char value[8];
                    int type_size = get_type_size(scan_type);
                    switch(scan_type) {
                        case SCAN_TYPE_INT: { int v = atoi(valueText); memcpy(value, &v, sizeof(int)); break; }
                        case SCAN_TYPE_FLOAT: { float v = atof(valueText); memcpy(value, &v, sizeof(float)); break; }
                        case SCAN_TYPE_DOUBLE: { double v = atof(valueText); memcpy(value, &v, sizeof(double)); break; }
                        case SCAN_TYPE_BYTE: { unsigned char v = atoi(valueText); value[0] = v; break; }
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
                    
                    int index = ListView_GetNextItem(hwndMatches, -1, LVNI_SELECTED);
                    if (index == LB_ERR || index >= match_count) {
                        MessageBox(hwnd, "Please select a match to freeze", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    if (frozen_count >= MAX_FROZEN) {
                        MessageBox(hwnd, "Maximum frozen addresses reached", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    
                    char valueText[256];
                    GetDlgItemText(hwnd, ID_NEWVALUE_EDIT, valueText, 256);
                    
                    frozen[frozen_count].addr = matches[index].addr;
                    frozen[frozen_count].type = scan_type;
                    frozen[frozen_count].active = 1;
                    
                    switch(scan_type) {
                        case SCAN_TYPE_INT: { int v = atoi(valueText); memcpy(frozen[frozen_count].value, &v, sizeof(int)); break; }
                        case SCAN_TYPE_FLOAT: { float v = atof(valueText); memcpy(frozen[frozen_count].value, &v, sizeof(float)); break; }
                        case SCAN_TYPE_DOUBLE: { double v = atof(valueText); memcpy(frozen[frozen_count].value, &v, sizeof(double)); break; }
                        case SCAN_TYPE_BYTE: { unsigned char v = atoi(valueText); frozen[frozen_count].value[0] = v; break; }
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
            stop_freeze_thread();
            if (process) CloseHandle(process);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
