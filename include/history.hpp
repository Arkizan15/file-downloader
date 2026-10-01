#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

struct HistoryItem {
    std::string timestamp;
    std::string filename;
    std::string filepath;
    std::string url;
    uint64_t total_bytes = 0;
    std::string status; // "SELESAI", "GAGAL", "DIBATALKAN"
    double elapsed_seconds = 0.0;
};

class HistoryManager {
public:
    HistoryManager(const std::string& history_file = "history_downloads.txt");
    ~HistoryManager();

    void add_item(const HistoryItem& item);
    std::vector<HistoryItem> get_items() const;
    void clear();

    void load();
    void save();

private:
    std::string file_path_;
    mutable std::mutex mutex_;
    std::vector<HistoryItem> items_;
};
