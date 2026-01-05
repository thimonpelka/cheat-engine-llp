#ifndef GUI_H
#define GUI_H

#include "types.h"

// GUI window handles
extern HWND hwndStatus, hwndMatches, hwndFrozen;

// GUI functions
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void update_matches_list(void);
void update_frozen_list(void);

#endif // GUI_H
