#include <windows.h>
#include <cstdint>

// Standalone Windows x64 native foundation. No Geode dependency.
// No game hooks or gameplay modifications are present in this version.
extern "C" {
__declspec(dllexport) uint32_t __cdecl SmartJumpApiVersion() noexcept {
    return 1;
}
__declspec(dllexport) bool __cdecl SmartJumpSupportedArchitecture() noexcept {
#if defined(_M_X64)
    return true;
#else
    return false;
#endif
}
__declspec(dllexport) double __cdecl SmartJumpRequestedHitboxScale() noexcept {
    return 0.995;
}
__declspec(dllexport) bool __cdecl SmartJumpGameplayActive() noexcept {
    return false;
}
}
BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
