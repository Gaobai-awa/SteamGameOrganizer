#include "console_ui.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <conio.h>   // _getch()

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================
// 编码设置: 控制台输出 UTF-8
// ============================================================
static void setup_encoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // 启用 ANSI 转义码 (Windows 10+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

// ============================================================
// ANSI 颜色 (跨平台简单实现)
// ============================================================
namespace Color {
    const char* RESET   = "\033[0m";
    const char* GREEN   = "\033[32m";
    const char* YELLOW  = "\033[33m";
    const char* CYAN    = "\033[36m";
    const char* MAGENTA = "\033[35m";
    const char* RED     = "\033[31m";
    const char* BOLD    = "\033[1m";
    const char* DIM     = "\033[2m";
}

ConsoleUI::ConsoleUI() {}

bool ConsoleUI::init() {
    setup_encoding();

    // 数据目录
    m_data_dir = "data";
    std::error_code ec;
    std::filesystem::create_directories(m_data_dir, ec);

    // 1. 扫描 Steam 游戏
    std::cout << Color::CYAN << "╔══════════════════════════════════════╗\n";
    std::cout << "║  第三方 Steam 游戏启动器 v1.0      ║\n";
    std::cout << "╚══════════════════════════════════════╝" << Color::RESET << "\n\n";
    
    std::cout << "正在扫描 Steam 游戏库...\n";
    m_scanner.scan_all_games();
    
    if (m_scanner.games().empty()) {
        std::cout << Color::RED << "\n[错误] 未找到 Steam 安装或没有已安装的游戏。\n" << Color::RESET;
        std::cout << "请确认:\n";
        std::cout << "  1. Steam 已安装\n";
        std::cout << "  2. 至少有一个游戏已安装\n";
        std::cout << "\n按任意键退出...";
        _getch();
        return false;
    }

    m_steam_path = SteamScanner::find_steam_path();

    // 2. 加载分类数据
    m_categories.load(m_data_dir + "/categories.json");

    // 3. 加载游玩时长数据
    m_playtime.load(m_data_dir + "/playtime.json");

    std::cout << Color::GREEN << "初始化完成! 按任意键进入主菜单..." << Color::RESET;
    _getch();
    return true;
}

// ============================================================
// 格式化工具
// ============================================================
std::string ConsoleUI::format_size(uint64_t bytes) {
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        ++unit_idx;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_idx];
    return oss.str();
}

std::string ConsoleUI::format_time(int64_t unix_timestamp) {
    if (unix_timestamp == 0) return "从未";
    time_t t = static_cast<time_t>(unix_timestamp);
    char buf[64];
    struct tm* tm_info = localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
    return std::string(buf);
}

std::string ConsoleUI::format_duration(int64_t seconds) {
    if (seconds < 60) return std::to_string(seconds) + " 秒";
    
    int64_t hours = seconds / 3600;
    int64_t mins = (seconds % 3600) / 60;
    
    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << " 小时 ";
        if (mins > 0) oss << mins << " 分钟";
    } else {
        oss << mins << " 分钟";
    }
    return oss.str();
}

// UTF-8 安全截断: 按字符数而非字节数截断
std::string ConsoleUI::truncate(const std::string& s, size_t max_chars) {
    size_t char_count = 0;
    size_t byte_pos = 0;
    while (byte_pos < s.size() && char_count < max_chars) {
        unsigned char c = static_cast<unsigned char>(s[byte_pos]);
        if (c < 0x80)       { byte_pos += 1; }      // 1字节 (ASCII)
        else if (c < 0xE0)  { byte_pos += 2; }      // 2字节
        else if (c < 0xF0)  { byte_pos += 3; }      // 3字节 (中文)
        else                { byte_pos += 4; }      // 4字节
        ++char_count;
    }
    if (byte_pos >= s.size()) return s;
    // 留出 "..." 的空间 (3个ASCII字符 = 3字节)
    size_t suffix_bytes = 3;
    if (byte_pos < suffix_bytes) return s.substr(0, byte_pos) + "...";
    // 从 max_chars - 3 个字符处截断，加上 "..."
    size_t trunc_chars = (max_chars > 3) ? (max_chars - 3) : 1;
    size_t trunc_pos = 0;
    size_t count = 0;
    while (trunc_pos < s.size() && count < trunc_chars) {
        unsigned char c = static_cast<unsigned char>(s[trunc_pos]);
        if (c < 0x80)       { trunc_pos += 1; }
        else if (c < 0xE0)  { trunc_pos += 2; }
        else if (c < 0xF0)  { trunc_pos += 3; }
        else                { trunc_pos += 4; }
        ++count;
    }
    return s.substr(0, trunc_pos) + "...";
}

// ============================================================
// 输入辅助
// ============================================================
int ConsoleUI::read_number(int min, int max) {
    int val;
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        try {
            val = std::stoi(line);
            if (val >= min && val <= max) return val;
        } catch (...) {}
        std::cout << Color::RED << "请输入 " << min << "-" << max << " 之间的数字: " << Color::RESET;
    }
}

std::string ConsoleUI::read_line(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

bool ConsoleUI::read_confirm(const std::string& prompt) {
    std::cout << prompt << " (y/n): ";
    std::string line;
    std::getline(std::cin, line);
    return !line.empty() && (line[0] == 'y' || line[0] == 'Y');
}

// ============================================================
// 显示
// ============================================================
void ConsoleUI::show_header() {
    std::cout << "\033[2J\033[H"; // 清屏
    std::cout << Color::CYAN << Color::BOLD
              << "╔══════════════════════════════════════════════════════╗\n"
              << "║         第三方 Steam 游戏启动器  v1.0              ║\n"
              << "╠══════════════════════════════════════════════════════╣\n"
              << "║  游戏总数: " << std::setw(4) << m_scanner.games().size() 
              << "  |  分类数: " << std::setw(3) << m_categories.categories().size()
              << "  |  会话数: " << std::setw(4) << m_playtime.get_recent_sessions(99999).size()
              << "  ║\n"
              << "╚══════════════════════════════════════════════════════╝" 
              << Color::RESET << "\n";
}

void ConsoleUI::show_game_list(const std::vector<GameInfo*>& games) {
    std::cout << Color::BOLD << "\n"
              << std::setw(5)  << std::left << "编号"
              << std::setw(35) << std::left << "游戏名称"
              << std::setw(12) << std::left << "大小"
              << std::setw(12) << std::left << "Steam时长"
              << std::setw(12) << std::left << "本程序时长"
              << std::setw(20) << std::left << "最后启动"
              << std::setw(25) << std::left << "分类"
              << Color::RESET << "\n";
    std::cout << std::string(120, '-') << "\n";

    int idx = 1;
    for (auto* g : games) {
        // 游戏名截断
        std::string name = truncate(g->name, 33);
        
        // Steam 游玩时长
        std::string steam_time = g->total_playtime_min > 0 
            ? format_duration(g->total_playtime_min * 60) 
            : "-";
        
        // 本程序追踪的时长
        int64_t local_time = m_playtime.get_total_playtime(g->appid);
        std::string local_time_str = local_time > 0 
            ? format_duration(local_time) 
            : "-";

        // 最后启动时间
        std::string last = g->last_played > 0 
            ? format_time(g->last_played) 
            : format_time(0);

        // 分类标签
        auto cats = m_categories.get_categories_for_game(g->appid);
        std::string cat_str;
        for (size_t i = 0; i < cats.size() && i < 3; ++i) {
            if (i > 0) cat_str += ", ";
            cat_str += cats[i];
        }
        if (cats.size() > 3) cat_str += "...";
        if (cats.empty()) cat_str = "-";

        std::cout << Color::YELLOW << std::setw(5)  << std::left << idx
                  << Color::RESET  << std::setw(35) << std::left << name
                  << std::setw(12) << std::left << format_size(g->size_on_disk)
                  << std::setw(12) << std::left << truncate(steam_time, 10)
                  << std::setw(12) << std::left << truncate(local_time_str, 10)
                  << std::setw(20) << std::left << last;

        // 分类用彩色
        if (cats.empty()) {
            std::cout << Color::DIM << cat_str << Color::RESET;
        } else {
            std::cout << Color::MAGENTA << truncate(cat_str, 23) << Color::RESET;
        }
        std::cout << "\n";
        ++idx;
    }
    std::cout << std::string(120, '-') << "\n";
}

void ConsoleUI::show_game_detail(const GameInfo& game) {
    std::cout << "\n" << Color::CYAN << Color::BOLD
              << "═══ " << game.name << " ═══" << Color::RESET << "\n\n";
    std::cout << "  AppID:          " << game.appid << "\n";
    std::cout << "  安装目录:       " << game.install_dir << "\n";
    std::cout << "  库文件夹:       " << game.library_path << "\n";
    std::cout << "  磁盘占用:       " << format_size(game.size_on_disk) << "\n";
    std::cout << "  最后更新:       " << format_time(game.last_updated) << "\n";
    std::cout << "  状态标记:       " << game.state_flags << "\n";
    std::cout << "\n  --- 游玩数据 ---\n";
    std::cout << "  Steam记录时长:  " << game.total_playtime_min << " 分钟 ("
              << format_duration(game.total_playtime_min * 60) << ")\n";
    std::cout << "  Steam最后启动:  " << format_time(game.last_played) << "\n";
    std::cout << "  本程序总时长:   " 
              << format_duration(m_playtime.get_total_playtime(game.appid)) << "\n";

    // 分类
    auto cats = m_categories.get_categories_for_game(game.appid);
    std::cout << "  所属分类:       ";
    if (cats.empty()) {
        std::cout << Color::DIM << "(未分类)" << Color::RESET;
    } else {
        for (size_t i = 0; i < cats.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << Color::MAGENTA << cats[i] << Color::RESET;
        }
    }
    std::cout << "\n";

    // 最近会话记录
    auto sessions = m_playtime.get_sessions_for_game(game.appid);
    if (!sessions.empty()) {
        std::cout << "\n  最近会话:\n";
        int count = 0;
        for (const auto& s : sessions) {
            if (count++ >= 5) break;
            std::cout << "    " << format_time(s.start_time) 
                      << " → 时长: " << format_duration(s.duration_sec) << "\n";
        }
    }
    std::cout << "\n";
}

void ConsoleUI::show_categories() {
    std::cout << Color::CYAN << Color::BOLD << "\n═══ 分类夹管理 ═══" << Color::RESET << "\n\n";
    
    const auto& cats = m_categories.categories();
    if (cats.empty()) {
        std::cout << "  (还没有创建任何分类夹)\n";
        return;
    }

    for (size_t i = 0; i < cats.size(); ++i) {
        std::cout << Color::YELLOW << "  [" << (i + 1) << "] " 
                  << Color::MAGENTA << cats[i].name 
                  << Color::RESET << " (" << cats[i].game_ids.size() << " 个游戏)\n";
    }
}

void ConsoleUI::show_category_games(const std::string& cat_name) {
    const auto* game_ids = m_categories.get_games_in_category(cat_name);
    if (!game_ids || game_ids->empty()) {
        std::cout << "该分类下没有游戏。\n";
        return;
    }

    std::vector<GameInfo*> games;
    for (uint32_t id : *game_ids) {
        auto* g = m_scanner.find_game(id);
        if (g) games.push_back(g);
    }

    std::cout << Color::CYAN << "\n═══ 分类: " << cat_name 
              << " (" << games.size() << " 个游戏) ═══" << Color::RESET << "\n";
    show_game_list(games);
}

void ConsoleUI::show_playtime_stats() {
    std::cout << Color::CYAN << Color::BOLD << "\n═══ 游玩时长统计 ═══" << Color::RESET << "\n\n";

    // 按总时长排序
    struct StatItem {
        uint32_t appid;
        int64_t total;
        std::string name;
        int sessions;
    };
    std::vector<StatItem> stats;

    for (const auto& game : m_scanner.games()) {
        int64_t t = m_playtime.get_total_playtime(game.appid);
        auto sessions = m_playtime.get_sessions_for_game(game.appid);
        if (t > 0) {
            stats.push_back({ game.appid, t, game.name, static_cast<int>(sessions.size()) });
        }
    }

    std::sort(stats.begin(), stats.end(), 
        [](const StatItem& a, const StatItem& b) { return a.total > b.total; });

    if (stats.empty()) {
        std::cout << "  还没有本程序记录的游玩数据。启动游戏后会自动追踪喵～\n";
        return;
    }

    std::cout << std::setw(5)  << std::left << "排名"
              << std::setw(35) << std::left << "游戏"
              << std::setw(15) << std::left << "总时长"
              << std::setw(10) << std::left << "会话数"
              << "\n" << std::string(65, '-') << "\n";

    for (size_t i = 0; i < stats.size() && i < 20; ++i) {
        const auto& s = stats[i];
        std::cout << Color::YELLOW << std::setw(5)  << std::left << (i + 1)
                  << Color::RESET  << std::setw(35) << std::left << truncate(s.name, 33)
                  << std::setw(15) << std::left << format_duration(s.total)
                  << std::setw(10) << std::left << s.sessions
                  << "\n";
    }
}

void ConsoleUI::show_help() {
    std::cout << Color::CYAN << Color::BOLD << "\n═══ 帮助 ═══" << Color::RESET << "\n\n";
    std::cout << "  Steam 协议启动:\n";
    std::cout << "    使用 steam://rungameid/<AppID> URL 协议启动游戏。\n";
    std::cout << "    这需要 Steam 客户端正在运行 (或会自动启动 Steam)。\n\n";
    std::cout << "  游戏信息获取:\n";
    std::cout << "    • appmanifest_*.acf → 游戏名称、大小、安装目录\n";
    std::cout << "    • localconfig.vdf   → Steam 记录的游玩时长、最后启动时间\n";
    std::cout << "    • 本程序自主追踪   → 实际游玩时长记录\n\n";
    std::cout << "  分类系统:\n";
    std::cout << "    支持多对多分类，一个游戏可以属于多个分类夹。\n";
    std::cout << "    分类数据保存在 data/categories.json。\n\n";
    std::cout << "  编码注意:\n";
    std::cout << "    • VDF/ACF 文件: UTF-8\n";
    std::cout << "    • 控制台输出: UTF-8 (SetConsoleOutputCP)\n";
    std::cout << "    • 文件读写: 使用 Windows API CreateFileW 支持中文路径\n";
    std::cout << "    • JSON 存储: UTF-8 不带 BOM\n";
}

// ============================================================
// 分页显示
// ============================================================
void ConsoleUI::paginate_games(const std::vector<GameInfo*>& games, 
                               std::function<void(int)> on_select) {
    int total = static_cast<int>(games.size());
    int current_page = 0;
    int total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;

    while (true) {
        show_header();

        int start = current_page * PAGE_SIZE;
        int end   = std::min(start + PAGE_SIZE, total);

        std::vector<GameInfo*> page_games(games.begin() + start, games.begin() + end);
        show_game_list(page_games);

        std::cout << Color::DIM << "\n  第 " << (current_page + 1) << "/" << total_pages << " 页"
                  << Color::RESET;
        std::cout << Color::YELLOW << "\n  [N]下一页  [P]上一页  [1-" << (end - start) 
                  << "]选择游戏  [L]启动  [C]分类管理  [T]时长统计  [R]刷新  [H]帮助  [Q]退出"
                  << Color::RESET << "\n\n> ";

        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "q" || cmd == "Q") return;
        if (cmd == "n" || cmd == "N") { 
            if (current_page < total_pages - 1) ++current_page; 
            continue; 
        }
        if (cmd == "p" || cmd == "P") { 
            if (current_page > 0) --current_page; 
            continue; 
        }
        if (cmd == "c" || cmd == "C") { manage_categories_menu(); continue; }
        if (cmd == "t" || cmd == "T") { show_playtime_stats(); std::cout << "\n按任意键返回..."; _getch(); continue; }
        if (cmd == "r" || cmd == "R") {
            std::cout << "正在刷新...\n";
            m_scanner.refresh();
            std::cout << "刷新完成! 按任意键继续...";
            _getch();
            continue;
        }
        if (cmd == "h" || cmd == "H") { show_help(); std::cout << "\n按任意键返回..."; _getch(); continue; }

        // 尝试解析为数字
        try {
            int idx = std::stoi(cmd);
            if (idx >= 1 && idx <= (end - start)) {
                int global_idx = start + idx - 1;
                if (cmd.size() > 0 && (cmd[0] == 'l' || cmd[0] == 'L')) {
                    // L+数字 = 直接启动
                    // (继续解析)
                }
                show_game_menu(*games[global_idx]);
                continue;
            }
        } catch (...) {}

        // 检查是否是 L+数字 格式 (快速启动)
        if (cmd.size() > 1 && (cmd[0] == 'l' || cmd[0] == 'L')) {
            try {
                int idx = std::stoi(cmd.substr(1));
                if (idx >= 1 && idx <= (end - start)) {
                    int global_idx = start + idx - 1;
                    launch_game(*games[global_idx]);
                    std::cout << "\n按任意键返回...";
                    _getch();
                    continue;
                }
            } catch (...) {}
        }
    }
}

// ============================================================
// 游戏菜单
// ============================================================
void ConsoleUI::show_game_menu(const GameInfo& game) {
    while (true) {
        show_header();
        show_game_detail(game);

        std::cout << Color::YELLOW 
                  << "  [1] 启动游戏\n"
                  << "  [2] 加入分类夹\n"
                  << "  [3] 从分类夹移除\n"
                  << "  [4] 查看分类\n"
                  << "  [5] 手动添加游玩时长\n"
                  << "  [B] 返回列表\n"
                  << Color::RESET << "\n> ";

        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "b" || cmd == "B") return;
        if (cmd == "1") { launch_game(game); }
        if (cmd == "2") { add_game_to_categories(game); }
        if (cmd == "3") {
            auto cats = m_categories.get_categories_for_game(game.appid);
            if (cats.empty()) {
                std::cout << "该游戏不属于任何分类。\n按任意键继续...";
                _getch();
                continue;
            }
            for (size_t i = 0; i < cats.size(); ++i) {
                std::cout << "  [" << (i + 1) << "] " << cats[i] << "\n";
            }
            int sel = read_number(1, static_cast<int>(cats.size()));
            m_categories.remove_game_from_category(game.appid, cats[sel - 1]);
            m_categories.save("");
            std::cout << Color::GREEN << "已从分类 '" << cats[sel - 1] << "' 中移除。\n" << Color::RESET;
            _getch();
        }
        if (cmd == "4") {
            auto cats = m_categories.get_categories_for_game(game.appid);
            std::cout << "所属分类: ";
            for (const auto& c : cats) std::cout << Color::MAGENTA << c << " " << Color::RESET;
            std::cout << "\n按任意键...";
            _getch();
        }
        if (cmd == "5") {
            std::cout << "输入游玩时长 (分钟): ";
            int mins = read_number(0, 999999);
            m_playtime.add_manual_session(game.appid, 
                PlaytimeTracker::now() - mins * 60, 
                PlaytimeTracker::now());
            m_playtime.save("");
            std::cout << Color::GREEN << "已添加 " << mins << " 分钟游玩记录。\n" << Color::RESET;
            _getch();
        }
    }
}

// ============================================================
// 启动游戏
// ============================================================
void ConsoleUI::launch_game(const GameInfo& game) {
    std::cout << "\n" << Color::GREEN << "正在启动 " << game.name << "..." << Color::RESET << "\n";
    
    // 开始追踪游玩时长
    int session_idx = m_playtime.start_session(game.appid);
    
    bool success = GameLauncher::smart_launch(game, m_steam_path);
    
    if (success) {
        std::cout << Color::GREEN << "启动命令已发送!" << Color::RESET << "\n";
        std::cout << Color::DIM << "(Steam 客户端可能需要几秒钟来处理启动请求)\n" << Color::RESET;
        
        // 询问是否结束会话
        std::cout << "\n游戏启动后，按任意键来结束本次游玩时长记录...";
        _getch();
        
        // 结束会话
        m_playtime.end_session(session_idx);
        m_playtime.save("");
        std::cout << Color::GREEN << "游玩时长已记录。" << Color::RESET << "\n";
    } else {
        std::cerr << Color::RED << "启动失败!" << Color::RESET << "\n";
        // 移除未完成的会话
        m_playtime.end_session(session_idx, 0);
    }
}

// ============================================================
// 分类管理
// ============================================================
void ConsoleUI::manage_categories_menu() {
    while (true) {
        show_header();
        show_categories();

        std::cout << Color::YELLOW << "\n  [1] 创建分类  [2] 删除分类  [3] 重命名分类  [4] 查看分类下游戏  [B] 返回"
                  << Color::RESET << "\n> ";

        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "b" || cmd == "B") return;

        if (cmd == "1") {
            std::string name = read_line("分类名称: ");
            if (!name.empty()) {
                if (m_categories.create_category(name)) {
                    m_categories.save("");
                    std::cout << Color::GREEN << "创建成功!\n" << Color::RESET;
                } else {
                    std::cout << Color::RED << "分类已存在!\n" << Color::RESET;
                }
                _getch();
            }
        }

        if (cmd == "2") {
            const auto& cats = m_categories.categories();
            if (cats.empty()) { _getch(); continue; }
            show_categories();
            std::cout << "选择要删除的分类编号: ";
            int sel = read_number(1, static_cast<int>(cats.size()));
            if (read_confirm("确认删除 '" + cats[sel - 1].name + "'?")) {
                m_categories.delete_category(cats[sel - 1].name);
                m_categories.save("");
                std::cout << Color::GREEN << "已删除。\n" << Color::RESET;
            }
            _getch();
        }

        if (cmd == "3") {
            const auto& cats = m_categories.categories();
            if (cats.empty()) { _getch(); continue; }
            show_categories();
            std::cout << "选择要重命名的分类编号: ";
            int sel = read_number(1, static_cast<int>(cats.size()));
            std::string new_name = read_line("新名称: ");
            if (!new_name.empty()) {
                m_categories.rename_category(cats[sel - 1].name, new_name);
                m_categories.save("");
                std::cout << Color::GREEN << "已重命名。\n" << Color::RESET;
            }
            _getch();
        }

        if (cmd == "4") {
            const auto& cats = m_categories.categories();
            if (cats.empty()) { _getch(); continue; }
            show_categories();
            std::cout << "选择分类编号: ";
            int sel = read_number(1, static_cast<int>(cats.size()));
            show_category_games(cats[sel - 1].name);
            std::cout << "\n按任意键返回...";
            _getch();
        }
    }
}

// ============================================================
// 将游戏加入分类
// ============================================================
void ConsoleUI::add_game_to_categories(const GameInfo& game) {
    const auto& cats = m_categories.categories();
    if (cats.empty()) {
        std::cout << "还没有任何分类! 请先在分类管理中创建分类。\n按任意键...";
        _getch();
        return;
    }

    show_categories();
    std::cout << "\n选择分类编号 (0 = 取消): ";
    int sel = read_number(0, static_cast<int>(cats.size()));
    if (sel == 0) return;

    if (m_categories.add_game_to_category(game.appid, cats[sel - 1].name)) {
        m_categories.save("");
        std::cout << Color::GREEN << "已将 '" << game.name << "' 加入分类 '" 
                  << cats[sel - 1].name << "'。\n" << Color::RESET;
    }
    _getch();
}

// ============================================================
// 主循环
// ============================================================
void ConsoleUI::run() {
    // 构建游戏指针列表
    std::vector<GameInfo*> game_ptrs;
    for (auto& g : m_scanner.games()) {
        game_ptrs.push_back(&g);
    }

    paginate_games(game_ptrs, [this](int idx) {
        show_game_menu(m_scanner.games()[idx]);
    });

    // 退出时保存
    std::cout << "\n保存数据中...\n";
    m_categories.save("");
    m_playtime.save("");
    std::cout << Color::GREEN << "再见喵～ (｡･ω･｡)ﾉ♡\n" << Color::RESET;
}
