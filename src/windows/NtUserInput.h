#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Syscall number discovered at runtime from the NT stub in win32u.dll / user32.dll
extern DWORD g_NtSyscallNumber;

// Direct-syscall function pointer — bypasses SendInput API hooks (same technique as MIDI++)
// Falls back to SendInput until InitializeNtUserSendInputCall() is called.
extern UINT(__fastcall* NtUserSendInputCall)(ULONG cInputs, LPINPUT pInputs, int cbSize);

// Inspect the NT stub to extract the syscall number.
// Throws std::runtime_error if the stub signature is unexpected.
unsigned long GetNtUserSendInputSyscallNumber(void);

// Allocate an RX trampoline that executes "syscall" with the discovered number.
void InitializeNtUserSendInputCall(void);

#ifdef __cplusplus
}
#endif
