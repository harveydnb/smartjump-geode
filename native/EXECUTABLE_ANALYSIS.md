# Exact executable analysis — standalone Windows target

Analyzed user-supplied `GeometryDash.exe` on 2026-10-18.

- SHA-256: `FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0`
- Format: PE32+ (64-bit Windows x86-64).
- Preferred image base: `0x140000000`; ASLR enabled (dynamic base).
- Entry point RVA: `0x4D1010` (preferred VA `0x1404D1010`).
- Code section reported size: `0x513200` bytes.
- PE timestamp: 2026-01-21 01:36:32 UTC (not reliable proof of game version).
- RTTI class-name strings include `PlayerObject` and `PlayLayer`.
- No verified gameplay function offsets, signatures, or calling conventions yet.

## Implementation status

The current native DLL is **not a gameplay mod**. Its exports only report API version, x64 support, desired scale, and `SmartJumpGameplayActive() == false`.

The next step is to reverse-engineer the relevant input, physics, and collision functions from the exact executable, including a safe way to identify the same functions when ASLR relocates the image. Any identified hook must be tested against the actual binary and should fail closed on mismatches. Early-click correction requires mode-specific timing/physics handling; scaling collision geometry requires separating spikes and walls from other object classes. Do not claim either works before in-game testing.

Level verification eligibility with modified gameplay cannot be guaranteed; no verification bypass is planned.
