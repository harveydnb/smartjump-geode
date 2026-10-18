#include <windows.h>
#include <cstdint>
#include <cstring>

// Standalone Windows x64 DLL; no Geode runtime dependency.
// Verified against the user's GeometryDash.exe (SHA-256 FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0).
// This build only validates function entry points. It does not install hooks.
namespace {
constexpr uintptr_t kPushButtonRva = 0x397F40;
constexpr uintptr_t kReleaseButtonRva = 0x3981D0;
constexpr uintptr_t kUpdateJumpRva = 0x38B900;
constexpr uintptr_t kPostCollisionRva = 0x38D580;
constexpr unsigned char kPushButtonBytes[] = {0x40, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x28};
constexpr unsigned char kReleaseButtonBytes[] = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20};
constexpr unsigned char kUpdateJumpBytes[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x55, 0x56, 0x57};
constexpr unsigned char kPostCollisionBytes[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0xF3, 0x0F, 0x11, 0x48, 0x10};

template<size_t N>
bool matches(HMODULE module, uintptr_t rva, const unsigned char (&bytes)[N]) noexcept {
    if (!module) return false;
    auto* address = reinterpret_cast<const unsigned char*>(reinterpret_cast<uintptr_t>(module) + rva);
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    return std::memcmp(address, bytes, N) == 0;
}
}

extern "C" {
__declspec(dllexport) uint32_t __cdecl SmartJumpApiVersion() noexcept { return 2; }
__declspec(dllexport) bool __cdecl SmartJumpSupportedArchitecture() noexcept {
#if defined(_M_X64)
    return true;
#else
    return false;
#endif
}
__declspec(dllexport) double __cdecl SmartJumpRequestedHitboxScale() noexcept { return 0.995; }
__declspec(dllexport) bool __cdecl SmartJumpGameplayActive() noexcept { return false; }

// Read-only validation of four candidate game function entry points.
// Not proof of complete executable identity; not a gameplay hook.
__declspec(dllexport) bool __cdecl SmartJumpKnownFunctionSignaturesMatch() noexcept {
#if defined(_M_X64)
    HMODULE exe = GetModuleHandleW(nullptr);
    return matches(exe, kPushButtonRva, kPushButtonBytes)
        && matches(exe, kReleaseButtonRva, kReleaseButtonBytes)
        && matches(exe, kUpdateJumpRva, kUpdateJumpBytes)
        && matches(exe, kPostCollisionRva, kPostCollisionBytes);
#else
    return false;
#endif
}
}
BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
