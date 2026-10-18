#include <windows.h>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <cstdio>
#include <MinHook.h>

// Standalone Windows x64 DLL; no Geode runtime dependency.
// Verified against the user's GeometryDash.exe (SHA-256 FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0).
// This build installs pass-through input hooks only; no early-click correction yet.
namespace {
using ButtonFn = void(__fastcall*)(void*, int);
std::atomic<bool> gInputHooksActive{false};
std::atomic<uint64_t> gPressCount{0};
std::atomic<uint64_t> gReleaseCount{0};
ButtonFn gOriginalPush = nullptr;
ButtonFn gOriginalRelease = nullptr;

void __fastcall onPush(void* player, int button) {
    gPressCount.fetch_add(1, std::memory_order_relaxed);
    if (gOriginalPush) gOriginalPush(player, button);
}
void __fastcall onRelease(void* player, int button) {
    gReleaseCount.fetch_add(1, std::memory_order_relaxed);
    if (gOriginalRelease) gOriginalRelease(player, button);
}

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

// A status file lets testers verify the hook without remote-process tooling.
void writeStatus(unsigned code) {
    wchar_t folder[MAX_PATH]{};
    DWORD length = GetTempPathW(MAX_PATH, folder);
    if (!length || length >= MAX_PATH) return;
    wchar_t path[MAX_PATH]{};
    if (swprintf_s(path, L"%sSmartJump-status-%lu.txt", folder, GetCurrentProcessId()) < 0) return;
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"w") != 0 || !file) return;
    fprintf(file, "SmartJump native hook diagnostic\\n");
    fprintf(file, "process_id=%lu\\n", GetCurrentProcessId());
    fprintf(file, "install_result=%u\\n", code);
    fprintf(file, "input_hooks_active=%u\\n", gInputHooksActive.load(std::memory_order_acquire) ? 1u : 0u);
    fprintf(file, "press_events=%llu\\n", static_cast<unsigned long long>(gPressCount.load(std::memory_order_relaxed)));
    fprintf(file, "release_events=%llu\\n", static_cast<unsigned long long>(gReleaseCount.load(std::memory_order_relaxed)));
    fprintf(file, "gameplay_correction_active=0\\n");
    fclose(file);
}

DWORD installInputHooksImpl();

DWORD WINAPI installInputHooks(LPVOID) {
    const unsigned result = installInputHooksImpl();
    writeStatus(result);
    if (result != 0) return result;
    for (;;) {
        Sleep(1000);
        writeStatus(0);
    }
}

DWORD installInputHooksImpl() {
#if defined(_M_X64)
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!matches(exe, kPushButtonRva, kPushButtonBytes) ||
        !matches(exe, kReleaseButtonRva, kReleaseButtonBytes) ||
        !matches(exe, kUpdateJumpRva, kUpdateJumpBytes) ||
        !matches(exe, kPostCollisionRva, kPostCollisionBytes)) return 1;

    if (MH_Initialize() != MH_OK) return 2;
    auto* base = reinterpret_cast<unsigned char*>(exe);
    void* push = base + kPushButtonRva;
    void* release = base + kReleaseButtonRva;
    if (MH_CreateHook(push, reinterpret_cast<LPVOID>(&onPush),
                      reinterpret_cast<LPVOID*>(&gOriginalPush)) != MH_OK) return 3;
    if (MH_CreateHook(release, reinterpret_cast<LPVOID>(&onRelease),
                      reinterpret_cast<LPVOID*>(&gOriginalRelease)) != MH_OK) {
        MH_RemoveHook(push);
        return 4;
    }
    if (MH_EnableHook(push) != MH_OK) return 5;
    if (MH_EnableHook(release) != MH_OK) {
        MH_DisableHook(push);
        return 6;
    }
    gInputHooksActive.store(true, std::memory_order_release);
    return 0;
#else
    return 7;
#endif
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
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, installInputHooks, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    // Deliberately no MH_Uninitialize during process teardown / loader lock.
    // Do not manually unload this DLL while the game is running.
    return TRUE;
}
