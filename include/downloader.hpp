#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdint>

enum class DownloadStatus {
    Idle,
    Connecting,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled
};

struct DownloadStats {
    DownloadStatus status = DownloadStatus::Idle;
    std::string url;
    std::string custom_filename;
    std::string resolved_filename;
    std::string output_dir = "./downloads";
    std::string target_filepath;
    std::string part_filepath;
    
    int http_status = 0;
    uint64_t total_bytes = 0;
    uint64_t downloaded_bytes = 0;
    
    double speed_bps = 0.0;
    int eta_seconds = -1;
    float progress = 0.0f; // 0.0 to 1.0
    
    std::string error_message;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point finish_time;
};

class Downloader {
public:
    using ProgressCallback = std::function<void(const DownloadStats&)>;
    using LogCallback = std::function<void(const std::string& message, const std::string& level)>;
    using CompleteCallback = std::function<void(const DownloadStats&)>;

    Downloader();
    ~Downloader();

    void set_on_progress(ProgressCallback cb);
    void set_on_log(LogCallback cb);
    void set_on_complete(CompleteCallback cb);

    bool start(const std::string& url, const std::string& output_dir = "./downloads", const std::string& custom_filename = "");
    bool pause();
    bool resume();
    bool cancel();
    void reset();

    DownloadStats get_stats() const;
    bool is_active() const;
    bool is_paused() const;
    std::string get_status_string() const;

private:
    void download_worker(uint64_t resume_offset);
    void log(const std::string& message, const std::string& level = "INFO");

    mutable std::mutex stats_mutex_;
    DownloadStats stats_;

    std::atomic<bool> is_running_{false};
    std::atomic<bool> pause_requested_{false};
    std::atomic<bool> cancel_requested_{false};

    std::jthread worker_thread_;

    ProgressCallback on_progress_;
    LogCallback on_log_;
    CompleteCallback on_complete_;
};
