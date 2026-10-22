#pragma once

#include "ll/api/io/Logger.h"

namespace structure_limit_remover {

struct PatchResult {
    int patched = 0; /* signature matches that are patched now */
    int missing = 0; /* sites whose signature was not found */
};

PatchResult applyPatches(ll::io::Logger& logger);

/// Writes the original bytes back to every place patched by applyPatches().
void restorePatches();

} // namespace structure_limit_remover
