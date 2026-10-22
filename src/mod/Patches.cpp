#include "mod/Patches.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

/*
 * Removes the 64-block X/Z limit of structures in Minecraft Bedrock for
 * Windows (x64).  PC port of
 * https://github.com/fangkuaichaoge/StructureLimitRemover (Android).
 *
 * On Android the limit lives in one small shared helper.  The Windows build is
 * compiled with clang and that helper is inlined everywhere, so the checks are
 * patched in place at every call site instead.  All x64 compares here use an
 * imm8 operand (0x40), which cannot hold 999, so instead of raising the
 * constant the checks are switched off:
 *
 *   - "too big" branches (jcc)            -> NOP / unconditional jmp to "ok"
 *   - clamps   "cmp r,64 ; cmovge r,64"   -> cmovo   (never taken after cmp)
 *              "cmp r,64 ; cmovl  c,r"    -> cmovno  (always taken after cmp)
 *
 * The Y size is NOT touched: it is limited by the dimension height, which is
 * the same as vanilla.  The lower clamp (size >= 1) stays as well.
 *
 * Sites (Minecraft.Windows.exe 1.26.52, RVA):
 *   command       0xB5F37C2  /structure save        X/Z > 64 -> tooBig
 *   script_int    0xC3F3F74  script StructureManager (int BlockVolume)
 *   script_flt_x  0xC3F80F7  script StructureManager (float Vector3) X
 *   script_flt_z  0xC3F872F  script StructureManager (float Vector3) Z
 *   clamp_a       0x694AA19, 0x694B133  structure block screen size setter
 *   clamp_b       0x69503DD, 0x6950490  structure block screen size setter
 *   clamp_c       0x16EF0D2  server-side structure block data
 *   export        0x6F0D2F3  structure block "Export" template validation
 *
 * Every write is preceded by a check of the original bytes, so on an unknown
 * build nothing is ever written to a wrong place.
 */

namespace structure_limit_remover {

namespace {

struct Patch {
    size_t                offset; /* inside the signature match */
    std::vector<uint8_t>  original;
    std::vector<uint8_t>  replacement;
};

struct PatchSite {
    const char*        name;
    const char*        signature;
    std::vector<Patch> patches;
};

const std::vector<uint8_t> kNop6 = {0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00};

const PatchSite kSites[] = {
    /* /structure save: cmp ecx,40h / ja tooBig ... cmp edx,41h / jae tooBig */
    {"command",
     "83 F9 40 0F 87 ?? ?? ?? ?? 45 39 EF 0F 8F ?? ?? ?? ?? 83 FA 41 0F 83",
     {{3, {0x0F, 0x87}, kNop6}, {21, {0x0F, 0x83}, kNop6}}},

    /* script API (BlockVolume): cmp r14d,3Fh / jg err ... cmp r13d,40h / jge err */
    {"script_int",
     "41 83 FE 3F 0F 8F ?? ?? ?? ?? 41 39 D7 0F 8D ?? ?? ?? ?? 41 83 FD 40 0F 8D",
     {{4, {0x0F, 0x8F}, kNop6}, {23, {0x0F, 0x8D}, kNop6}}},

    /* script API (Vector3): ucomiss x,64.0 / jbe ok -> jmp ok */
    {"script_flt_x",
     "0F 2E 05 ?? ?? ?? ?? 0F 86 ?? ?? ?? ?? C7 45 ?? 40 00 00 00 89 45 ?? C7 45 ?? 40 00 00 00",
     {{7, {0x0F, 0x86}, {0x90, 0xE9}}}},

    /* script API (Vector3): ucomiss z,64.0 / jbe ok -> jmp ok */
    {"script_flt_z", "F3 41 0F 10 46 08 41 B4 01 0F 2E 05 ?? ?? ?? ?? 0F 86", {{16, {0x0F, 0x86}, {0x90, 0xE9}}}},

    /* structure block screen: clamp X (cmovge -> cmovo), Z (cmovl -> cmovno) */
    {"clamp_a",
     "83 F9 40 41 B8 40 00 00 00 41 0F 4D C8 44 39 D0 41 0F 4D C2 8B 97 ?? ?? ?? ?? 83 FA 40 44 0F 4C C2",
     {{11, {0x4D}, {0x40}}, {31, {0x4C}, {0x41}}}},

    {"clamp_b",
     "83 F9 40 BA 40 00 00 00 0F 4D CA 44 39 D0 41 0F 4D C2 44 8B 83 ?? ?? ?? ?? 41 83 F8 40 41 0F 4C D0",
     {{9, {0x4D}, {0x40}}, {31, {0x4C}, {0x41}}}},

    /* structure block "Export": template validation before saving the
     * .mcstructure, cmp [r13+30h],40h / jg fail ... cmp [r13+38h],40h / jg fail */
    {"export",
     "41 83 7D 30 40 7F ?? 0F BF D7 0F BF C9 29 CA 41 39 55 34 7F ?? 41 83 7D 38 40 7F",
     {{5, {0x7F}, {0x66, 0x90}}, {26, {0x7F}, {0x66, 0x90}}}},

    /* server-side structure block data (packet / block actor) */
    {"clamp_c",
     "41 83 FA 40 41 BB 40 00 00 00 45 0F 4D D3 41 39 F1 44 0F 4D CE 8B 75 ?? 83 FE 40 44 0F 4C DE",
     {{12, {0x4D}, {0x40}}, {29, {0x4C}, {0x41}}}},
};

struct Written {
    uint8_t*             address;
    std::vector<uint8_t> original;
};

std::vector<Written> gWritten;

struct Sig {
    std::vector<uint8_t> bytes;
    std::vector<bool>    mask; /* true = must match */
};

Sig parseSig(const char* s) {
    Sig sig;
    while (*s) {
        if (*s == ' ') {
            ++s;
            continue;
        }
        if (s[0] == '?') {
            sig.bytes.push_back(0);
            sig.mask.push_back(false);
            s += (s[1] == '?') ? 2 : 1;
        } else {
            sig.bytes.push_back(static_cast<uint8_t>(std::strtoul(std::string(s, 2).c_str(), nullptr, 16)));
            sig.mask.push_back(true);
            s += 2;
        }
    }
    return sig;
}

std::vector<uint8_t*> findAll(uint8_t* begin, size_t size, const Sig& sig) {
    std::vector<uint8_t*> hits;
    const size_t          n = sig.bytes.size();
    if (n == 0 || size < n) return hits;
    const uint8_t  first = sig.bytes[0];
    uint8_t*       p     = begin;
    uint8_t* const last  = begin + size - n;
    while (p <= last) {
        p = static_cast<uint8_t*>(std::memchr(p, first, static_cast<size_t>(last - p) + 1));
        if (!p) break;
        size_t i = 1;
        while (i < n && (!sig.mask[i] || p[i] == sig.bytes[i])) ++i;
        if (i == n) hits.push_back(p);
        ++p;
    }
    return hits;
}

bool getTextSection(uint8_t*& begin, size_t& size) {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos  = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt   = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* sec  = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 5) == 0) {
            begin = base + sec->VirtualAddress;
            size  = sec->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

bool writeCode(uint8_t* addr, const std::vector<uint8_t>& bytes) {
    DWORD old;
    if (!VirtualProtect(addr, bytes.size(), PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(addr, bytes.data(), bytes.size());
    VirtualProtect(addr, bytes.size(), old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, bytes.size());
    return true;
}

} // namespace

PatchResult applyPatches(ll::io::Logger& logger) {
    PatchResult result;
    uint8_t*    text;
    size_t      textSize;
    if (!getTextSection(text, textSize)) {
        logger.error(".text section not found");
        return result;
    }
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));

    for (const auto& site : kSites) {
        const auto hits = findAll(text, textSize, parseSig(site.signature));
        if (hits.empty()) {
            logger.warn("{}: not found", site.name);
            ++result.missing;
            continue;
        }
        for (uint8_t* hit : hits) {
            bool ok = true;
            for (const auto& p : site.patches) {
                uint8_t* at = hit + p.offset;
                if (std::memcmp(at, p.replacement.data(), p.replacement.size()) == 0) continue;
                if (std::memcmp(at, p.original.data(), p.original.size()) != 0) {
                    ok = false;
                    continue;
                }
                std::vector<uint8_t> original(at, at + p.replacement.size());
                if (writeCode(at, p.replacement)) {
                    gWritten.push_back({at, std::move(original)});
                } else {
                    ok = false;
                }
            }
            if (ok) {
                logger.debug("{}: patched at +0x{:X}", site.name, reinterpret_cast<uintptr_t>(hit) - base);
                ++result.patched;
            } else {
                logger.warn("{}: unexpected bytes at +0x{:X}", site.name, reinterpret_cast<uintptr_t>(hit) - base);
            }
        }
    }
    return result;
}

void restorePatches() {
    for (auto it = gWritten.rbegin(); it != gWritten.rend(); ++it) writeCode(it->address, it->original);
    gWritten.clear();
}

} // namespace structure_limit_remover
