#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <set>

// ============================================================
// AppSettings
// ============================================================
struct AppSettings {
    // Steam 安装路径 (可手动修改)
    std::string steam_path;

    // 游戏读取方式:
    // 0 = 读取已安装游戏 (appmanifest_*.acf, 默认)
    // 1 = 注入 DLL 到 Steam (OpenSteamTool, 已废弃, 保留代码)
    // 2 = 解密 appinfo.vdf (含未安装游戏, 推荐用于查看完整游戏库)
    int game_scan_method = 2;

    // 显示工具类 App (Dedicated Server / SDK / 游戏支持实用工具)
    // 默认不显示, 该选项仅在 game_scan_method == 2 时有效
    bool show_tools = false;

    // 已删除的游戏 AppID 集合（从游戏列表中隐藏，但不从 Steam 删除）
    std::set<uint32_t> hidden_appids;

    // 上次使用的用户ID (用于成就读取)
    uint32_t last_user_id = 0;

    // 默认构造
    AppSettings() = default;

    // 从文件加载
    static AppSettings load(const std::string& path) {
        AppSettings s;
        std::ifstream f(path);
        if (!f) return s;
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        f.close();

        auto find_val = [&](const std::string& key) -> std::string {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            pos = content.find(':', pos);
            if (pos == std::string::npos) return "";
            pos = content.find_first_of("\"0123456789", pos + 1);
            if (pos == std::string::npos) return "";
            if (content[pos] == '\"') {
                ++pos;
                std::string val;
                while (pos < content.size() && content[pos] != '\"') val += content[pos++];
                return val;
            }
            std::string val;
            while (pos < content.size() && (isdigit(content[pos]) || content[pos] == '-'))
                val += content[pos++];
            return val;
        };

        s.steam_path = find_val("steam_path");
        std::string method = find_val("game_scan_method");
        if (!method.empty()) s.game_scan_method = std::stoi(method);
        std::string uid = find_val("last_user_id");
        if (!uid.empty()) s.last_user_id = (uint32_t)std::stoul(uid);
        std::string st = find_val("show_tools");
        if (!st.empty()) s.show_tools = (st == "true" || st == "1");

        // 加载已删除的 AppID 列表
        {
            size_t hpos = content.find("\"hidden_appids\"");
            if (hpos != std::string::npos) {
                hpos = content.find("[", hpos);
                if (hpos != std::string::npos) {
                    size_t hend = content.find("]", hpos);
                    if (hend != std::string::npos) {
                        std::string arr = content.substr(hpos + 1, hend - hpos - 1);
                        size_t p = 0;
                        while (p < arr.size()) {
                            while (p < arr.size() && (arr[p] == ' ' || arr[p] == ',' || arr[p] == '\n' || arr[p] == '\r' || arr[p] == '\t')) ++p;
                            std::string num;
                            while (p < arr.size() && isdigit((unsigned char)arr[p])) { num += arr[p]; ++p; }
                            if (!num.empty()) {
                                try { s.hidden_appids.insert((uint32_t)std::stoul(num)); } catch (...) {}
                            }
                        }
                    }
                }
            }
        }

        return s;
    }

    // 保存到文件
    void save(const std::string& path) {
        std::ofstream f(path, std::ios::trunc);
        if (!f) return;
        auto esc = [](const std::string& s) -> std::string {
            std::string r;
            for (char c : s) { if (c == '\"') r += "\\\""; else r += c; }
            return r;
        };
        f << "{\n";
        f << "    \"steam_path\": \"" << esc(steam_path) << "\",\n";
        f << "    \"game_scan_method\": " << game_scan_method << ",\n";
        f << "    \"show_tools\": " << (show_tools ? "true" : "false") << ",\n";
        f << "    \"last_user_id\": " << last_user_id << ",\n";
        f << "    \"hidden_appids\": [";
        bool first = true;
        for (uint32_t id : hidden_appids) {
            if (!first) f << ",";
            f << id;
            first = false;
        }
        f << "]\n";
        f << "}\n";
        f.close();
    }
};
