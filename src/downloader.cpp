#include "downloader.hpp"
#include "utils.hpp"
#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <regex>

Downloader::Downloader() = default;

Downloader::~Downloader() {
    cancel();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void Downloader::set_on_progress(ProgressCallback cb) {
    on_progress_ = std::move(cb);
}

void Downloader::set_on_log(LogCallback cb) {
    on_log_ = std::move(cb);
}

void Downloader::set_on_complete(CompleteCallback cb) {
    on_complete_ = std::move(cb);
}

void Downloader::log(const std::string& message, const std::string& level) {
    if (on_log_) {
        on_log_(message, level);
    }
}

bool Downloader::start(const std::string& url, const std::string& output_dir, const std::string& custom_filename) {
    if (is_running_.load()) {
        log("Unduhan masih berjalan, tidak dapat memulai unduhan baru.", "WARNING");
        return false;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = DownloadStats();
        stats_.url = url;
        stats_.output_dir = output_dir.empty() ? "./downloads" : output_dir;
        stats_.custom_filename = custom_filename;
        stats_.status = DownloadStatus::Connecting;
        stats_.start_time = std::chrono::system_clock::now();
    }

    pause_requested_.store(false);
    cancel_requested_.store(false);
    is_running_.store(true);

    worker_thread_ = std::jthread([this]() {
        this->download_worker(0);
    });

    return true;
}

bool Downloader::pause() {
    if (!is_running_.load()) return false;
    pause_requested_.store(true);
    log("Meminta jeda unduhan...", "INFO");
    return true;
}

bool Downloader::resume() {
    if (is_running_.load()) return false;
    if (stats_.status != DownloadStatus::Paused) return false;

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    uint64_t resume_offset = stats_.downloaded_bytes;
    pause_requested_.store(false);
    cancel_requested_.store(false);
    is_running_.store(true);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Connecting;
    }

    log("Melanjutkan unduhan dari byte " + std::to_string(resume_offset) + "...", "INFO");

    worker_thread_ = std::jthread([this, resume_offset]() {
        this->download_worker(resume_offset);
    });

    return true;
}

bool Downloader::cancel() {
    if (!is_running_.load()) {
        if (stats_.status == DownloadStatus::Paused) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.status = DownloadStatus::Cancelled;
            log("Unduhan yang dijeda telah dibatalkan.", "INFO");
            return true;
        }
        return false;
    }
    cancel_requested_.store(true);
    log("Membatalkan unduhan...", "WARNING");
    return true;
}

void Downloader::reset() {
    cancel();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = DownloadStats();
}

DownloadStats Downloader::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

bool Downloader::is_active() const {
    return is_running_.load();
}

bool Downloader::is_paused() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.status == DownloadStatus::Paused;
}

std::string Downloader::get_status_string() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    switch (stats_.status) {
        case DownloadStatus::Idle: return "IDLE";
        case DownloadStatus::Connecting: return "CONNECTING";
        case DownloadStatus::Downloading: return "DOWNLOADING";
        case DownloadStatus::Paused: return "PAUSED";
        case DownloadStatus::Completed: return "COMPLETED";
        case DownloadStatus::Failed: return "FAILED";
        case DownloadStatus::Cancelled: return "CANCELLED";
    }
    return "UNKNOWN";
}

void Downloader::download_worker(uint64_t resume_offset) {
    std::string url;
    std::string out_dir;
    std::string custom_fname;
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        url = stats_.url;
        out_dir = stats_.output_dir;
        custom_fname = stats_.custom_filename;
    }

    log("Membuka koneksi internet untuk: " + url, "INFO");

    HINTERNET hInternet = InternetOpenA("FTXUIDownloader/1.0 (Windows)", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        DWORD err = GetLastError();
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Failed;
        stats_.error_message = "InternetOpen gagal dengan kode error: " + std::to_string(err);
        is_running_.store(false);
        log(stats_.error_message, "ERROR");
        return;
    }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION |
                  INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;

    if (url.rfind("https://", 0) == 0) {
        flags |= INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID;
    }

    std::string headers;
    if (resume_offset > 0) {
        headers = "Range: bytes=" + std::to_string(resume_offset) + "-\r\n";
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), headers.empty() ? NULL : headers.c_str(), 
                                      (DWORD)headers.length(), flags, 0);
    if (!hUrl) {
        DWORD err = GetLastError();
        InternetCloseHandle(hInternet);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Failed;
        stats_.error_message = "Koneksi URL gagal (Error " + std::to_string(err) + "). Periksa alamat URL dan jaringan.";
        is_running_.store(false);
        log(stats_.error_message, "ERROR");
        return;
    }

    // Query HTTP Status Code
    DWORD http_code = 0;
    DWORD code_len = sizeof(http_code);
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &http_code, &code_len, NULL)) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.http_status = static_cast<int>(http_code);
    }
    log("Status respons server: HTTP " + std::to_string(http_code), http_code >= 400 ? "ERROR" : "INFO");

    if (http_code >= 400) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Failed;
        stats_.error_message = "Server mengembalikan status HTTP " + std::to_string(http_code);
        is_running_.store(false);
        return;
    }

    // Determine filename
    std::string resolved_filename = custom_fname;
    if (resolved_filename.empty()) {
        char disp_buf[512] = {0};
        DWORD disp_len = sizeof(disp_buf);
        if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_DISPOSITION, disp_buf, &disp_len, NULL)) {
            std::string disp_str(disp_buf);
            // Search for filename="..." or filename=...
            std::regex re("filename\\*?=['\"]?(?:UTF-8'')?([^;'\"]+)['\"]?");
            std::smatch match;
            if (std::regex_search(disp_str, match, re) && match.size() > 1) {
                resolved_filename = Utils::sanitize_filename(match[1].str());
                log("Nama file dari Content-Disposition: " + resolved_filename, "INFO");
            }
        }
    }

    if (resolved_filename.empty()) {
        resolved_filename = Utils::extract_filename_from_url(url);
    }

    // Ensure output directory exists
    try {
        std::filesystem::create_directories(out_dir);
    } catch (const std::exception& e) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Failed;
        stats_.error_message = "Gagal membuat folder tujuan: " + std::string(e.what());
        is_running_.store(false);
        log(stats_.error_message, "ERROR");
        return;
    }

    std::filesystem::path target_path = std::filesystem::path(out_dir) / resolved_filename;
    std::filesystem::path part_path = std::filesystem::path(out_dir) / (resolved_filename + ".part");

    // Query Content-Length
    DWORD content_length = 0;
    DWORD cl_len = sizeof(content_length);
    uint64_t total_expected = 0;
    bool has_content_length = false;

    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &content_length, &cl_len, NULL)) {
        has_content_length = true;
        if (http_code == 206) {
            total_expected = resume_offset + content_length;
        } else {
            total_expected = content_length;
            resume_offset = 0; // Server restarted from beginning
        }
        log("Ukuran file: " + Utils::format_bytes(total_expected), "INFO");
    } else {
        log("Server tidak mengirim Content-Length (ukuran dinamis)", "WARNING");
    }

    // Setup file stream
    std::ios_base::openmode mode = std::ios::binary | std::ios::out;
    if (resume_offset > 0 && http_code == 206) {
        mode |= std::ios::app;
    } else {
        mode |= std::ios::trunc;
        resume_offset = 0;
    }

    std::ofstream outfile(part_path, mode);
    if (!outfile.is_open()) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Failed;
        stats_.error_message = "Gagal membuka file penyimpanan untuk penulisan.";
        is_running_.store(false);
        log(stats_.error_message, "ERROR");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.resolved_filename = resolved_filename;
        stats_.target_filepath = target_path.string();
        stats_.part_filepath = part_path.string();
        stats_.total_bytes = total_expected;
        stats_.downloaded_bytes = resume_offset;
        stats_.status = DownloadStatus::Downloading;
    }

    log("Memulai pengunduhan file ke: " + part_path.string(), "INFO");

    // Buffer for reading data
    constexpr DWORD BUFFER_SIZE = 65536; // 64 KB
    std::vector<char> buffer(BUFFER_SIZE);

    DWORD bytes_read = 0;
    auto last_time = std::chrono::steady_clock::now();
    uint64_t last_bytes = resume_offset;
    double smoothed_speed = 0.0;

    while (InternetReadFile(hUrl, buffer.data(), BUFFER_SIZE, &bytes_read) && bytes_read > 0) {
        // Check for cancellation
        if (cancel_requested_.load()) {
            outfile.close();
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            // Optionally remove partial file
            std::error_code ec;
            std::filesystem::remove(part_path, ec);

            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.status = DownloadStatus::Cancelled;
            is_running_.store(false);
            log("Unduhan berhasil dibatalkan dan file sementara dibersihkan.", "WARNING");
            return;
        }

        // Check for pause
        if (pause_requested_.load()) {
            outfile.flush();
            outfile.close();
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);

            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.status = DownloadStatus::Paused;
            stats_.speed_bps = 0.0;
            stats_.eta_seconds = -1;
            is_running_.store(false);
            log("Unduhan berhasil dijeda pada " + Utils::format_bytes(stats_.downloaded_bytes), "INFO");
            return;
        }

        outfile.write(buffer.data(), bytes_read);
        if (!outfile.good()) {
            outfile.close();
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.status = DownloadStatus::Failed;
            stats_.error_message = "Kesalahan I/O disk saat menulis file!";
            is_running_.store(false);
            log(stats_.error_message, "ERROR");
            return;
        }

        // Update stats
        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = current_time - last_time;

        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.downloaded_bytes += bytes_read;

            if (elapsed.count() >= 0.25) { // update metrics every 250ms
                uint64_t delta_bytes = stats_.downloaded_bytes - last_bytes;
                double instant_speed = static_cast<double>(delta_bytes) / elapsed.count();
                if (smoothed_speed <= 0.0) {
                    smoothed_speed = instant_speed;
                } else {
                    smoothed_speed = smoothed_speed * 0.7 + instant_speed * 0.3;
                }

                stats_.speed_bps = smoothed_speed;

                if (stats_.total_bytes > 0 && stats_.speed_bps > 0) {
                    uint64_t remaining_bytes = (stats_.total_bytes > stats_.downloaded_bytes) ? 
                                               (stats_.total_bytes - stats_.downloaded_bytes) : 0;
                    stats_.eta_seconds = static_cast<int>(remaining_bytes / stats_.speed_bps);
                } else {
                    stats_.eta_seconds = -1;
                }

                last_time = current_time;
                last_bytes = stats_.downloaded_bytes;
            }

            if (stats_.total_bytes > 0) {
                stats_.progress = std::clamp(static_cast<float>(stats_.downloaded_bytes) / stats_.total_bytes, 0.0f, 1.0f);
            }
        }

        if (on_progress_) {
            on_progress_(get_stats());
        }
    }

    outfile.flush();
    outfile.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    // Finalize download
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        // If content length was unknown, set total to downloaded
        if (stats_.total_bytes == 0) {
            stats_.total_bytes = stats_.downloaded_bytes;
        }
        stats_.progress = 1.0f;
        stats_.speed_bps = 0.0;
        stats_.eta_seconds = 0;
        stats_.finish_time = std::chrono::system_clock::now();
    }

    // Rename .part to target filename
    std::error_code ec;
    if (std::filesystem::exists(target_path, ec)) {
        std::filesystem::remove(target_path, ec);
    }
    std::filesystem::rename(part_path, target_path, ec);
    if (ec) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Failed;
        stats_.error_message = "Gagal mengubah nama file sementara: " + ec.message();
        is_running_.store(false);
        log(stats_.error_message, "ERROR");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.status = DownloadStatus::Completed;
    }
    is_running_.store(false);

    log("🎉 Unduhan selesai! File disimpan ke: " + target_path.string(), "SUCCESS");

    if (on_complete_) {
        on_complete_(get_stats());
    }
}
