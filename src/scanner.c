#include "../include/scanner.h"
#include <stdio.h>
#include <string.h>

// Global variables
Match matches[MAX_MATCHES];
int match_count = 0;
int scan_type = 0;
HANDLE process = NULL;

/*
 * Detects whether the given target process is 64-bit
 */
int is_x64(void) {
    BOOL wow64;
    IsWow64Process(process, &wow64);
    return !wow64; // crude but works for scanner (would not detect native 32-bit OS, but thats fine here)
}

/*
 * Returns the size of a pointer in the target process (depends on whether the target process is 64 or 32 bit)
 */
int ptr_size(void) {
    return is_x64() ? 8 : 4;
}

/*
 * Get size of currently selected datatype (in bytes)
 */
int get_type_size(int type) {
    switch(type) {
        case SCAN_TYPE_INT: return sizeof(int);
        case SCAN_TYPE_FLOAT: return sizeof(float);
        case SCAN_TYPE_DOUBLE: return sizeof(double);
        case SCAN_TYPE_BYTE: return sizeof(unsigned char);
    }
    return 4;
}

/*
 * Compare two values of a given type (dynamically)
 */
int compare_value(char* buffer, char* target, int type) {
    switch(type) {
        case SCAN_TYPE_INT: return *(int*)buffer == *(int*)target;
        case SCAN_TYPE_FLOAT: return *(float*)buffer == *(float*)target;
        case SCAN_TYPE_DOUBLE: return *(double*)buffer == *(double*)target;
        case SCAN_TYPE_BYTE: return *(unsigned char*)buffer == *(unsigned char*)target;
    }
    return 0;
}


/*
 * Checks if the given pointer (addr) points to one of our known matches
 * Used for pointer scanning / pointer path discovery
 */
int is_pointer_to_match(void* addr, void** out_target) {
    SIZE_T read;
    void* ptr = NULL;

    // Try to read memory
    if (!ReadProcessMemory(process, addr, &ptr, ptr_size(), &read))
        return 0;

    // Check if size of read memory equals expected size (size of pointer)
    if (read != (SIZE_T)ptr_size())
        return 0;

    // Iterate through all matches and compare if their address equals our pointer
    for (int i = 0; i < match_count; i++) {
        if (matches[i].addr == ptr) {
            // Store ptr into out_target (if set)
            if (out_target)
                *out_target = ptr;

            // Return 1 = is pointer
            return 1;
        }
    }
    // Return 0 = is no pointer
    return 0;
}

/*
 * Iterates through all matches and classifies them as pointer or non-pointers
 */
void classify_matches(void) {
    // Check if current type size could even be a pointer (if scanned data size does not equal pointer size then -> cannot be a pointer)
    if (get_type_size(scan_type) != ptr_size()) {
        // if not pointer sized -> mark everything as non-pointer
        for (int i = 0; i < match_count; i++) {
            matches[i].is_pointer = 0;
            matches[i].points_to = NULL;
        }
        return;
    }

    // Else -> test each match for whether its a pointer or not
    for (int i = 0; i < match_count; i++) {
        matches[i].is_pointer =
            is_pointer_to_match(matches[i].addr, &matches[i].points_to);
    }
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
                // go through all read bytes until the last valid combination
                // (e.g. 100 bytes, int size is 4 bytes => last valid start at 96 bytes)
                for (SIZE_T i = 0; i + type_size <= bytesRead; i++) {
                    // Compare value of buffer to target; if match => store and increase counter
                    if (compare_value(buffer + i, target, scan_type) && match_count < MAX_MATCHES) {
                        matches[match_count].addr = (char*)mbi.BaseAddress + i;
                        matches[match_count].is_pointer = 0;
                        matches[match_count].points_to = NULL;
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
                // Old memory will stay the same but since we adjusted the match_count this
                // will lead to no problem
                matches[new_count++] = matches[i];
            }
        }
    }
    match_count = new_count;
}
