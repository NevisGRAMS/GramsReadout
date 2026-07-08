#ifndef MIRROR_ENGINE_H
#define MIRROR_ENGINE_H

#include <cstdint>

namespace mirror_engine {

struct MirrorConfig {
    // 0 → use DATA_MIRROR_MAX_MBYTES_PER_SEC env / default 150
    uint64_t max_mbytes_per_sec{0};
};

// Blocks until SIGINT/SIGTERM. Returns 0 on graceful shutdown.
int Run(const MirrorConfig& config = {});

} // namespace mirror_engine

#endif // MIRROR_ENGINE_H
