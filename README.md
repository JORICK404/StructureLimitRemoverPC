# StructureLimitRemover

A [LeviLamina](https://github.com/LiteLDev/LeviLamina) mod for Minecraft Bedrock Edition (Windows)
that removes the 64-block limit on structure width (X) and depth (Z).

Android version: [fangkuaichaoge/StructureLimitRemover](https://github.com/fangkuaichaoge/StructureLimitRemover).

## Features

The X and Z limit is removed in:

- the structure block size fields
- the structure block **Export** button
- `/structure save`
- the Script API `world.structureManager`

The height (Y) is still limited by the world height, same as vanilla.

## Supported versions

| Minecraft | LeviLamina |
|---|---|
| 1.26.51 | 26.51.x |

## Installation

1. Install LeviLamina for the client, for example with [LeviLauncher](https://github.com/LiteLDev/LeviLauncher).
2. Download `StructureLimitRemover-x.x.x-client-windows-x64.zip` from
   [Releases](../../releases) and import it in LeviLauncher as a mod.
3. Start the game. The LeviLamina log should show
   `Structure size limit removed (10 site(s) patched, 0 missing)`.

## Notes

- There is no upper limit anymore. Saving very large structures can freeze the game or run it out
  of memory.
- After a game update the mod may stop working. In that case the log will show missing sites; the
  mod does not touch anything it can't find.

## Building

Requirements: [xmake](https://xmake.io), Visual Studio 2022, LLVM (clang-cl).

```bash
xmake f -a x64 -m release -p windows --target_type=client -y
xmake -y
```

The mod is placed in `bin/StructureLimitRemover/`.
