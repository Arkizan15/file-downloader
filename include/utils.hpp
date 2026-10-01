#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <windows.h>
#include <shellapi.h>

namespace Utils {

// Format bytes into human readable string (B, KB, MB, GB)
inline std::string format_bytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }

    std::ostringstream oss;
    if (unit_idx == 0) {
        oss << bytes << " " << units[unit_idx];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit_idx];
    }
    return oss.str();
}

// Format download speed (B/s, KB/s, MB/s, GB/s)
inline std::string format_speed(double bytes_per_sec) {
    if (bytes_per_sec <= 0.0) return "0 B/s";
    return format_bytes(static_cast<uint64_t>(bytes_per_sec)) + "/s";
}

// Format duration in seconds (HH:MM:SS or MM:SS)
inline std::string format_duration(int seconds) {
    if (seconds < 0) return "--:--";
    if (seconds >= 3600) {
        int h = seconds / 3600;
        int m = (seconds % 3600) / 60;
        int s = seconds % 60;
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << h << ":"
            << std::setw(2) << std::setfill('0') << m << ":"
            << std::setw(2) << std::setfill('0') << s;
        return oss.str();
    } else {
        int m = seconds / 60;
        int s = seconds % 60;
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << m << ":"
            << std::setw(2) << std::setfill('0') << s;
        return oss.str();
    }
}

// Get current local timestamp as string HH:MM:SS
inline std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return std::string(buf);
}

// Sanitize filename for Windows filesystem
inline std::string sanitize_filename(const std::string& name) {
    std::string safe = name;
    const std::string invalid_chars = "\\/:*?\"<>|";
    for (char& c : safe) {
        if (invalid_chars.find(c) != std::string::npos || static_cast<unsigned char>(c) < 32) {
            c = '_';
        }
    }
    // Trim spaces and periods from ends
    while (!safe.empty() && (safe.front() == ' ' || safe.front() == '.')) {
        safe.erase(0, 1);
    }
    while (!safe.empty() && (safe.back() == ' ' || safe.back() == '.')) {
        safe.pop_back();
    }
    if (safe.empty()) {
        safe = "downloaded_file.bin";
    }
    return safe;
}

// Extract filename from URL (stripping query string and fragment)
inline std::string extract_filename_from_url(const std::string& url) {
    if (url.empty()) return "downloaded_file.bin";

    std::string clean = url;
    // Strip query parameters
    size_t qmark = clean.find('?');
    if (qmark != std::string::npos) {
        clean = clean.substr(0, qmark);
    }
    // Strip fragments
    size_t hash = clean.find('#');
    if (hash != std::string::npos) {
        clean = clean.substr(0, hash);
    }
    // Remove trailing slash
    while (!clean.empty() && clean.back() == '/') {
        clean.pop_back();
    }

    size_t slash = clean.find_last_of("/\\");
    std::string filename;
    if (slash != std::string::npos && slash + 1 < clean.size()) {
        filename = clean.substr(slash + 1);
    }

    if (filename.empty()) {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        filename = "download_" + std::to_string(now) + ".bin";
    }

    return sanitize_filename(filename);
}

// Open folder in Windows File Explorer
inline bool open_directory_in_explorer(const std::string& path) {
    std::filesystem::path p(path);
    if (!std::filesystem::exists(p)) {
        std::filesystem::create_directories(p);
    }
    auto abs_path = std::filesystem::absolute(p).string();
    HINSTANCE result = ShellExecuteA(NULL, "open", abs_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

} // namespace Utils
