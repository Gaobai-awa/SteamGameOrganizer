#pragma once
#include "steam_scanner.h"
#include "category_manager.h"
#include "playtime_tracker.h"
#include "game_launcher.h"
#include <string>
#include <functional>

// ============================================================
// 控制台交互界面
// 提供菜单驱动的命令行交互
// ============================================================
class ConsoleUI {
public:
    ConsoleUI();
    
    // 初始化
    bool init();

    // 主循环
    void run();

private:
    // --- 数据显示 ---
    void show_header();
    void show_game_list(const std::vector<GameInfo*>& games);
    void show_game_detail(const GameInfo& game);
    void show_categories();
    void show_category_games(const std::string& cat_name);
    void show_playtime_stats();
    void show_help();

    // --- 格式化辅助 ---
    static std::string format_size(uint64_t bytes);
    static std::string format_time(int64_t unix_timestamp);
    static std::string format_duration(int64_t seconds);
    static std::string truncate(const std::string& s, size_t max_len);

    // --- 用户交互 ---
    int  read_number(int min, int max);
    std::string read_line(const std::string& prompt);
    bool read_confirm(const std::string& prompt);

    // --- 分页 ---
    void paginate_games(const std::vector<GameInfo*>& games, 
                        std::function<void(int)> on_select);

    // --- 核心操作 ---
    void launch_game(const GameInfo& game);
    void show_game_menu(const GameInfo& game);
    void manage_categories_menu();
    void add_game_to_categories(const GameInfo& game);

    // --- 数据 ---
    SteamScanner m_scanner;
    CategoryManager m_categories;
    PlaytimeTracker m_playtime;
    std::string m_steam_path;
    std::string m_data_dir;

    // --- 常量 ---
    static constexpr int PAGE_SIZE = 20;
};
