#ifndef FREEZER_H
#define FREEZER_H

#include "types.h"

// External variables
extern FrozenAddress frozen[MAX_FROZEN];
extern int frozen_count;
extern HANDLE freeze_thread;
extern int freeze_running;

// Freezer functions
DWORD WINAPI freeze_thread_func(LPVOID param);
void start_freeze_thread(void);
void stop_freeze_thread(void);

#endif // FREEZER_H
