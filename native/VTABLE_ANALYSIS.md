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
