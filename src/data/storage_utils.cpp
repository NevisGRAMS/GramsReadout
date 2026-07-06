#include "storage_utils.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sys/statvfs.h>

namespace storage_utils {

namespace {

constexpr const char* kDefaultNvmeSsd0 = "/write_data_nvme_ssd0";
constexpr const char* kDefaultNvmeSsd1 = "/write_data_nvme_ssd1";

bool EnvFlagTrue(const char* value, bool default_value) {
    if (value == nullptr) {
        return default_value;
    }
    const std::string flag(value);
    if (flag == "0" || flag == "false" || flag == "FALSE" || flag == "no" || flag == "NO") {
        return false;
    }
    if (flag == "1" || flag == "true" || flag == "TRUE" || flag == "yes" || flag == "YES") {
        return true;
    }
    return default_value;
}

std::string TrimPath(const char* value) {
    if (value == nullptr) {
        return {};
    }
    std::string path(value);
    while (!path.empty() && (path.front() == ' ' || path.front() == '\t')) {
        path.erase(path.begin());
    }
    while (!path.empty() && (path.back() == ' ' || path.back() == '\t')) {
        path.pop_back();
    }
    return path;
}

} // namespace

FreeBytesResult QueryFreeBytes(const std::string& path) {
    FreeBytesResult result;
    struct statvfs info {};
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (statvfs(path.c_str(), &info) == 0) {
            result.bytes = static_cast<uint64_t>(info.f_bavail)
                           * static_cast<uint64_t>(info.f_frsize);
            return result;
        }
        result.statvfs_errno = errno;
        if (errno != EINTR) {
            break;
        }
    }
    return result;
}

std::optional<uint64_t> GetFreeBytes(const std::string& path) {
    return QueryFreeBytes(path).bytes;
}

bool PickMaxFreeEnabled() {
    // Unset → legacy SSD0-first (same as pre-failover behavior). Set to 1 for max-free at run start.
    return EnvFlagTrue(std::getenv("DATA_PICK_MAX_FREE"), false);
}

std::vector<std::string> GetNvmeCandidatesFromEnv() {
    std::vector<std::string> candidates;
    const std::string ssd0 = TrimPath(std::getenv("DATA_SSD0_DIR"));
    const std::string ssd1 = TrimPath(std::getenv("DATA_SSD1_DIR"));
    candidates.emplace_back(ssd0.empty() ? kDefaultNvmeSsd0 : ssd0);
    candidates.emplace_back(ssd1.empty() ? kDefaultNvmeSsd1 : ssd1);
    return candidates;
}

PickResult PickMaxFree(const std::vector<std::string>& candidates, uint64_t min_free_bytes) {
    PickResult best;
    for (const auto& candidate : candidates) {
        const auto free_bytes = GetFreeBytes(candidate);
        if (!free_bytes.has_value()) {
            continue;
        }
        if (best.path.empty() || *free_bytes > best.free_bytes) {
            best.path = candidate;
            best.free_bytes = *free_bytes;
            best.ok = *free_bytes >= min_free_bytes;
        }
    }
    return best;
}

PickResult PickSsd0First(const std::string& ssd0, const std::string& ssd1, uint64_t min_free_bytes) {
    const auto free0 = GetFreeBytes(ssd0);
    if (free0.has_value() && *free0 >= min_free_bytes) {
        return {ssd0, *free0, true};
    }

    const auto free1 = GetFreeBytes(ssd1);
    if (free1.has_value() && *free1 >= min_free_bytes) {
        return {ssd1, *free1, true};
    }

    if (free0.has_value() && free1.has_value()) {
        if (*free1 > *free0) {
            return {ssd1, *free1, false};
        }
        return {ssd0, *free0, false};
    }
    if (free0.has_value()) {
        return {ssd0, *free0, false};
    }
    if (free1.has_value()) {
        return {ssd1, *free1, false};
    }
    return {};
}

PickResult SelectNvmeDataDir(bool pick_max_free, uint64_t min_free_bytes) {
    const auto candidates = GetNvmeCandidatesFromEnv();
    if (candidates.empty()) {
        return {};
    }
    if (pick_max_free || candidates.size() == 1) {
        return PickMaxFree(candidates, min_free_bytes);
    }
    return PickSsd0First(candidates.at(0), candidates.at(1), min_free_bytes);
}

bool AllNvmeDisksBelowMinFree(uint64_t min_free_bytes) {
    const auto candidates = GetNvmeCandidatesFromEnv();
    if (candidates.empty()) {
        return true;
    }
    for (const auto& candidate : candidates) {
        const auto free_bytes = GetFreeBytes(candidate);
        if (free_bytes.has_value() && *free_bytes >= min_free_bytes) {
            return false;
        }
    }
    return true;
}

bool WriteDataBaseDirConf(const std::string& path) {
    std::ofstream env_file("/run_number/data_ssd.conf");
    if (!env_file.is_open()) {
        return false;
    }
    env_file << "DATA_BASE_DIR=" << path << "\n";
    return true;
}

} // namespace storage_utils
