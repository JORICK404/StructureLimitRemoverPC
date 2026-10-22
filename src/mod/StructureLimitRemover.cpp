#include "mod/StructureLimitRemover.h"

#include "ll/api/mod/RegisterHelper.h"

#include "mod/Patches.h"

namespace structure_limit_remover {

StructureLimitRemover& StructureLimitRemover::getInstance() {
    static StructureLimitRemover instance;
    return instance;
}

bool StructureLimitRemover::load() { return true; }

bool StructureLimitRemover::enable() {
    auto& logger = getSelf().getLogger();
    const auto result = applyPatches(logger);
    if (result.patched == 0) {
        logger.error("No structure size checks were found, this game version is not supported");
        return false;
    }
    logger.info("Structure size limit removed ({} site(s) patched, {} missing)", result.patched, result.missing);
    return true;
}

bool StructureLimitRemover::disable() {
    restorePatches();
    getSelf().getLogger().info("Structure size limit restored");
    return true;
}

} // namespace structure_limit_remover

LL_REGISTER_MOD(structure_limit_remover::StructureLimitRemover, structure_limit_remover::StructureLimitRemover::getInstance());
