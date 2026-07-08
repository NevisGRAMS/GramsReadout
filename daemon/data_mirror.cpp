#include "mirror_engine.h"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

void PrintUsage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--max-mbps N]\n"
              << "  --max-mbps N   Initial mirror rate limit in MiB/s (default: env or 150)\n"
              << "  Runtime rate:  echo N > /var/lib/grams_mirror/max_mbps\n";
}

} // namespace

int main(int argc, char* argv[]) {
    mirror_engine::MirrorConfig config;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--max-mbps") == 0) {
            if (i + 1 >= argc) {
                PrintUsage(argv[0]);
                return 1;
            }
            config.max_mbytes_per_sec = std::strtoull(argv[++i], nullptr, 10);
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << argv[i] << '\n';
            PrintUsage(argv[0]);
            return 1;
        }
    }

    quill::BackendOptions backend_options;
    backend_options.sleep_duration = std::chrono::microseconds{100};
    quill::Backend::start(backend_options);

    quill::Logger* logger = quill::Frontend::create_or_get_logger(
        "mirror_logger",
        quill::Frontend::create_or_get_sink<quill::ConsoleSink>("mirror_sink"));
    LOG_INFO(logger, "GRAMS data mirror daemon starting (max_mbps={})",
             config.max_mbytes_per_sec == 0 ? 150ULL : config.max_mbytes_per_sec);

    const int rc = mirror_engine::Run(config);

    LOG_INFO(logger, "GRAMS data mirror daemon stopped");
    return rc;
}
