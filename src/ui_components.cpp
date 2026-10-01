#include "ui_components.hpp"
#include "utils.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

#include <chrono>
#include <format>
#include <thread>

using namespace ftxui;

DownloaderApp::DownloaderApp() {
    url_input_ = "https://speed.cloudflare.com/__down?bytes=10485760"; // 10MB test file default
    filename_input_ = "";
    dir_input_ = "./downloads";

    tab_titles_ = {
        " 📥 1. Unduh File ",
        " 📋 2. Riwayat & Log ",
        " 🚀 3. Preset Tes ",
        " 💡 4. Bantuan "
    };

    init_presets();

    downloader_.set_on_progress([this](const DownloadStats&) {
        if (screen_) {
            screen_->PostEvent(Event::Custom);
        }
    });

    downloader_.set_on_log([this](const std::string& msg, const std::string& level) {
        add_log(msg, level);
        if (screen_) {
            screen_->PostEvent(Event::Custom);
        }
    });

    downloader_.set_on_complete([this](const DownloadStats& stats) {
        HistoryItem item;
        item.timestamp = Utils::current_timestamp();
        item.filename = stats.resolved_filename;
        item.filepath = stats.target_filepath;
        item.url = stats.url;
        item.total_bytes = stats.total_bytes;
        item.status = (stats.status == DownloadStatus::Completed) ? "SELESAI" : "GAGAL";

        auto dur = std::chrono::duration<double>(stats.finish_time - stats.start_time);
        item.elapsed_seconds = dur.count();

        history_.add_item(item);

        if (screen_) {
            screen_->PostEvent(Event::Custom);
        }
    });

    add_log("Aplikasi File Downloader siap digunakan.", "SUCCESS");
    add_log("Gunakan Tab/Shift+Tab untuk navigasi, atau tombol 1-4 untuk pindah menu.", "INFO");
}

DownloaderApp::~DownloaderApp() {
    app_running_.store(false);
}

void DownloaderApp::init_presets() {
    presets_ = {
        {
            "Cloudflare 1 MB (Tes Cepat)",
            "File tes berukuran 1 Megabyte, cocok untuk pengujian kilat koneksi.",
            "https://speed.cloudflare.com/__down?bytes=1048576",
            "1.00 MB"
        },
        {
            "Cloudflare 10 MB (Tes Sedang)",
            "File tes 10 Megabyte untuk menguji stabilitas kecepatan unduh.",
            "https://speed.cloudflare.com/__down?bytes=10485760",
            "10.00 MB"
        },
        {
            "Cloudflare 25 MB (Tes Besar)",
            "File tes 25 Megabyte untuk pengujian kapasitas dan fitur jeda/lanjut.",
            "https://speed.cloudflare.com/__down?bytes=26214400",
            "25.00 MB"
        },
        {
            "FTXUI Readme.md (File Teks)",
            "File Markdown dokumentasi resmi FTXUI dari repositori GitHub.",
            "https://raw.githubusercontent.com/ArthurSonzogni/FTXUI/master/README.md",
            "~23 KB"
        },
        {
            "SQLite 3 Amalgamation Zip",
            "Source code SQLite C library versi terbaru dalam format .zip resmi.",
            "https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip",
            "~2.6 MB"
        },
        {
            "Google Favicon (File Gambar Kecil)",
            "Ikon favicon Google resmi format ICO berukuran sangat kecil.",
            "https://www.google.com/favicon.ico",
            "~5 KB"
        }
    };

    preset_titles_.clear();
    for (const auto& p : presets_) {
        preset_titles_.push_back(p.title + " [" + p.size_hint + "]");
    }
}

void DownloaderApp::add_log(const std::string& msg, const std::string& level) {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    LogMessage entry;
    entry.timestamp = Utils::current_timestamp();
    entry.level = level;
    entry.text = msg;
    logs_.push_back(entry);
    if (logs_.size() > 200) {
        logs_.erase(logs_.begin());
    }
}

void DownloaderApp::start_download() {
    if (url_input_.empty()) {
        add_log("URL tidak boleh kosong!", "WARNING");
        return;
    }
    if (downloader_.is_active()) {
        add_log("Unduhan sedang berjalan. Harap jeda atau batalkan unduhan aktif terlebih dahulu.", "WARNING");
        return;
    }

    add_log("Memulai proses unduh: " + url_input_, "INFO");
    downloader_.start(url_input_, dir_input_, filename_input_);
}

void DownloaderApp::toggle_pause_resume() {
    if (downloader_.is_active()) {
        downloader_.pause();
    } else if (downloader_.is_paused()) {
        downloader_.resume();
    } else {
        add_log("Tidak ada unduhan yang sedang berjalan atau dijeda.", "INFO");
    }
}

void DownloaderApp::cancel_download() {
    auto stats = downloader_.get_stats();
    if (downloader_.is_active() || downloader_.is_paused()) {
        downloader_.cancel();

        HistoryItem item;
        item.timestamp = Utils::current_timestamp();
        item.filename = stats.resolved_filename.empty() ? Utils::extract_filename_from_url(stats.url) : stats.resolved_filename;
        item.filepath = stats.target_filepath;
        item.url = stats.url;
        item.total_bytes = stats.downloaded_bytes;
        item.status = "DIBATALKAN";
        item.elapsed_seconds = 0.0;
        history_.add_item(item);
    } else {
        add_log("Tidak ada unduhan aktif untuk dibatalkan.", "INFO");
    }
}

void DownloaderApp::clear_inputs() {
    url_input_.clear();
    filename_input_.clear();
    add_log("Form input telah dibersihkan.", "INFO");
}

void DownloaderApp::open_download_folder() {
    std::string target_dir = dir_input_.empty() ? "./downloads" : dir_input_;
    if (Utils::open_directory_in_explorer(target_dir)) {
        add_log("Membuka folder unduhan: " + target_dir, "INFO");
    } else {
        add_log("Gagal membuka folder di File Explorer.", "ERROR");
    }
}

void DownloaderApp::load_preset(int index) {
    if (index >= 0 && index < static_cast<int>(presets_.size())) {
        url_input_ = presets_[index].url;
        filename_input_.clear();
        tab_selected_ = 0; // Pindah langsung ke tab download
        add_log("Preset dimuat: " + presets_[index].title, "SUCCESS");
    }
}

void DownloaderApp::run() {
    screen_ = std::make_unique<ScreenInteractive>(ScreenInteractive::Fullscreen());

    // Background animation ticker
    std::jthread anim_thread([this]() {
        while (app_running_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            anim_counter_++;
            if (downloader_.is_active() && screen_) {
                screen_->PostEvent(Event::Custom);
            }
        }
    });

    // Components for Tab 0: Download
    auto input_url = Input(&url_input_, "https://example.com/file.zip");
    auto input_fname = Input(&filename_input_, "(Biarkan kosong untuk deteksi otomatis)");
    auto input_dir = Input(&dir_input_, "./downloads");

    auto btn_start = Button(" 🚀 Mulai Unduh ", [this] { start_download(); }, ButtonOption::Animated());
    auto btn_pause = Button(" ⏸️ Jeda / Lanjut ", [this] { toggle_pause_resume(); }, ButtonOption::Animated());
    auto btn_cancel = Button(" ⏹️ Batalkan ", [this] { cancel_download(); }, ButtonOption::Animated());
    auto btn_clear = Button(" 🧹 Bersihkan ", [this] { clear_inputs(); }, ButtonOption::Animated());
    auto btn_folder = Button(" 📂 Buka Folder ", [this] { open_download_folder(); }, ButtonOption::Animated());

    auto container_inputs = Container::Vertical({
        input_url,
        input_fname,
        input_dir,
    });

    auto container_buttons = Container::Horizontal({
        btn_start,
        btn_pause,
        btn_cancel,
        btn_clear,
        btn_folder,
    });

    auto tab_download_comp = Container::Vertical({
        container_inputs,
        container_buttons,
    });

    // Components for Tab 1: History
    auto btn_clear_history = Button(" 🗑️ Hapus Riwayat ", [this] {
        history_.clear();
        add_log("Riwayat unduhan telah dibersihkan.", "INFO");
    }, ButtonOption::Animated());

    auto tab_history_comp = Container::Vertical({
        btn_clear_history,
    });

    // Components for Tab 2: Presets
    auto menu_presets = Menu(&preset_titles_, &preset_selected_);
    auto btn_use_preset = Button(" 📥 Gunakan Preset Ini ", [this] {
        load_preset(preset_selected_);
    }, ButtonOption::Animated());

    auto tab_presets_comp = Container::Vertical({
        menu_presets,
        btn_use_preset,
    });

    // Components for Tab 3: Help
    auto tab_help_comp = Container::Vertical({});

    // Tab bar toggle
    auto tab_toggle = Toggle(&tab_titles_, &tab_selected_);

    // Tab container
    auto tab_container = Container::Tab({
        tab_download_comp,
        tab_history_comp,
        tab_presets_comp,
        tab_help_comp,
    }, &tab_selected_);

    // Main layout container
    auto main_container = Container::Vertical({
        tab_toggle,
        tab_container,
    });

    // Global keyboard shortcut handler
    auto global_handler = CatchEvent(main_container, [this](Event event) {
        if (event == Event::Character('1')) {
            tab_selected_ = 0;
            return true;
        }
        if (event == Event::Character('2')) {
            tab_selected_ = 1;
            return true;
        }
        if (event == Event::Character('3')) {
            tab_selected_ = 2;
            return true;
        }
        if (event == Event::Character('4')) {
            tab_selected_ = 3;
            return true;
        }
        if (event == Event::Character('q') || event == Event::Character('Q')) {
            if (!downloader_.is_active()) {
                screen_->ExitLoopClosure()();
                return true;
            }
        }
        if (event == Event::Escape) {
            screen_->ExitLoopClosure()();
            return true;
        }
        return false;
    });

    // Main renderer
    auto renderer = Renderer(global_handler, [this, &tab_toggle, &tab_container, 
                                             &input_url, &input_fname, &input_dir, 
                                             &btn_start, &btn_pause, &btn_cancel, &btn_clear, &btn_folder,
                                             &btn_clear_history, &menu_presets, &btn_use_preset] {
        Element content;
        if (tab_selected_ == 0) {
            // Render Tab 0: Download
            auto stats = downloader_.get_stats();

            // Status Badge
            Element status_badge;
            switch (stats.status) {
                case DownloadStatus::Idle:
                    status_badge = text(" ⚪ SIAP (IDLE) ") | bold | bgcolor(Color::GrayDark) | color(Color::White);
                    break;
                case DownloadStatus::Connecting:
                    status_badge = text(" 🔄 MENGHUBUNGKAN... ") | bold | bgcolor(Color::Yellow) | color(Color::Black);
                    break;
                case DownloadStatus::Downloading: {
                    std::string spin_char = spinner(18, anim_counter_)->Render().ToString();
                    status_badge = text(" ⬇️ " + spin_char + " MENGUNDUH ") | bold | bgcolor(Color::Cyan) | color(Color::Black);
                    break;
                }
                case DownloadStatus::Paused:
                    status_badge = text(" ⏸️ DIJEDA (PAUSED) ") | bold | bgcolor(Color::Yellow) | color(Color::Black);
                    break;
                case DownloadStatus::Completed:
                    status_badge = text(" ✅ SELESAI (COMPLETED) ") | bold | bgcolor(Color::Green) | color(Color::Black);
                    break;
                case DownloadStatus::Failed:
                    status_badge = text(" ❌ GAGAL (FAILED) ") | bold | bgcolor(Color::Red) | color(Color::White);
                    break;
                case DownloadStatus::Cancelled:
                    status_badge = text(" ⏹️ DIBATALKAN ") | bold | bgcolor(Color::Magenta) | color(Color::White);
                    break;
            }

            // Progress Bar
            float progress_ratio = stats.progress;
            int pct = static_cast<int>(progress_ratio * 100.0f);
            Color bar_color = (stats.status == DownloadStatus::Completed) ? Color::Green :
                              (stats.status == DownloadStatus::Failed) ? Color::Red :
                              (stats.status == DownloadStatus::Paused) ? Color::Yellow : Color::Cyan;

            Element progress_element = hbox({
                gauge(progress_ratio) | color(bar_color) | flex,
                text(" " + std::to_string(pct) + "% ") | bold | color(bar_color),
            });

            // Metrics Grid
            Element metrics_box = hbox({
                vbox({
                    text("Ukuran Total:") | dim,
                    text(stats.total_bytes > 0 ? Utils::format_bytes(stats.total_bytes) : "Menunggu...") | bold | color(Color::White),
                }) | flex,
                separator(),
                vbox({
                    text("Terunduh:") | dim,
                    text(Utils::format_bytes(stats.downloaded_bytes)) | bold | color(Color::Cyan),
                }) | flex,
                separator(),
                vbox({
                    text("Kecepatan:") | dim,
                    text(Utils::format_speed(stats.speed_bps)) | bold | color(Color::Yellow),
                }) | flex,
                separator(),
                vbox({
                    text("Perkiraan Selesai (ETA):") | dim,
                    text(stats.eta_seconds >= 0 ? Utils::format_duration(stats.eta_seconds) : "--:--") | bold | color(Color::Magenta),
                }) | flex,
            }) | borderLight;

            // Target file info
            Element file_info_box = vbox({
                hbox({
                    text("File Target : ") | bold | color(Color::Yellow),
                    text(stats.resolved_filename.empty() ? "(Belum ditentukan)" : stats.resolved_filename) | color(Color::White),
                }),
                hbox({
                    text("Lokasi Simpan: ") | bold | color(Color::Yellow),
                    text(stats.target_filepath.empty() ? (dir_input_ + "/...") : stats.target_filepath) | color(Color::White),
                }),
                (stats.status == DownloadStatus::Failed && !stats.error_message.empty()) ?
                    hbox({ text("Pesan Error  : ") | bold | color(Color::Red), text(stats.error_message) | color(Color::RedLight) }) :
                    nothing(),
            });

            // Mini Log Box (last 5 messages)
            Elements mini_logs;
            {
                std::lock_guard<std::mutex> lock(logs_mutex_);
                size_t start_idx = (logs_.size() > 5) ? (logs_.size() - 5) : 0;
                for (size_t i = start_idx; i < logs_.size(); ++i) {
                    const auto& l = logs_[i];
                    Color col = (l.level == "SUCCESS") ? Color::Green :
                                (l.level == "ERROR") ? Color::Red :
                                (l.level == "WARNING") ? Color::Yellow : Color::GrayLight;
                    mini_logs.push_back(hbox({
                        text("[" + l.timestamp + "] ") | dim,
                        text("[" + l.level + "] ") | bold | color(col),
                        text(l.text),
                    }));
                }
            }
            if (mini_logs.empty()) {
                mini_logs.push_back(text("Belum ada aktivitas log.") | dim);
            }

            content = vbox({
                // Input form
                vbox({
                    hbox({ text(" 🌐 URL File        : ") | bold, input_url->Render() | flex }) | borderLight,
                    hbox({ text(" 📝 Nama File (Opt) : ") | bold, input_fname->Render() | flex }) | borderLight,
                    hbox({ text(" 📁 Folder Tujuan   : ") | bold, input_dir->Render() | flex }) | borderLight,
                }),
                // Action Buttons
                hbox({
                    btn_start->Render(),
                    separatorLight(),
                    btn_pause->Render(),
                    separatorLight(),
                    btn_cancel->Render(),
                    separatorLight(),
                    btn_clear->Render(),
                    separatorLight(),
                    btn_folder->Render(),
                }) | center,
                separator(),
                // Monitor Panel
                vbox({
                    hbox({
                        text(" Status Unduhan: ") | bold,
                        status_badge,
                        filler(),
                        downloader_.is_active() ? text("Sedang Berjalan...") | dim : text("Idle") | dim,
                    }),
                    separatorLight(),
                    progress_element,
                    metrics_box,
                    file_info_box,
                }) | borderRounded,
                // Mini activity logs
                vbox({
                    text(" 📜 Log Aktivitas Terbaru:") | bold | color(Color::Yellow),
                    vbox(std::move(mini_logs)),
                }) | borderLight | flex,
            });

        } else if (tab_selected_ == 1) {
            // Render Tab 1: History & Log
            auto items = history_.get_items();
            std::vector<std::vector<std::string>> table_data;
            table_data.push_back({"Waktu", "Status", "Ukuran", "Nama File", "Durasi", "URL"});

            for (const auto& it : items) {
                table_data.push_back({
                    it.timestamp,
                    it.status,
                    Utils::format_bytes(it.total_bytes),
                    it.filename,
                    Utils::format_duration(static_cast<int>(it.elapsed_seconds)),
                    it.url.size() > 40 ? (it.url.substr(0, 37) + "...") : it.url
                });
            }

            Element history_table_elem;
            if (items.empty()) {
                history_table_elem = text(" Belum ada riwayat unduhan pada sesi ini. ") | dim | center;
            } else {
                auto t = Table(table_data);
                t.SelectRow(0).Decorate(bold);
                t.SelectRow(0).DecorateCells(color(Color::Yellow));
                t.SelectColumns(1, 1).DecorateCells(bold);
                history_table_elem = t.Render();
            }

            // All logs view
            Elements full_logs;
            {
                std::lock_guard<std::mutex> lock(logs_mutex_);
                size_t start_idx = (logs_.size() > 15) ? (logs_.size() - 15) : 0;
                for (size_t i = start_idx; i < logs_.size(); ++i) {
                    const auto& l = logs_[i];
                    Color col = (l.level == "SUCCESS") ? Color::Green :
                                (l.level == "ERROR") ? Color::Red :
                                (l.level == "WARNING") ? Color::Yellow : Color::GrayLight;
                    full_logs.push_back(hbox({
                        text("[" + l.timestamp + "] ") | dim,
                        text("[" + l.level + "] ") | bold | color(col),
                        text(l.text),
                    }));
                }
            }

            content = vbox({
                hbox({
                    text(" 📋 Riwayat Unduhan File") | bold | color(Color::Cyan),
                    filler(),
                    btn_clear_history->Render(),
                }),
                separatorLight(),
                history_table_elem | borderRounded | flex,
                separator(),
                text(" 📜 Log Sistem Lengkap (15 Baris Terakhir):") | bold | color(Color::Yellow),
                vbox(std::move(full_logs)) | borderLight | flex,
            });

        } else if (tab_selected_ == 2) {
            // Render Tab 2: Presets
            const auto& cur_preset = presets_[preset_selected_];

            content = vbox({
                text(" 🚀 Pilihan URL Sampel & Tes Cepat") | bold | color(Color::Cyan),
                text(" Pilih salah satu link di bawah untuk menguji kecepatan unduh atau fungsionalitas aplikasi.") | dim,
                separatorLight(),
                hbox({
                    vbox({
                        text(" Daftar Preset:") | bold | color(Color::Yellow),
                        menu_presets->Render() | borderLight | flex,
                    }) | flex,
                    separator(),
                    vbox({
                        text(" Detail Preset Terpilih:") | bold | color(Color::Yellow),
                        separatorLight(),
                        hbox({ text("Judul   : ") | bold, text(cur_preset.title) | color(Color::White) }),
                        hbox({ text("Ukuran  : ") | bold, text(cur_preset.size_hint) | color(Color::Green) }),
                        hbox({ text("URL     : ") | bold, text(cur_preset.url) | color(Color::Cyan) }),
                        separatorLight(),
                        text("Keterangan:") | bold,
                        paragraph(cur_preset.description) | dim,
                        filler(),
                        btn_use_preset->Render() | center,
                    }) | borderRounded | flex,
                }) | flex,
            });

        } else {
            // Render Tab 3: Help
            content = vbox({
                text(" 💡 Bantuan, Petunjuk & Informasi Aplikasi") | bold | color(Color::Cyan),
                separatorLight(),
                vbox({
                    text(" ⌨️  Pintasan Keyboard (Shortcuts):") | bold | color(Color::Yellow),
                    separatorLight(),
                    hbox({ text("  • [Tab] / [Shift+Tab]   : ") | bold, text("Berpindah fokus antar kolom input dan tombol.") }),
                    hbox({ text("  • [Tombol Panah]        : ") | bold, text("Navigasi menu preset atau pilihan.") }),
                    hbox({ text("  • [Enter]               : ") | bold, text("Menekan tombol aktif atau memilih preset.") }),
                    hbox({ text("  • [1], [2], [3], [4]    : ") | bold, text("Pindah tab langsung secara cepat.") }),
                    hbox({ text("  • [q] / [Esc]           : ") | bold, text("Keluar dari aplikasi (hanya saat unduhan tidak aktif).") }),
                    hbox({ text("  • [Ctrl + C]            : ") | bold, text("Hentikan paksa aplikasi secara aman.") }),
                }) | borderRounded,
                separatorLight(),
                vbox({
                    text(" ⚙️  Fitur Utama:") | bold | color(Color::Yellow),
                    separatorLight(),
                    text("  1. Multithreaded Core  : Antarmuka TUI 60 FPS tetap responsif saat mengunduh di latar belakang."),
                    text("  2. Smart Range Request : Mendukung Jeda (Pause) dan Lanjut (Resume) tanpa mengulang dari 0."),
                    text("  3. Windows Native HTTP : Menggunakan WinINet API dengan dukungan HTTPS, redirect, dan proxy."),
                    text("  4. Part File Protection: Mengunduh ke ekstensi .part sementara untuk mencegah file korup."),
                    text("  5. Speed & ETA Smoother: Kalkulasi kecepatan rata-rata bergerak (Moving Average) yang akurat."),
                }) | borderRounded,
                filler(),
            });
        }

        return vbox({
            render_header(),
            tab_toggle->Render() | center,
            separator(),
            content | flex,
            separator(),
            render_footer(),
        }) | border;
    });

    screen_->Loop(renderer);
}

Element DownloaderApp::render_header() {
    auto stats = downloader_.get_stats();
    bool active = downloader_.is_active();

    return hbox({
        text(" ⚡ ") | bold | color(Color::Yellow),
        text("C++ TERMINAL FILE DOWNLOADER") | bold | color(Color::Cyan),
        text(" | Powered by FTXUI ") | dim | color(Color::White),
        filler(),
        text("Status: ") | dim,
        active ? 
            (text("● AKTIF ") | bold | color(Color::Green)) : 
            (downloader_.is_paused() ? (text("⏸ DIJEDA ") | bold | color(Color::Yellow)) : (text("○ IDLE ") | dim | color(Color::GrayLight))),
    });
}

Element DownloaderApp::render_footer() {
    return hbox({
        text(" [1] Unduh  [2] Riwayat  [3] Preset  [4] Bantuan  |  [Tab]: Fokus  [Enter]: Pilih  [q]: Keluar") | dim | color(Color::GrayLight),
        filler(),
        text("sinaucpp • Windows Native ") | dim | color(Color::Cyan),
    });
}
