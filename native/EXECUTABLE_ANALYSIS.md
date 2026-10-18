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

## Exact-build RTTI results (independently extracted)

The following RVAs were obtained directly from the uploaded executable, **not** from gd-addresses or any older-version offset list. They describe C++ type metadata, **not gameplay function entry points**:

| Class | Decorated type-name file offset | TypeDescriptor RVA | Candidate image-relative references (RVAs) |
| --- | --- | --- | --- |
| PlayerObject | `0x6BCC20` | `0x6BEA10` | `0x644C34`, `0x644C50`, `0x644D44`, `0x644D6C`, `0x644E4C` |
| PlayLayer | `0x6BCC78` | `0x6BEA68` | `0x644F04`, `0x644F2C`, `0x644FAC`, `0x64500C`, `0x6450D4`, `0x6451FC`, `0x645224`, `0x645268`, `0x645314`, `0x6453CC`, `0x645424` |
| GameObject | `0x6BA7C0` | `0x6BC5B0` | `0x632A04`, `0x632A2C`, `0x632A7C`, `0x632B20` |

These candidate references still need to be checked as MSVC CompleteObjectLocator fields, associated with class hierarchies and vtables, and mapped to specific input/collision behavior. No code patch or injection is justified from this metadata alone.
