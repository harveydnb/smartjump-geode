# Verified x64 RTTI → vtable tracing (exact uploaded executable)

Analysis performed directly on the user-supplied GeometryDash.exe matching SHA-256 `FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0`.

MSVC x64 CompleteObjectLocator candidates were validated by signature `1`, the TypeDescriptor image-relative RVA, `pSelf` pointing to the locator itself, an RTTI class-hierarchy reference in `.rdata`, and a pointer to the locator immediately before a vtable containing pointers into `.text`.

| Class | Validated candidate vtable RVAs |
| --- | --- |
| PlayerObject | `0x5C8918`, `0x5C8270`, `0x5C8990`, `0x5C7D98` |
| PlayLayer | `0x5C9388`, `0x5C9790`, `0x5C9D30`, `0x5C93B0`, `0x5C9758`, `0x5C9770`, `0x5C9D20`, `0x5C9378`, `0x5C93A0`, `0x5CA538` |
| GameObject | `0x556AA8`, `0x556360`, `0x5563B0` |

Examples of code pointers from those tables (all **RVAs**, not fixed runtime addresses):

- PlayerObject vtable `0x5C8270`: `0x4CFC03`, `0x385B30`, `0x4CFE85`, `0x4CFE7F`, `0x388D80`.
- PlayLayer vtable `0x5C9790`: `0x4CFC03`, `0x3A50E0`, `0x4CFE85`, `0x4CFE7F`, `0x237850`.
- GameObject vtable `0x5563B0`: `0x4CFC03`, `0x137CD0`, `0x4CFE85`, `0x4CFE7F`, `0x3BE80`.

**Limitations:** C++ classes may have multiple vtables due to inheritance. A vtable entry does not identify a jump or collision routine by itself; many entries are shared runtime thunks. No hooks or patches have been validated. These offsets are independently extracted from the uploaded binary, **not from the 2.1 gd-addresses repository**.

Next: analyze disassembly and call sites for the candidate code functions; validate the calling conventions, timing semantics, and object-type checks before attempting a native gameplay hook.

## Disassembly check: first class-specific entries

Using `objdump -d -M intel` on the exact PE64 file, the first class-specific entries were examined. For `PlayerObject`, RVA `0x385B30` saves `this` from RCX, calls RVA `0x3864A0`, tests bit 0 of EDX, and conditionally calls RVA `0x4D07AC` with allocation size `0xC48`. This is consistent with an MSVC scalar deleting destructor, **not a jump handler**. Thus treating the first class-specific vtable slot as a gameplay hook would be incorrect.

This is a verified negative result. Next work should trace non-destructor entries and cross-references from input and collision call sites; no safe patch address has been established.

## Further investigation: PlayerObject vtable slot 4 (zero-indexed)

Validated directly with `objdump -d -M intel --start-address=0x140388d80 --stop-address=0x140388e10 GeometryDash.exe` on the exact binary. The entry at PlayerObject vtable `0x5C8270 + 4*8` points to **RVA `0x388D80`**.

Unlike the deleting destructor in slot 1, this function has a substantial stack frame (`sub rsp, 0x110`), preserves floating-point registers, receives a float-like argument in XMM1 (`movaps xmm8, xmm1` and `cvtss2sd xmm14, xmm8`), multiplies it by a constant (`mulss xmm8, xmm6`), and reads a double from `[rcx+0x808]`. These observations make it a **plausible per-frame player update / physics candidate**, not proof of a jump-input or collision handler. The class and vtable slot are independently verified from the uploaded executable; no old-version offsets were used.

**Still needed:** trace callers and downstream calls, determine exact signature and effect, and establish testable invariants before writing any hook. Do not patch this RVA on the strength of these observations alone.

## Correction after inspecting more instructions (2026-10-18)

Extended disassembly of RVA `0x388D80` shows it compares a double at `this+0x808` with values at `this+0xAA0`, `this+0x810`, and `this+0x814`; computes an interpolation fraction (`divsd`), clamps with `minss`, and interpolates byte/word-sized fields around `this+0x97A`. It also calls a virtual method on the subobject at `this+0x140` and RVA `0x39D3A0`. These operations are **more consistent with time-based interpolation/animation state than with a jump-input handler**. The earlier physics-update hypothesis is therefore weakened and must not be used as a hook without additional evidence.

**Status:** The exact-game executable has been inspected directly, but no verified input or collision hook has been located. No third-party 2.1 offsets used.

## Confirmed import-thunk classification (direct disassembly)

The common entries near RVA `0x4CFE85` are **not ordinary player-function redirectors**. On the uploaded executable, `0x4CFE85` disassembles as `FF 25 2D 6F 04 00`, an x86-64 `jmp qword ptr [rip+0x46F2D]`, targeting an import address table cell at RVA `0x516DB8`. Nearby vtable entries `0x4CFE7F`, `0x4CFE79`, `0x4CFE73` and others have the same import-thunk form. These shared slots are **external imported functions**, not candidate game-specific jump hooks. The analysis classifier was corrected to detect the `FF 25` form in addition to `E9` relative jumps.

Disassembly of RVA `0x39D3A0` (previously examined as a callee from `0x388D80`) shows two virtual calls on objects referenced from `this+0x740` and `this+0x760`, and copies three-byte values into `this+0x8A0/+0x8A8` subobjects at offset `0x2F4`. Nearby code at RVA `0x39D430` handles these three-byte values and writes fields `this+0x97A..0x97F`. This strengthens the assessment that the `0x388D80` path is visual/color-state interpolation, **not verified jumping**.

No safe jump-input or collision hook has yet been established. All observations derive from the uploaded x64 executable, not old 2.1 offsets.

## Directly verified PlayerObject vtable overrides

Reading the uploaded executable's `.rdata` and disassembling the pointed-to code yields these zero-indexed slots of the `PlayerObject` vtable at RVA `0x5C8270`:

| Slot | PlayerObject entry RVA | GameObject entry RVA | Result |
|---:|---:|---:|---|
| 15 | `0x3A3910` | `0x198250` | Five-byte `E9` thunk to `0x198250` |
| 17 | `0x3A3920` | `0x198330` | Five-byte `E9` thunk to `0x198330` |
| 20 | `0x3A3930` | `0x198420` | Five-byte `E9` thunk to `0x198420` |
| 23 | `0x39C640` | `0x197B60` | Substantial distinct function, begins by writing word `0x0101` to `this+0x368`, then calls `0x197B60` |

The first three apparent overrides **collapse to exactly the same implementation** as `GameObject` when direct jumps are resolved. They must be excluded from player-specific hook candidates. Slot 23, `0x39C640`, is a genuine extended override and warrants further semantic analysis. Its initial operations and virtual calls on `this+0x758` do **not** establish that it handles jump input. No hook has been validated or shipped.
