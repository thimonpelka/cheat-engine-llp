#ifndef TYPES_H
#define TYPES_H

#include <windows.h>

// Maximum number of matches to store
#define MAX_MATCHES 10000

// Maximum number of frozen addresses
#define MAX_FROZEN 100

// Control IDs for GUI elements
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

// Scan type enumeration
typedef enum {
    SCAN_TYPE_INT = 0,
    SCAN_TYPE_FLOAT = 1,
    SCAN_TYPE_DOUBLE = 2,
    SCAN_TYPE_BYTE = 3
} ScanType;

// Structure representing a matched memory address
typedef struct {
    void* addr;
    int is_pointer;
    void* points_to;
} Match;

// Structure representing a frozen memory address
typedef struct {
    void* addr;
    char value[8];
    int type;
    int active;
} FrozenAddress;

// Structure for process information
typedef struct {
    char name[256];
    DWORD pid;
} ProcessInfo;

#endif // TYPES_H
