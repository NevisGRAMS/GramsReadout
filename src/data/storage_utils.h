#ifndef STORAGE_UTILS_H
#define STORAGE_UTILS_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace storage_utils {

// Run-start NVMe selection is performed in controller::Controller::SelectNvmeDataDirForRun()
// (Configure), shared by GramsReadout and tpc_daq. Mid-run failover uses kDiskFileSwitchMinFreeBytes
// in data_handler::MaybeSwitchDataDisk().

// Minimum free space to accept Configure / start a new run.
// If every NVMe candidate is below this, Configure fails and disk_full is reported.
// ~5 GiB leaves headroom for config logs, trigger sidecar, and the first readout file.
constexpr uint64_t kDiskRunStartMinFreeBytes = 5ULL * 1024ULL * 1024ULL * 1024ULL;

// Minimum free space on the *current* NVMe before opening the next readout file.
// Checked at every SwitchWriteFile (~5000 events). Below this → try the other NVMe.
// ~3 GiB ≈ two closed readout files (~1.47 GiB each).
constexpr uint64_t kDiskFileSwitchMinFreeBytes = 3ULL * 1024ULL * 1024ULL * 1024ULL;

struct PickResult {
    std::string path;
    uint64_t free_bytes{0};
    bool ok{false};
};

struct FreeBytesResult {
    std::optional<uint64_t> bytes;
    int statvfs_errno{0}; // 0 when statvfs succeeded
};

FreeBytesResult QueryFreeBytes(const std::string& path);
std::optional<uint64_t> GetFreeBytes(const std::string& path);

// DATA_PICK_MAX_FREE: unset or 0/false → legacy SSD0-first at run start; 1/true → pick NVMe with most free.
bool PickMaxFreeEnabled();

// NVMe write candidates only (DATA_SSD0_DIR, DATA_SSD1_DIR). Never includes SATA backup.
std::vector<std::string> GetNvmeCandidatesFromEnv();

PickResult PickMaxFree(const std::vector<std::string>& candidates, uint64_t min_free_bytes);

// Legacy order: SSD0 if it has min_free_bytes, else SSD1. Two statfs calls maximum.
PickResult PickSsd0First(const std::string& ssd0, const std::string& ssd1, uint64_t min_free_bytes);

// Run-start selection. pick_max_free=false uses PickSsd0First (same order as before).
PickResult SelectNvmeDataDir(bool pick_max_free, uint64_t min_free_bytes);

// True when every NVMe candidate is below min_free_bytes (run must not start).
bool AllNvmeDisksBelowMinFree(uint64_t min_free_bytes);

bool WriteDataBaseDirConf(const std::string& path);

} // namespace storage_utils

#endif // STORAGE_UTILS_H
