//
// Created by Jon Sensenig on 3/31/25.
// Modified by Kenichi Sakai on 8/20/2026
//

#include "CommunicationCodes.hh"
#include "FlightOps/GRAMS_TOF_DAQController.h"
#include "FlightOps/GRAMS_TOF_EventClient.h"
#include "FlightOps/GRAMS_TOF_LogCodec.h"

#include <asio.hpp>
#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <regex>

// --- Configuration ---
//const char* kHubIp = "192.168.100.100"; // Flight computer Hub
//const char* kHubIp = "192.168.1.100";   // Lab Hub
const char* kHubIp = "127.0.0.1";         // DummyHub / local
constexpr uint16_t kTofCommandPort = 50007;
constexpr uint16_t kTofStatusPort = 50006;

namespace pgrams::tofdaq {

using namespace communication;
using TOF_ControllerPtr = std::unique_ptr<GRAMS_TOF_DAQController>;

std::atomic_bool g_running(true);
std::condition_variable g_shutdown_cv;
std::mutex g_shutdown_mutex;

// --- Signal Handling ---
void SignalHandler(int signum) {
    if (signum == SIGTERM || signum == SIGINT || signum == SIGHUP) {
        g_running.store(false);
        {
            std::lock_guard<std::mutex> lock(g_shutdown_mutex);
        }
        g_shutdown_cv.notify_one();
    }
}

void SetupSignalHandlers() {
    struct sigaction sa{};
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    constexpr int signals_to_handle[] = {SIGTERM, SIGINT, SIGHUP};
    for (int sig : signals_to_handle) {
        if (sigaction(sig, &sa, nullptr) == -1) {
            std::cerr << "FATAL: Failed to set signal handler for " << sig << ": " << strerror(errno) << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, nullptr) == -1) {
        std::cerr << "WARNING: Failed to ignore SIGPIPE: " << strerror(errno) << std::endl;
    }
}

// --- Journal Forwarder Thread Class ---
class JournalLogForwarder {
public:
    explicit JournalLogForwarder(GRAMS_TOF_EventClient* client)
        : client_(client), running_(false) {}

    ~JournalLogForwarder() {
        stop();
    }

    void start() {
        if (running_) return;
        running_ = true;
        worker_ = std::thread(&JournalLogForwarder::run, this);
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        if (pipe_) {
            pclose(pipe_);
            pipe_ = nullptr;
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void run() {
        pipe_ = popen("journalctl -u tof_daq -f -n 0 -o cat 2>&1", "r");
        if (!pipe_) return;

        static const std::regex ansi_regex(R"(\x1B\[[0-9;]*[a-zA-Z])");

        char buffer[2048];
        while (running_ && fgets(buffer, sizeof(buffer), pipe_) != nullptr) {
            std::string line(buffer);
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (line.empty()) continue;

            std::string clean_line = std::regex_replace(line, ansi_regex, "");

            if (client_ && client_->isConnected()) {
                GRAMS_TOF_LogCodec::LogData data;
                data.run_number = 0;

                auto now = std::chrono::system_clock::now().time_since_epoch();
                data.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                data.level = 2; // Default INFO level

                std::strncpy(data.component, "JOURNAL", sizeof(data.component) - 1);
                data.component[sizeof(data.component) - 1] = '\0';

                data.message = clean_line;

                client_->sendLogData(TOFCommandCode::LOGGER_DATA_STREAM, data);
            }
        }

        if (pipe_) {
            pclose(pipe_);
            pipe_ = nullptr;
        }
    }

    GRAMS_TOF_EventClient* client_{nullptr};
    std::atomic<bool> running_{false};
    std::thread worker_;
    FILE* pipe_{nullptr};
};

// --- Logging Setup ---
void SetupLogging() {
    try {
        quill::BackendOptions backend_options;
        backend_options.sleep_duration = std::chrono::microseconds{100};
        quill::Backend::start(backend_options);

        // Standard Console Output Sink Only
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");

        auto logger = quill::Frontend::create_or_get_logger(
            "readout_logger",
            {std::move(console_sink)}
        );

        logger->set_log_level(quill::LogLevel::Info);
        QUILL_LOG_INFO(logger, "Logging system initialized successfully.");
    } catch (const std::exception& e) {
        std::cerr << "FATAL: Failed to initialize Quill logging: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

void JoinThread(std::thread &thread, quill::Logger *logger) {
    QUILL_LOG_DEBUG(logger, "Joining thread...");
    if (thread.joinable()) {
        try {
            thread.join();
            QUILL_LOG_DEBUG(logger, "Thread joined.");
        } catch (const std::system_error& e) {
            QUILL_LOG_ERROR(logger, "Error joining thread: {}", e.what());
        }
    }
}

bool WaitForThreadJoin(std::thread &thread, quill::Logger *logger) {
    auto fut = std::async(std::launch::async, [&] { JoinThread(thread, logger); });

    if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        QUILL_LOG_CRITICAL(logger, "Thread did not terminate gracefully within timeout.");
        return false;
    }
    return true;
}

void StartTofProcess(TOF_ControllerPtr &tof_ptr, std::thread &tof_thread, quill::Logger *logger) {
    if (tof_thread.joinable()) {
       QUILL_LOG_WARNING(logger, "GRAMS TOF DAQ controller already running!");
       return;
    }
    QUILL_LOG_DEBUG(logger, "Starting GRAMS TOF DAQ Control thread");

    GRAMS_TOF_DAQController::Config config;
    config.noFpgaMode = false;
    //config.noFpgaMode = true;
    config.commandListenPort = kTofCommandPort;
    config.eventTargetPort = kTofStatusPort;
    config.remoteEventHub = kHubIp;
    config.remoteCommandHub = kHubIp;
    config.configFile = "";

    if (config.configFile.empty()) {
        if (!GRAMS_TOF_Config::loadDefaultConfig()) {
            throw std::runtime_error("Configuration file not specified and default GLIB path failed to load.");
        }
        config.configFile = GRAMS_TOF_Config::instance().getConfigFilePath();
    }

    try {
        tof_ptr = std::make_unique<GRAMS_TOF_DAQController>(config);

        if (!tof_ptr->initialize()) {
            QUILL_LOG_ERROR(logger, "GRAMS TOF DAQ Controller initialization failed!");
            tof_ptr.reset(nullptr);
            return;
        }

        tof_thread = std::thread([&]() {
            tof_ptr->run();
        });

        QUILL_LOG_INFO(logger, "GRAMS TOF DAQ Controller thread successfully started.");

    } catch (const std::exception& e) {
        QUILL_LOG_ERROR(logger, "Failed to start GRAMS TOF DAQ Controller: {}", e.what());
        tof_ptr.reset(nullptr);
    }
}

void StopTofProcess(TOF_ControllerPtr &tof_ptr, std::thread &tof_thread, quill::Logger *logger) {
    if (tof_ptr) {
        QUILL_LOG_DEBUG(logger, "Signaling GRAMS TOF DAQ process to stop...");
        tof_ptr->stop();

        WaitForThreadJoin(tof_thread, logger);

        tof_ptr.reset(nullptr);
        QUILL_LOG_DEBUG(logger, "Stopped GRAMS TOF DAQ process and cleaned up resources.");
    } else {
        QUILL_LOG_WARNING(logger, "GRAMS TOF DAQ process not running!");
    }
}

} // namespace pgrams::tofdaq

int main() {
    pgrams::tofdaq::SetupSignalHandlers();
    pgrams::tofdaq::SetupLogging();

    quill::Logger* logger = quill::Frontend::create_or_get_logger("readout_logger");

    QUILL_LOG_INFO(logger, "TOF DAQ service starting up...");
    QUILL_LOG_INFO(logger, "Server IP: {}, Cmd Port: [{}], Status Port: [{}]", kHubIp, kTofCommandPort, kTofStatusPort);

    std::thread daq_thread;
    pgrams::tofdaq::TOF_ControllerPtr tof_ptr;
    std::unique_ptr<pgrams::tofdaq::JournalLogForwarder> journal_forwarder;

    try {
        pgrams::tofdaq::StartTofProcess(tof_ptr, daq_thread, logger);

        if (tof_ptr && tof_ptr->getEventClient()) {
            journal_forwarder = std::make_unique<pgrams::tofdaq::JournalLogForwarder>(tof_ptr->getEventClient());
            journal_forwarder->start();
            QUILL_LOG_INFO(logger, "Journalctl log forwarder thread successfully started.");
        }
    } catch (const std::exception& e) {
        QUILL_LOG_CRITICAL(logger, "Exception during initialization phase: {}", e.what());
        pgrams::tofdaq::g_running.store(false);
    }

    if (pgrams::tofdaq::g_running.load()) {
        QUILL_LOG_INFO(logger, "Service running. Waiting for termination signal...");
        std::unique_lock<std::mutex> lock(pgrams::tofdaq::g_shutdown_mutex);
        pgrams::tofdaq::g_shutdown_cv.wait(lock, [] { return !pgrams::tofdaq::g_running.load(); });
    } else {
        QUILL_LOG_WARNING(logger, "Initialization error detected. Proceeding directly to cleanup.");
    }

    QUILL_LOG_INFO(logger, "Starting graceful shutdown sequence...");

    if (journal_forwarder) {
        journal_forwarder->stop();
        QUILL_LOG_INFO(logger, "Journalctl log forwarder stopped.");
    }

    pgrams::tofdaq::StopTofProcess(tof_ptr, daq_thread, logger);
    QUILL_LOG_INFO(logger, "Shutdown sequence complete. Exiting...");

    return EXIT_SUCCESS;
}
