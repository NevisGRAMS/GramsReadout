#include "run_stop_coordinator.h"
#include "../data/data_handler.h"

namespace controller {

void RunStopCoordinator::Poll(const data_handler::DataHandler* data_handler) {
    if (data_handler == nullptr) {
        return;
    }
    if (data_handler->DiskStopRequested()) {
        pending_reason_.store(RunStopReason::kDiskFull, std::memory_order_relaxed);
    }
}

} // namespace controller
