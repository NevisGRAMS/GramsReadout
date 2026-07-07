#include "mirror_worker.h"

#include "storage_utils.h"

#include "quill/Frontend.h"
#include "quill/Logger.h"
#include "quill/LogMacros.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <queue>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

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

namespace mirror_worker {
namespace {

struct MirrorJob {
    std::string src_path;
    std::string dst_path;
};

class MirrorWorker {
public:
    static MirrorWorker& Instance() {
        static MirrorWorker worker;
        return worker;
    }

    void Enqueue(const std::string& src_path) {
        const std::string dst_path = storage_utils::MirrorDestinationPath(src_path);
        if (dst_path.empty()) {
            LOG_WARNING(Logger(), "Mirror skipped (no writable backup dst) for {}", src_path);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(MirrorJob{src_path, dst_path});
        }
        LOG_INFO(Logger(), "Mirror queued {} -> {}", src_path, dst_path);
        cv_.notify_one();
        EnsureStarted();
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!worker_started_) {
                return;
            }
            shutdown_.store(true, std::memory_order_release);
        }
        cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        worker_started_ = false;
        shutdown_.store(false, std::memory_order_release);
    }

private:
    MirrorWorker() = default;

    quill::Logger* Logger() {
        return quill::Frontend::create_or_get_logger("readout_logger");
    }

    void EnsureStarted() {
        std::call_once(start_once_, [this]() {
            shutdown_.store(false, std::memory_order_release);
            worker_thread_ = std::thread([this]() { Run(); });
            worker_started_ = true;
        });
    }

    static void SetIdleIoPriority() {
#if defined(__linux__)
        syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0,
                IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));
#endif
    }

    static bool EnsureParentDirs(const std::string& file_path) {
        const auto pos = file_path.find_last_of('/');
        if (pos == std::string::npos) {
            return true;
        }

        std::string dir = file_path.substr(0, pos);
        std::string built;
        built.reserve(dir.size());

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

    static void ApplyRateLimit(uint64_t bytes_copied, uint64_t max_bytes_per_sec,
                               std::chrono::steady_clock::time_point& window_start,
                               uint64_t& window_bytes) {
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

    bool CopyFile(const MirrorJob& job) {
        quill::Logger* logger = Logger();
        const int in_fd = open(job.src_path.c_str(), O_RDONLY);
        if (in_fd < 0) {
            LOG_ERROR(logger, "Mirror open src failed {}: {}", job.src_path, std::strerror(errno));
            return false;
        }

        if (!EnsureParentDirs(job.dst_path)) {
            LOG_ERROR(logger, "Mirror mkdir failed for {}", job.dst_path);
            close(in_fd);
            return false;
        }

        const int out_fd = open(job.dst_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            LOG_ERROR(logger, "Mirror open dst failed {}: {}", job.dst_path, std::strerror(errno));
            close(in_fd);
            return false;
        }

        const uint64_t max_bytes_per_sec = storage_utils::MirrorMaxBytesPerSec();
        auto window_start = std::chrono::steady_clock::now();
        uint64_t window_bytes = 0;
        uint64_t total_bytes = 0;

        while (true) {
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
                    LOG_ERROR(logger, "Mirror read failed {}: {}", job.src_path, std::strerror(errno));
                    close(in_fd);
                    close(out_fd);
                    return false;
                }

                ssize_t written_total = 0;
                while (written_total < read_bytes) {
                    const ssize_t written = write(out_fd, buffer + written_total,
                                                  static_cast<size_t>(read_bytes - written_total));
                    if (written < 0) {
                        if (errno == EINTR) {
                            continue;
                        }
                        LOG_ERROR(logger, "Mirror write failed {}: {}", job.dst_path, std::strerror(errno));
                        close(in_fd);
                        close(out_fd);
                        return false;
                    }
                    written_total += written;
                }
                copied = read_bytes;
            }

            total_bytes += static_cast<uint64_t>(copied);
            ApplyRateLimit(static_cast<uint64_t>(copied), max_bytes_per_sec, window_start, window_bytes);
        }

        if (fsync(out_fd) != 0) {
            LOG_WARNING(logger, "Mirror fsync failed {}: {}", job.dst_path, std::strerror(errno));
        }

        close(in_fd);
        close(out_fd);
        LOG_INFO(logger, "Mirrored {} -> {} ({} B)", job.src_path, job.dst_path, total_bytes);
        return true;
    }

    void Run() {
        SetIdleIoPriority();
        quill::Logger* logger = Logger();
        LOG_INFO(logger, "Mirror worker started (max {} MiB/s)",
                 storage_utils::MirrorMaxBytesPerSec() / (1024ULL * 1024ULL));

        while (!shutdown_.load(std::memory_order_acquire)) {
            MirrorJob job;
            bool have_job = false;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() {
                    return shutdown_.load(std::memory_order_acquire) || !queue_.empty();
                });
                if (shutdown_.load(std::memory_order_acquire) && queue_.empty()) {
                    break;
                }
                if (queue_.empty()) {
                    continue;
                }
                job = std::move(queue_.front());
                queue_.pop();
                have_job = true;
            }

            if (have_job) {
                CopyFile(job);
            }
        }
    }

    std::atomic<bool> shutdown_{false};
    bool worker_started_{false};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<MirrorJob> queue_;
    std::once_flag start_once_;
    std::thread worker_thread_;
};

} // namespace

void EnqueueClosedFile(const std::string& src_path) {
    if (!storage_utils::MirrorEnabled()) {
        return;
    }
    MirrorWorker::Instance().Enqueue(src_path);
}

void Shutdown() {
    MirrorWorker::Instance().Stop();
}

} // namespace mirror_worker
