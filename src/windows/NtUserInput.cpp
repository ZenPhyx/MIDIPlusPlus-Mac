#include "NtUserInput.h"
#include <cstring>
#include <stdexcept>

DWORD g_NtSyscallNumber = 0;

static UINT __fastcall FallbackSendInput(ULONG n, LPINPUT p, int sz) {
    return SendInput(n, p, sz);
}
UINT(__fastcall* NtUserSendInputCall)(ULONG, LPINPUT, int) = FallbackSendInput;

unsigned long GetNtUserSendInputSyscallNumber(void) {
    // Search win32u.dll first, then user32.dll, then ntdll.dll (same order as MIDI++)
    static const char* const DLLS[] = { "win32u.dll", "user32.dll", "ntdll.dll" };
    FARPROC pFunc = nullptr;
    for (auto dll : DLLS) {
        HMODULE mod = GetModuleHandleA(dll);
        if (!mod) continue;
        pFunc = GetProcAddress(mod, "NtUserSendInput");
        if (pFunc) break;
    }
    if (!pFunc)
        throw std::runtime_error("NtUserSendInput not found in win32u/user32/ntdll");

    // win32u.dll stub layout (standard on Windows 10/11):
    //   4C 8B D1        mov r10, rcx
    //   B8 xx xx xx xx  mov eax, <syscall_number>   ← number is at offset 4
    //   0F 05           syscall
    //   C3              ret
    const BYTE* p = reinterpret_cast<const BYTE*>(pFunc);
    if (p[0] != 0x4C || p[1] != 0x8B || p[2] != 0xD1)
        throw std::runtime_error("Unexpected NtUserSendInput stub — not a standard NT syscall stub");

    DWORD num;
    std::memcpy(&num, p + 4, sizeof(DWORD));
    return num;
}

void InitializeNtUserSendInputCall(void) {
    DWORD num;
    try { num = GetNtUserSendInputSyscallNumber(); }
    catch (...) { return; }  // stay on SendInput fallback

    g_NtSyscallNumber = num;

    // Build the same 11-byte trampoline as MIDI++:
    //   4C 8B D1        mov r10, rcx   (NT calling convention)
    //   B8 xx xx xx xx  mov eax, N
    //   0F 05           syscall
    //   C3              ret
    void* mem = VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return;

    BYTE* code = static_cast<BYTE*>(mem);
    code[0] = 0x4C; code[1] = 0x8B; code[2] = 0xD1;   // mov r10, rcx
    code[3] = 0xB8;                                      // mov eax, ...
    std::memcpy(&code[4], &num, sizeof(DWORD));          // ...syscall number
    code[8] = 0x0F; code[9] = 0x05;                     // syscall
    code[10] = 0xC3;                                     // ret

    DWORD old;
    VirtualProtect(mem, 16, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), mem, 16);

    NtUserSendInputCall = reinterpret_cast<UINT(__fastcall*)(ULONG, LPINPUT, int)>(mem);
}
