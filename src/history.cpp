#include "history.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>

HistoryManager::HistoryManager(const std::string& history_file)
    : file_path_(history_file) {
    load();
}

HistoryManager::~HistoryManager() {
    save();
}

void HistoryManager::add_item(const HistoryItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Insert at front so newest is at the top
    items_.insert(items_.begin(), item);
    // Keep maximum 100 items
    if (items_.size() > 100) {
        items_.pop_back();
    }
    save();
}

std::vector<HistoryItem> HistoryManager::get_items() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_;
}

void HistoryManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();
    std::error_code ec;
    std::filesystem::remove(file_path_, ec);
}

void HistoryManager::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    items_.clear();

    std::ifstream file(file_path_);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string ts, status, size_str, fname, fpath, url, elapsed_str;

        if (std::getline(ss, ts, '\t') &&
            std::getline(ss, status, '\t') &&
            std::getline(ss, size_str, '\t') &&
            std::getline(ss, fname, '\t') &&
            std::getline(ss, fpath, '\t') &&
            std::getline(ss, url, '\t') &&
            std::getline(ss, elapsed_str)) {
            
            HistoryItem item;
            item.timestamp = ts;
            item.status = status;
            item.total_bytes = std::strtoull(size_str.c_str(), nullptr, 10);
            item.filename = fname;
            item.filepath = fpath;
            item.url = url;
            item.elapsed_seconds = std::strtod(elapsed_str.c_str(), nullptr);
            items_.push_back(item);
        }
    }
}

void HistoryManager::save() {
    std::ofstream file(file_path_, std::ios::trunc);
    if (!file.is_open()) return;

    for (const auto& item : items_) {
        file << item.timestamp << "\t"
             << item.status << "\t"
             << item.total_bytes << "\t"
             << item.filename << "\t"
             << item.filepath << "\t"
             << item.url << "\t"
             << item.elapsed_seconds << "\n";
    }
}
