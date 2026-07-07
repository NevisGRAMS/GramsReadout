#ifndef MIRROR_WORKER_H
#define MIRROR_WORKER_H

#include <string>

namespace mirror_worker {

// Enqueue a closed file for background copy to the SATA backup tree.
// No-op when DATA_MIRROR_ENABLE is false or no backup root is available.
void EnqueueClosedFile(const std::string& src_path);

// Stop the worker thread so the process can exit cleanly. Safe to call when mirroring is disabled.
void Shutdown();

} // namespace mirror_worker

#endif // MIRROR_WORKER_H
