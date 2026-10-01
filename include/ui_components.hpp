#pragma once

#include "downloader.hpp"
#include "history.hpp"
#include "utils.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

struct PresetItem {
    std::string title;
    std::string description;
    std::string url;
    std::string size_hint;
};

struct LogMessage {
    std::string timestamp;
    std::string level;
    std::string text;
};

class DownloaderApp {
public:
    DownloaderApp();
    ~DownloaderApp();

    void run();

    void add_log(const std::string& msg, const std::string& level = "INFO");

private:
    void init_presets();
    void start_download();
    void toggle_pause_resume();
    void cancel_download();
    void clear_inputs();
    void open_download_folder();
    void load_preset(int index);

    // Render helpers
    ftxui::Element render_header();
    ftxui::Element render_footer();
    ftxui::Element render_tab_download();
    ftxui::Element render_tab_history();
    ftxui::Element render_tab_presets();
    ftxui::Element render_tab_help();

    Downloader downloader_;
    HistoryManager history_;

    mutable std::mutex logs_mutex_;
    std::vector<LogMessage> logs_;

    std::string url_input_;
    std::string filename_input_;
    std::string dir_input_;

    int tab_selected_ = 0;
    int preset_selected_ = 0;
    int history_selected_ = 0;
    size_t anim_counter_ = 0;

    std::vector<std::string> tab_titles_;
    std::vector<PresetItem> presets_;
    std::vector<std::string> preset_titles_;

    std::atomic<bool> app_running_{true};
    std::unique_ptr<ftxui::ScreenInteractive> screen_;
};
