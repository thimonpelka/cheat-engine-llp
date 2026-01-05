#ifndef SCANNER_H
#define SCANNER_H

#include "types.h"

// External variables
extern Match matches[MAX_MATCHES];
extern int match_count;
extern int scan_type;
extern HANDLE process;

// Architecture detection
int is_x64(void);
int ptr_size(void);

// Type utilities
int get_type_size(int type);
int compare_value(char* buffer, char* target, int type);

// Pointer detection
int is_pointer_to_match(void* addr, void** out_target);
void classify_matches(void);

// Scanning functions
void scan_memory(char* target);
void refine_scan(char* target);

#endif // SCANNER_H
