#include "mirror_engine.h"

#include "storage_utils.h"

#include <cctype>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <sys/syscall.h>
#ifndef IOPRIO_CLASS_IDLE
#define IOPRIO_CLASS_IDLE 3
#endif
#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif
#ifndef IOPRIO_PRIO_VALUE
#define IOPRIO_PRIO_VALUE(class, data) (((class) << 13) | (data))
#endif
#endif

namespace mirror_engine {
namespace {

constexpr size_t kInotifyBufferSize = 32 * (sizeof(struct inotify_event) + 256);

struct FileStat {
    uint64_t size{0};
    int64_t mtime{0};
};

struct FileMeta {
    uint32_t run_id{std::numeric_limits<uint32_t>::max()};
    size_t nvme_index{0};
    int priority{9};
    int segment{0};
};

struct PendingFile {
    std::string path;
    std::string dst_path;
    FileMeta meta;
};

std::atomic<bool> g_running{true};
std::atomic<uint64_t> g_max_bytes_per_sec{0};
std::atomic<int64_t> g_min_mtime{0};

std::string StateFilePath() {
    const char* env = std::getenv("DATA_MIRROR_STATE_FILE");
    if (env != nullptr && env[0] != '\0') {
        return env;
    }
    return "/var/lib/grams_mirror/state.tsv";
}

std::string RateFilePath() {
    const char* env = std::getenv("DATA_MIRROR_RATE_FILE");
    if (env != nullptr && env[0] != '\0') {
        return env;
    }
    return "/var/lib/grams_mirror/max_mbps";
}

int64_t ParseMinMtimeFromEnv() {
    const char* unix_env = std::getenv("DATA_MIRROR_MIN_MTIME");
    if (unix_env != nullptr && unix_env[0] != '\0') {
        char* end = nullptr;
        const long long parsed = std::strtoll(unix_env, &end, 10);
        if (end != unix_env) {
            return static_cast<int64_t>(parsed);
        }
    }

    const char* env = std::getenv("DATA_MIRROR_MIN_DATE");
    if (env == nullptr || env[0] == '\0') {
        return 0;
    }

    std::tm tm {};
    const char* end = strptime(env, "%Y-%m-%dT%H:%M:%S", &tm);
    if (end == nullptr || *end != '\0') {
        end = strptime(env, "%Y-%m-%d %H:%M:%S", &tm);
    }
    if (end == nullptr || *end != '\0') {
        end = strptime(env, "%Y-%m-%d", &tm);
    }
    if (end == nullptr || *end != '\0') {
        return 0;
    }
    tm.tm_isdst = -1;
    const time_t t = mktime(&tm);
    return t < 0 ? 0 : static_cast<int64_t>(t);
}

void InitGlobals(const MirrorConfig& config) {
    uint64_t bytes_per_sec = storage_utils::MirrorMaxBytesPerSec();
    if (config.max_mbytes_per_sec > 0) {
        bytes_per_sec = config.max_mbytes_per_sec * 1024ULL * 1024ULL;
    }
    g_max_bytes_per_sec.store(bytes_per_sec, std::memory_order_release);
    g_min_mtime.store(ParseMinMtimeFromEnv(), std::memory_order_release);
}

uint64_t CurrentMaxBytesPerSec() {
    static std::mutex rate_mutex;
    static std::chrono::steady_clock::time_point last_read{};
    static uint64_t cached = 0;

    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(rate_mutex);
    if (cached == 0 || now - last_read >= std::chrono::seconds(1)) {
        cached = g_max_bytes_per_sec.load(std::memory_order_acquire);
        std::ifstream in(RateFilePath());
        if (in.is_open()) {
            uint64_t mbps = 0;
            if (in >> mbps) {
                cached = mbps * 1024ULL * 1024ULL;
            }
        }
        last_read = now;
    }
    return cached;
}

void SetIdleIoPriority() {
#if defined(__linux__)
    syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));
#endif
}

bool EnsureParentDirs(const std::string& file_path) {
    const auto pos = file_path.find_last_of('/');
    if (pos == std::string::npos) {
        return true;
    }
    std::string dir = file_path.substr(0, pos);
    std::string built;
    for (size_t i = 0; i < dir.size(); ++i) {
        if (dir[i] == '/' && !built.empty()) {
            if (mkdir(built.c_str(), 0755) != 0 && errno != EEXIST) {
                return false;
            }
        }
        built.push_back(dir[i]);
    }
    if (!built.empty() && mkdir(built.c_str(), 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

bool StatFile(const std::string& path, FileStat* out) {
    struct stat info {};
    if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode)) {
        return false;
    }
    out->size = static_cast<uint64_t>(info.st_size);
    out->mtime = static_cast<int64_t>(info.st_mtime);
    return true;
}

bool PassesMinMtimeFilter(const std::string& path) {
    const int64_t min_mtime = g_min_mtime.load(std::memory_order_acquire);
    if (min_mtime == 0) {
        return true;
    }
    FileStat stat {};
    if (!StatFile(path, &stat)) {
        return false;
    }
    return stat.mtime >= min_mtime;
}

bool ParseUintAfter(const std::string& path, size_t offset, uint32_t* value, size_t* end_offset) {
    if (offset >= path.size() || !std::isdigit(static_cast<unsigned char>(path[offset]))) {
        return false;
    }
    uint64_t parsed = 0;
    size_t i = offset;
    while (i < path.size() && std::isdigit(static_cast<unsigned char>(path[i]))) {
        parsed = parsed * 10 + static_cast<uint64_t>(path[i] - '0');
        ++i;
    }
    if (parsed > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    *value = static_cast<uint32_t>(parsed);
    if (end_offset != nullptr) {
        *end_offset = i;
    }
    return true;
}

FileMeta ParseFileMeta(const std::string& path) {
    FileMeta meta;
    meta.nvme_index = storage_utils::NvmeIndexForPath(path);

    const auto grams_pos = path.find("pGRAMS_bin_");
    if (grams_pos != std::string::npos) {
        size_t end = 0;
        if (ParseUintAfter(path, grams_pos + 11, &meta.run_id, &end) && end < path.size()
            && path[end] == '_') {
            meta.priority = 1;
            uint32_t segment = 0;
            ParseUintAfter(path, end + 1, &segment, nullptr);
            meta.segment = static_cast<int>(segment);
            return meta;
        }
    }

    const auto trigger_pos = path.find("trigger_raw_");
    if (trigger_pos != std::string::npos) {
        size_t end = 0;
        if (ParseUintAfter(path, trigger_pos + 12, &meta.run_id, &end)) {
            meta.priority = 2;
            if (end < path.size() && path[end] == '_') {
                uint32_t segment = 0;
                ParseUintAfter(path, end + 1, &segment, nullptr);
                meta.segment = static_cast<int>(segment);
            }
            return meta;
        }
    }

    const auto pps_pos = path.find("pps_data_");
    if (pps_pos != std::string::npos) {
        size_t end = 0;
        if (ParseUintAfter(path, pps_pos + 9, &meta.run_id, &end)) {
            meta.priority = 3;
            if (end < path.size() && path[end] == '_') {
                uint32_t segment = 0;
                ParseUintAfter(path, end + 1, &segment, nullptr);
                meta.segment = static_cast<int>(segment);
            }
            return meta;
        }
    }

    const auto run_pos = path.find("/run_");
    if (run_pos != std::string::npos) {
        size_t end = 0;
        if (ParseUintAfter(path, run_pos + 5, &meta.run_id, &end)) {
            meta.priority = 4;
            return meta;
        }
    }

    return meta;
}

bool NeedsCopy(const std::string& src_path, const std::string& dst_path) {
    FileStat src_stat {};
    if (!StatFile(src_path, &src_stat)) {
        return false;
    }

    FileStat dst_stat {};
    if (!StatFile(dst_path, &dst_stat)) {
        return true;
    }
    return src_stat.size != dst_stat.size || src_stat.mtime > dst_stat.mtime;
}

void ApplyRateLimit(uint64_t bytes_copied, uint64_t max_bytes_per_sec,
                    std::chrono::steady_clock::time_point& window_start, uint64_t& window_bytes) {
    if (max_bytes_per_sec == 0) {
        return;
    }
    window_bytes += bytes_copied;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - window_start);
    if (elapsed.count() <= 0) {
        return;
    }
    const double expected_ms = (static_cast<double>(window_bytes) * 1000.0)
                               / static_cast<double>(max_bytes_per_sec);
    if (expected_ms > static_cast<double>(elapsed.count())) {
        const auto sleep_ms = static_cast<int64_t>(expected_ms - elapsed.count());
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    if (elapsed >= std::chrono::seconds(1)) {
        window_start = now;
        window_bytes = 0;
    }
}

bool CopyFile(const std::string& src_path, const std::string& dst_path) {
    FileStat src_stat {};
    if (!StatFile(src_path, &src_stat)) {
        return false;
    }

    const int in_fd = open(src_path.c_str(), O_RDONLY);
    if (in_fd < 0) {
        return false;
    }
#if defined(__linux__)
    posix_fadvise(in_fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
    if (!EnsureParentDirs(dst_path)) {
        close(in_fd);
        return false;
    }

    const std::string tmp_path = dst_path + ".mirror_partial";
    const int out_fd = open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        close(in_fd);
        return false;
    }

    auto window_start = std::chrono::steady_clock::now();
    uint64_t window_bytes = 0;
    bool copy_ok = true;

    while (g_running.load(std::memory_order_relaxed)) {
        const uint64_t max_bytes_per_sec = CurrentMaxBytesPerSec();
        ssize_t copied = 0;
#if defined(__linux__)
        copied = copy_file_range(in_fd, nullptr, out_fd, nullptr, 1024 * 1024, 0);
#endif
        if (copied <= 0) {
            char buffer[1024 * 1024];
            const ssize_t read_bytes = read(in_fd, buffer, sizeof(buffer));
            if (read_bytes == 0) {
                break;
            }
            if (read_bytes < 0) {
                if (errno == EINTR) {
                    continue;
                }
                copy_ok = false;
                break;
            }
            ssize_t written_total = 0;
            while (written_total < read_bytes) {
                const ssize_t written = write(out_fd, buffer + written_total,
                                              static_cast<size_t>(read_bytes - written_total));
                if (written < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    copy_ok = false;
                    break;
                }
                written_total += written;
            }
            if (!copy_ok) {
                break;
            }
            copied = read_bytes;
        }
        ApplyRateLimit(static_cast<uint64_t>(copied), max_bytes_per_sec, window_start, window_bytes);
    }

    if (!copy_ok || !g_running.load(std::memory_order_relaxed)) {
        close(in_fd);
        close(out_fd);
        unlink(tmp_path.c_str());
        return false;
    }

    fsync(out_fd);
    close(in_fd);
    close(out_fd);

    if (rename(tmp_path.c_str(), dst_path.c_str()) != 0) {
        unlink(tmp_path.c_str());
        return false;
    }

    struct timespec times[2];
    times[0].tv_sec = src_stat.mtime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = src_stat.mtime;
    times[1].tv_nsec = 0;
    utimensat(AT_FDCWD, dst_path.c_str(), times, 0);
    return true;
}

class MirrorDaemon {
public:
    void RequestStop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
    }

    void Enqueue(const std::string& src_path) {
        if (!PassesMinMtimeFilter(src_path)) {
            return;
        }

        FileStat src_stat {};
        if (!StatFile(src_path, &src_stat)) {
            return;
        }

        const size_t nvme_index = storage_utils::NvmeIndexForPath(src_path);
        const auto backup_root = storage_utils::SelectBackupRootForCopy(nvme_index, src_stat.size);
        if (!backup_root.has_value()) {
            return;
        }

        const std::string dst_path =
            storage_utils::MirrorDestinationPathToRoot(src_path, *backup_root);
        if (dst_path.empty() || !NeedsCopy(src_path, dst_path)) {
            return;
        }

        FileMeta meta = ParseFileMeta(src_path);
        meta.nvme_index = nvme_index;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pending_.count(src_path) != 0) {
                return;
            }
            pending_.insert(src_path);
            run_queues_[meta.run_id].push_back(PendingFile{src_path, dst_path, meta});
        }
        cv_.notify_one();
    }

    void LoadState() {
        std::ifstream in(StateFilePath());
        if (!in.is_open()) {
            return;
        }
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream iss(line);
            std::string path;
            uint64_t size = 0;
            int64_t mtime = 0;
            if (!(iss >> path >> size >> mtime)) {
                continue;
            }
            state_[path] = FileStat{size, mtime};
        }
    }

    void SaveStateEntry(const std::string& src_path) {
        FileStat stat {};
        if (!StatFile(src_path, &stat)) {
            return;
        }
        state_[src_path] = stat;

        EnsureParentDirs(StateFilePath());
        std::ofstream out(StateFilePath(), std::ios::app);
        if (!out.is_open()) {
            return;
        }
        out << src_path << '\t' << stat.size << '\t' << stat.mtime << '\n';
    }

    bool IsSynced(const std::string& src_path) const {
        const auto it = state_.find(src_path);
        if (it == state_.end()) {
            return false;
        }
        FileStat current {};
        if (!StatFile(src_path, &current)) {
            return false;
        }
        return current.size == it->second.size && current.mtime == it->second.mtime;
    }

    void ScanBacklog() {
        static const char* kSubdirs[] = {
            "readout_data", "trigger_data", "pps_data", "config_logs", "logs"
        };
        const auto nvme_roots = storage_utils::GetNvmeCandidatesFromEnv();
        for (const char* subdir : kSubdirs) {
            for (const auto& nvme_root : nvme_roots) {
                ScanDirectory(nvme_root + "/" + subdir);
            }
        }
    }

    void RunWorker() {
        SetIdleIoPriority();
        while (true) {
            std::optional<PendingFile> job = PopNext();
            if (!job.has_value()) {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return !running_ || HasPending(); });
                if (!running_ && !HasPending()) {
                    break;
                }
                continue;
            }

            if (!job->dst_path.empty() && NeedsCopy(job->path, job->dst_path)
                && CopyFile(job->path, job->dst_path)) {
                SaveStateEntry(job->path);
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                pending_.erase(job->path);
            }
        }
    }

    void RunInotify() {
        const int fd = inotify_init1(IN_NONBLOCK);
        if (fd < 0) {
            while (g_running.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            return;
        }

        std::vector<std::string> watch_roots;
        static const char* kSubdirs[] = {
            "readout_data", "trigger_data", "pps_data", "config_logs", "logs"
        };
        for (const char* subdir : kSubdirs) {
            for (const auto& nvme_root : storage_utils::GetNvmeCandidatesFromEnv()) {
                const std::string path = nvme_root + "/" + subdir;
                const int wd = inotify_add_watch(fd, path.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO);
                if (wd >= 0) {
                    watch_roots.emplace_back(path);
                }
            }
        }

        std::vector<char> buffer(kInotifyBufferSize);
        while (g_running.load(std::memory_order_acquire)) {
            const ssize_t len = read(fd, buffer.data(), buffer.size());
            if (len < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            ssize_t offset = 0;
            while (offset < len) {
                const auto* event = reinterpret_cast<const struct inotify_event*>(buffer.data() + offset);
                offset += sizeof(struct inotify_event) + event->len;
                if (event->len == 0) {
                    continue;
                }
                const std::string name(event->name);
                for (const auto& root : watch_roots) {
                    if (name.find('/') != std::string::npos) {
                        continue;
                    }
                    Enqueue(root + "/" + name);
                }
            }
        }
        close(fd);
    }

private:
    bool HasPending() const {
        for (const auto& [run_id, files] : run_queues_) {
            (void)run_id;
            if (!files.empty()) {
                return true;
            }
        }
        return false;
    }

    std::optional<PendingFile> PopNext() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto pick_from_run = [&](uint32_t run_id) -> std::optional<PendingFile> {
            auto it = run_queues_.find(run_id);
            if (it == run_queues_.end() || it->second.empty()) {
                return std::nullopt;
            }
            auto& files = it->second;
            auto best = files.begin();
            for (auto iter = files.begin(); iter != files.end(); ++iter) {
                if (iter->meta.priority < best->meta.priority
                    || (iter->meta.priority == best->meta.priority
                        && iter->meta.segment < best->meta.segment)
                    || (iter->meta.priority == best->meta.priority
                        && iter->meta.segment == best->meta.segment
                        && iter->meta.nvme_index < best->meta.nvme_index)
                    || (iter->meta.priority == best->meta.priority
                        && iter->meta.segment == best->meta.segment
                        && iter->meta.nvme_index == best->meta.nvme_index
                        && iter->path < best->path)) {
                    best = iter;
                }
            }
            PendingFile job = *best;
            files.erase(best);
            if (files.empty()) {
                run_queues_.erase(it);
            }
            active_run_id_ = run_id;
            return job;
        };

        if (active_run_id_.has_value()) {
            if (auto job = pick_from_run(*active_run_id_)) {
                return job;
            }
            active_run_id_.reset();
        }

        for (auto it = run_queues_.begin(); it != run_queues_.end(); ++it) {
            if (!it->second.empty()) {
                active_run_id_ = it->first;
                return pick_from_run(it->first);
            }
        }
        return std::nullopt;
    }

    void ScanDirectory(const std::string& dir_path) {
        if (access(dir_path.c_str(), R_OK) != 0) {
            return;
        }

        const auto scan = [&](const std::string& path, auto& self) -> void {
            struct stat info {};
            if (stat(path.c_str(), &info) != 0) {
                return;
            }
            if (S_ISREG(info.st_mode)) {
                if (!IsSynced(path)) {
                    Enqueue(path);
                }
                return;
            }
            if (!S_ISDIR(info.st_mode)) {
                return;
            }
            DIR* dir = opendir(path.c_str());
            if (dir == nullptr) {
                return;
            }
            while (const dirent* entry = readdir(dir)) {
                if (entry->d_name[0] == '.') {
                    continue;
                }
                self(path + "/" + entry->d_name, self);
            }
            closedir(dir);
        };

        scan(dir_path, scan);
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<uint32_t, std::vector<PendingFile>> run_queues_;
    std::optional<uint32_t> active_run_id_;
    std::set<std::string> pending_;
    std::unordered_map<std::string, FileStat> state_;
    bool running_{true};
};

MirrorDaemon* g_daemon = nullptr;

void HandleSignal(int) {
    g_running.store(false, std::memory_order_release);
    if (g_daemon != nullptr) {
        g_daemon->RequestStop();
    }
}

} // namespace

int Run(const MirrorConfig& config) {
    InitGlobals(config);
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    MirrorDaemon daemon;
    g_daemon = &daemon;
    daemon.LoadState();
    daemon.ScanBacklog();

    std::thread worker([&daemon]() { daemon.RunWorker(); });
    std::thread watcher([&daemon]() { daemon.RunInotify(); });

    while (g_running.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    daemon.RequestStop();
    if (watcher.joinable()) {
        watcher.join();
    }
    if (worker.joinable()) {
        worker.join();
    }

    g_daemon = nullptr;
    return 0;
}

} // namespace mirror_engine
