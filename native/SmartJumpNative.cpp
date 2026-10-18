#include "TimingPlanner.hpp"
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <MinHook.h>

// Standalone Windows x64 DLL; no Geode runtime dependency.
// Verified against the user's GeometryDash.exe (SHA-256 FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0).
// This build installs pass-through input hooks only; no early-click correction yet.
namespace {
using ButtonFn = void(__fastcall*)(void*, int);
using UpdateFn = void(__fastcall*)(void*, float);
std::atomic<bool> gTapExtensionEnabled{false};
std::atomic<uint64_t> gExtendedTaps{0};
std::mutex gPendingMutex;
struct PendingRelease { int button; ULONGLONG deadline; };
std::unordered_map<void*, PendingRelease> gPending;
UpdateFn gOriginalUpdate = nullptr;
std::atomic<bool> gInputHooksActive{false};
std::atomic<uint64_t> gPressCount{0};
std::atomic<uint64_t> gReleaseCount{0};
ButtonFn gOriginalPush = nullptr;
ButtonFn gOriginalRelease = nullptr;

void __fastcall onUpdate(void* player, float dt) {
    bool flush = false;
    int button = 0;
    {
        std::lock_guard<std::mutex> lock(gPendingMutex);
        auto it = gPending.find(player);
        if (it != gPending.end() && GetTickCount64() >= it->second.deadline) {
            button = it->second.button;
            gPending.erase(it);
            flush = true;
        }
    }
    if (flush && gOriginalRelease) gOriginalRelease(player, button);
    if (gOriginalUpdate) gOriginalUpdate(player, dt);
}
void __fastcall onPush(void* player, int button) {
    bool flush = false;
    int previousButton = 0;
    {
        std::lock_guard<std::mutex> lock(gPendingMutex);
        auto it = gPending.find(player);
        if (it != gPending.end()) {
            previousButton = it->second.button;
            gPending.erase(it);
            flush = true;
        }
    }
    if (flush && gOriginalRelease) gOriginalRelease(player, previousButton);
    gPressCount.fetch_add(1, std::memory_order_relaxed);
    if (gOriginalPush) gOriginalPush(player, button);
}
void __fastcall onRelease(void* player, int button) {
    gReleaseCount.fetch_add(1, std::memory_order_relaxed);
    // The predictive planner is connected to the input path, but until a
    // validated collision simulator supplies candidates, never delay input.
    const auto decision = smartjump::chooseTiming(std::span<const smartjump::Candidate>{});
    if (gTapExtensionEnabled.load(std::memory_order_acquire) && decision.shouldDelay) {
        std::lock_guard<std::mutex> lock(gPendingMutex);
        gPending[player] = {button, GetTickCount64() + 45};
        gExtendedTaps.fetch_add(1, std::memory_order_relaxed);
    } else if (gOriginalRelease) {
        gOriginalRelease(player, button);
    }
}

constexpr uintptr_t kPushButtonRva = 0x397F40;
constexpr uintptr_t kReleaseButtonRva = 0x3981D0;
constexpr uintptr_t kUpdateRva = 0x388D80;
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
HMODULE gSelfModule = nullptr;
void writeStatusTo(const wchar_t* path, unsigned code) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"w") != 0 || !file) return;
    fprintf(file, "SmartJump native diagnostics\n");
    fprintf(file, "process_id=%lu\n", GetCurrentProcessId());
    fprintf(file, "install_result=%u\\n", code);
    fprintf(file, "signature_codes=11:push,12:release,13:updateJump,14:postCollision\\n");
    fprintf(file, "input_hooks_active=%u\\n", gInputHooksActive.load() ? 1u : 0u);
    fprintf(file, "press_events=%llu\n", static_cast<unsigned long long>(gPressCount.load()));
    fprintf(file, "release_events=%llu\n", static_cast<unsigned long long>(gReleaseCount.load()));
    fprintf(file, "tap_extension_enabled=%u\n", gTapExtensionEnabled.load() ? 1u : 0u);
    fprintf(file, "extended_taps=%llu\n", static_cast<unsigned long long>(gExtendedTaps.load()));
    fprintf(file, "prediction_simulator_ready=0\\n");
    fprintf(file, "hitbox_scale_active=0\\n");
    HMODULE exe = GetModuleHandleW(nullptr);
    if (exe) {
        auto* address = reinterpret_cast<const unsigned char*>(
            reinterpret_cast<uintptr_t>(exe) + 0x397F40);
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(address, &mbi, sizeof(mbi)) == sizeof(mbi) &&
            mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            reinterpret_cast<uintptr_t>(address) + 16 <=
                reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) {
            fprintf(file, "push_actual_bytes=");
            for (unsigned i = 0; i < 16; ++i) fprintf(file, "%02X", address[i]);
            fprintf(file, "\\n");
        } else {
            fprintf(file, "push_actual_bytes=unreadable\\n");
        }
    }
    fclose(file);
}
void writeStatus(unsigned code) {
    wchar_t folder[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, folder) > 0) {
        wchar_t path[MAX_PATH]{};
        if (swprintf_s(path, L"%sSmartJump-status-%lu.txt", folder, GetCurrentProcessId()) >= 0)
            writeStatusTo(path, code);
    }
    wchar_t modulePath[MAX_PATH]{};
    if (gSelfModule && GetModuleFileNameW(gSelfModule, modulePath, MAX_PATH)) {
        wchar_t* separator = wcsrchr(modulePath, L'\\');
        if (separator) {
            *(separator + 1) = L'\0';
            wchar_t path[MAX_PATH]{};
            if (swprintf_s(path, L"%sSmartJump-status-%lu.txt", modulePath, GetCurrentProcessId()) >= 0)
                writeStatusTo(path, code);
        }
    }
}

DWORD installInputHooksImpl();

DWORD WINAPI installInputHooks(LPVOID) {
    writeStatus(100); // Worker started, before signature checks.
    const unsigned result = installInputHooksImpl();
    writeStatus(result);
    if (result != 0) return result;
    bool previousF8 = false;
    for (;;) {
        // Toggle only on the F8 key's down transition while GD has focus.
        bool focused = GetForegroundWindow() != nullptr &&
            GetWindowThreadProcessId(GetForegroundWindow(), nullptr) != 0;
        DWORD foregroundPid = 0;
        if (focused) GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
        const bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (foregroundPid == GetCurrentProcessId() && f8 && !previousF8) {
            const bool old = gTapExtensionEnabled.load(std::memory_order_acquire);
            gTapExtensionEnabled.store(!old, std::memory_order_release);
        }
        previousF8 = f8;
        writeStatus(0);
        Sleep(100);
    }
}

// A 14-byte FF 25 indirect jump is a common existing x64 detour.
// MinHook can attempt to chain through it; never accept arbitrary mismatches.
bool existingDetourAt(HMODULE exe, uintptr_t rva) noexcept {
    if (!exe) return false;
    auto* p = reinterpret_cast<const unsigned char*>(
        reinterpret_cast<uintptr_t>(exe) + rva);
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        reinterpret_cast<uintptr_t>(p) + 14 >
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize)
        return false;
    static constexpr unsigned char prefix[] = {0xFF, 0x25, 0, 0, 0, 0};
    if (memcmp(p, prefix, sizeof(prefix)) != 0) return false;
    uintptr_t destination = 0;
    memcpy(&destination, p + 6, sizeof(destination));
    MEMORY_BASIC_INFORMATION target{};
    if (VirtualQuery(reinterpret_cast<const void*>(destination), &target,
                     sizeof(target)) != sizeof(target) || target.State != MEM_COMMIT)
        return false;
    DWORD protection = target.Protect & 0xFF;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

DWORD installInputHooksImpl() {
#if defined(_M_X64)
    HMODULE exe = GetModuleHandleW(nullptr);
    // Distinct return codes identify the mismatched entry point.
    if (!matches(exe, kPushButtonRva, kPushButtonBytes) &&
        !existingDetourAt(exe, kPushButtonRva)) return 11;
    if (!matches(exe, kReleaseButtonRva, kReleaseButtonBytes) &&
        !existingDetourAt(exe, kReleaseButtonRva)) return 12;
    if (!matches(exe, kUpdateJumpRva, kUpdateJumpBytes) &&
        !existingDetourAt(exe, kUpdateJumpRva)) return 13;
    if (!matches(exe, kPostCollisionRva, kPostCollisionBytes) &&
        !existingDetourAt(exe, kPostCollisionRva)) return 14;

    if (MH_Initialize() != MH_OK) return 2;
    auto* base = reinterpret_cast<unsigned char*>(exe);
    void* push = base + kPushButtonRva;
    void* release = base + kReleaseButtonRva;
    void* update = base + kUpdateRva;
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
__declspec(dllexport) void __cdecl SmartJumpEnableTapExtension(bool enabled) noexcept {
    gTapExtensionEnabled.store(enabled, std::memory_order_release);
}
__declspec(dllexport) uint64_t __cdecl SmartJumpExtendedTaps() noexcept {
    return gExtendedTaps.load(std::memory_order_relaxed);
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
        gSelfModule = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, installInputHooks, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    // Deliberately no MH_Uninitialize during process teardown / loader lock.
    // Do not manually unload this DLL while the game is running.
    return TRUE;
}
