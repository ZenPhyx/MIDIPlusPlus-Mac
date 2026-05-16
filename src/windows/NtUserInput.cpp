#include "NtUserInput.h"
#include <cstring>
#include <stdexcept>

DWORD g_NtSyscallNumber = 0;

static UINT __fastcall FallbackSendInput(ULONG n, LPINPUT p, int sz) {
    return SendInput(n, p, sz);
}
UINT(__fastcall* NtUserSendInputCall)(ULONG, LPINPUT, int) = FallbackSendInput;

unsigned long GetNtUserSendInputSyscallNumber(void) {
    HMODULE mod = GetModuleHandleW(L"win32u.dll");
    if (!mod) mod = LoadLibraryW(L"win32u.dll");
    if (!mod) throw std::runtime_error("win32u.dll not available");

    FARPROC fn = GetProcAddress(mod, "NtUserSendInput");
    if (!fn) throw std::runtime_error("NtUserSendInput not exported");

    // Stub starts with: B8 xx xx xx xx  (mov eax, syscall_number)
    const uint8_t* stub = reinterpret_cast<const uint8_t*>(fn);
    if (stub[0] != 0xB8)
        throw std::runtime_error("Unexpected stub — syscall extraction aborted");

    DWORD num;
    std::memcpy(&num, stub + 1, sizeof(DWORD));
    return num;
}

void InitializeNtUserSendInputCall(void) {
    DWORD num;
    try { num = GetNtUserSendInputSyscallNumber(); }
    catch (...) { return; }  // stay on SendInput fallback

    g_NtSyscallNumber = num;

    // Trampoline: mov r10,rcx | mov eax,N | syscall | ret
    // r10=rcx is required by the NT calling convention on x64.
    static const size_t SZ = 12;
    static uint8_t tramp[SZ] = {
        0x4C, 0x8B, 0xD1,               // mov r10, rcx
        0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, <num>
        0x0F, 0x05,                      // syscall
        0xC3,                            // ret
        0x90,                            // nop (alignment)
    };
    std::memcpy(&tramp[4], &num, sizeof(DWORD));

    void* mem = VirtualAlloc(nullptr, SZ, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return;  // stay on fallback
    std::memcpy(mem, tramp, SZ);

    DWORD old;
    VirtualProtect(mem, SZ, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), mem, SZ);

    NtUserSendInputCall = reinterpret_cast<UINT(__fastcall*)(ULONG, LPINPUT, int)>(mem);
}
