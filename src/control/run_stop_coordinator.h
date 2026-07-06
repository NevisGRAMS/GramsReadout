#ifndef RUN_STOP_COORDINATOR_H
#define RUN_STOP_COORDINATOR_H

#include <atomic>
#include <cstdint>

namespace data_handler {
class DataHandler;
}

namespace controller {

// Reasons the DAQ should leave the Running state. Extend here (e.g. TriggerRateHigh).
enum class RunStopReason : uint32_t {
    kNone = 0,
    kDiskFull = 1,
};

// Central place for automatic run-stop policies.
// Today: disk full → same path as manual TPC_Stop_Run (StopRun + kStopped).
// Future: trigger-rate cap, prescale-then-restart, etc.
class RunStopCoordinator {
public:
    void Poll(const data_handler::DataHandler* data_handler);

    bool ShouldStopRun() const {
        return pending_reason_.load(std::memory_order_relaxed) != RunStopReason::kNone;
    }

    RunStopReason PendingReason() const {
        return pending_reason_.load(std::memory_order_relaxed);
    }

    void ClearPendingReason() {
        pending_reason_.store(RunStopReason::kNone, std::memory_order_relaxed);
    }

private:
    std::atomic<RunStopReason> pending_reason_{RunStopReason::kNone};
};

} // namespace controller

#endif // RUN_STOP_COORDINATOR_H
