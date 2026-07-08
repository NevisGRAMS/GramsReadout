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

// DATA_MIRROR_MAX_MBYTES_PER_SEC: max copy rate in MiB/s; 0 = unlimited. Default 150.
uint64_t MirrorMaxBytesPerSec();

// NVMe mount that contains src_path, or empty if not under a known NVMe root.
std::string FindNvmeRootForPath(const std::string& src_path);

// Backup root for nvme_index (0=ssd0, 1=ssd1). Single-drive fallback uses any writable backup dir.
std::optional<std::string> GetBackupRootForNvmeIndex(size_t nvme_index);

// Minimum free space on backup before accepting a mirror copy (~1 GiB headroom).
constexpr uint64_t kMirrorMinBackupFreeBytes = 1ULL * 1024ULL * 1024ULL * 1024ULL;

// NVMe index (0/1) for src_path.
size_t NvmeIndexForPath(const std::string& src_path);

// Pick a writable backup root with enough free space. Tries mapped drive first, then fallback.
std::optional<std::string> SelectBackupRootForCopy(size_t nvme_index, uint64_t file_size_bytes);

// Build destination path under a specific backup root (preserves NVMe mount name).
std::string MirrorDestinationPathToRoot(const std::string& src_path, const std::string& backup_root);

// Destination path under backup, preserving NVMe mount name, e.g.
// /backup_data_sata_ssd1/write_data_nvme_ssd0/readout_data/...
// Empty when no backup with sufficient space is available.
std::string MirrorDestinationPath(const std::string& src_path);

// DATA_MIRROR_ENABLE: when true, GramsReadout/tpc_daq starts the data_mirror systemd service on startup.
bool MirrorDaemonStartRequested();
void TryStartMirrorDaemon();

} // namespace storage_utils

#endif // STORAGE_UTILS_H
