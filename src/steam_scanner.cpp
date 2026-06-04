#include "steam_scanner.h"
#include "vdf_parser.h"
#include <windows.h>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <iostream>

namespace fs = std::filesystem;

// ============================================================
// 从注册表查找 Steam 安装路径
// 注册表路径: HKEY_CURRENT_USER\SOFTWARE\Valve\Steam
// 键名: SteamPath
// ============================================================
std::string SteamScanner::find_steam_path() {
    HKEY hKey;
    // 先用 64 位视图
    if (RegOpenKeyExA(HKEY_CURRENT_USER, 
        "SOFTWARE\\Valve\\Steam", 0, 
        KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        
        char value[1024] = {};
        DWORD size = sizeof(value);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, 
            reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::string path(value);
            // 统一使用正斜杠
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }
        RegCloseKey(hKey);
    }

    // 尝试 32 位视图
    if (RegOpenKeyExA(HKEY_CURRENT_USER, 
        "SOFTWARE\\Valve\\Steam", 0, 
        KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        
        char value[1024] = {};
        DWORD size = sizeof(value);
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, 
            reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::string path(value);
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }
        RegCloseKey(hKey);
    }

    // 尝试 HKLM
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Valve\\Steam", 0,
        KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        
        char value[1024] = {};
        DWORD size = sizeof(value);
        if (RegQueryValueExA(hKey, "InstallPath", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            std::string path(value);
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }
        RegCloseKey(hKey);
    }

    return "";
}

// ============================================================
// 解析 libraryfolders.vdf，获取所有库文件夹路径
// ============================================================
std::vector<std::string> SteamScanner::get_library_paths(const std::string& steam_path) {
    std::vector<std::string> paths;

    // 默认库文件夹 (Steam 安装目录本身)
    paths.push_back(steam_path + "/steamapps");

    // 读取 libraryfolders.vdf
    std::string vdf_path = steam_path + "/steamapps/libraryfolders.vdf";
    
    try {
        VDFNode root = VDFParser::parse_file(vdf_path);
        
        // 新版 VDF 格式 (libraryfolders 直接是对象)
        if (!root.has("libraryfolders")) {
            return paths;
        }
        
        const VDFNode& lib_folders = root["libraryfolders"];
        
        // 遍历所有数字索引
        for (int i = 0; ; ++i) {
            std::string idx = std::to_string(i);
            if (!lib_folders.has(idx)) break;
            
            const VDFNode& entry = lib_folders[idx];
            
            // 优先读取 "path" 字段
            std::string lib_path = entry.get_str("path");
            if (lib_path.empty()) {
                // 旧版: 索引节点的子节点里可能有 path
                if (entry.type == VDFType::Object) {
                    for (const auto& [k, v] : entry.children) {
                        if (v.type == VDFType::String && k != "label" && k != "contentstatsid") {
                            lib_path = v.str_value;
                            break;
                        }
                    }
                }
            }

            if (!lib_path.empty()) {
                std::replace(lib_path.begin(), lib_path.end(), '\\', '/');
                std::string apps_path = lib_path + "/steamapps";
                // 避免重复
                if (std::find(paths.begin(), paths.end(), apps_path) == paths.end()) {
                    paths.push_back(apps_path);
                }
            }
        }
    }
    catch (const std::exception& e) {
        // libraryfolders.vdf 解析失败，至少有默认路径
        std::cerr << "[警告] 无法解析 libraryfolders.vdf: " << e.what() << "\n";
    }

    return paths;
}

// ============================================================
// 解析单个 appmanifest_*.acf 文件
// ============================================================
GameInfo SteamScanner::parse_appmanifest(const std::string& filepath, const std::string& library_path) {
    GameInfo info;
    info.library_path = library_path;

    try {
        VDFNode root = VDFParser::parse_file(filepath);
        
        if (!root.has("AppState")) {
            return info;
        }

        const VDFNode& state = root["AppState"];
        info.appid        = static_cast<uint32_t>(state.get_uint64("appid"));
        info.name          = state.get_str("name");
        info.install_dir   = state.get_str("installdir");
        info.size_on_disk  = state.get_uint64("SizeOnDisk");
        info.last_updated  = state.get_uint64("LastUpdated");
        info.state_flags   = static_cast<int>(state.get_uint64("StateFlags"));

        // 合并 Steam 记录的游玩数据
        auto it = m_playtime_data.find(info.appid);
        if (it != m_playtime_data.end()) {
            info.total_playtime_min = it->second.first;
            info.last_played        = it->second.second;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[警告] 无法解析 " << filepath << ": " << e.what() << "\n";
    }

    return info;
}

// ============================================================
// 读取 localconfig.vdf 获取用户游玩数据
// ============================================================
void SteamScanner::read_localconfig(const std::string& config_path) {
    try {
        VDFNode root = VDFParser::parse_file(config_path);

        // 路径: UserLocalConfigStore -> Software -> Valve -> Steam -> apps -> {appid}
        if (!root.has("UserLocalConfigStore")) return;
        const VDFNode& store = root["UserLocalConfigStore"];
        if (!store.has("Software")) return;
        const VDFNode& sw = store["Software"];
        if (!sw.has("Valve")) return;
        const VDFNode& valve = sw["Valve"];
        if (!valve.has("Steam")) return;
        const VDFNode& steam = valve["Steam"];

        // 可能有 apps 节点在 steam 下
        const VDFNode* apps_node = nullptr;
        if (steam.has("apps")) {
            apps_node = &steam["apps"];
        }

        if (apps_node && apps_node->type == VDFType::Object) {
            for (const auto& [appid_str, app_data] : apps_node->children) {
                try {
                    uint32_t appid = static_cast<uint32_t>(std::stoul(appid_str));

                    uint64_t playtime = 0;
                    uint64_t last_played = 0;

                    if (app_data.type == VDFType::Object) {
                        playtime    = app_data.get_uint64("Playtime");
                        last_played = app_data.get_uint64("LastPlayed");
                    }

                    // 合并数据：取最大值（可能有多个用户）
                    auto it = m_playtime_data.find(appid);
                    if (it != m_playtime_data.end()) {
                        if (playtime > it->second.first) it->second.first = playtime;
                        if (last_played > it->second.second) it->second.second = last_played;
                    }
                    else {
                        m_playtime_data[appid] = { playtime, last_played };
                    }

                    // 读取离线游玩时长 (PlaytimeDisconnected)
                    if (app_data.type == VDFType::Object) {
                        uint64_t playtime_dc = app_data.get_uint64("PlaytimeDisconnected");
                        if (playtime_dc > 0) {
                            auto dcit = m_playtime_disconnected.find(appid);
                            if (dcit != m_playtime_disconnected.end()) {
                                if (playtime_dc > dcit->second) dcit->second = playtime_dc;
                            } else {
                                m_playtime_disconnected[appid] = playtime_dc;
                            }
                        }
                    }
                }
                catch (...) {
                    // 跳过格式异常的条目
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[警告] 无法解析 localconfig: " << e.what() << "\n";
    }
}

// ============================================================
// 读取所有用户的游玩数据
// ============================================================
std::map<uint32_t, std::pair<uint64_t, uint64_t>> 
SteamScanner::read_user_playtime(const std::string& steam_path) {
    m_playtime_data.clear();
    m_playtime_disconnected.clear();

    std::string userdata_path = steam_path + "/userdata";
    
    std::error_code ec;
    if (!fs::exists(userdata_path, ec)) {
        return m_playtime_data;
    }

    // 遍历所有用户目录
    for (const auto& entry : fs::directory_iterator(userdata_path, ec)) {
        if (!entry.is_directory(ec)) continue;
        
        std::string config_path = entry.path().string() + "/config/localconfig.vdf";
        std::replace(config_path.begin(), config_path.end(), '\\', '/');
        
        if (fs::exists(config_path, ec)) {
            read_localconfig(config_path);
        }
    }

    return m_playtime_data;
}

// ============================================================
// 扫描所有已安装游戏
// ============================================================
std::vector<GameInfo> SteamScanner::scan_all_games() {
    m_steam_path = find_steam_path();
    
    if (m_steam_path.empty()) {
        std::cerr << "[错误] 未找到 Steam 安装。请确认 Steam 已安装。\n";
        return {};
    }

    std::cout << "[信息] Steam 安装路径: " << m_steam_path << "\n";

    // 先读取所有用户的游玩数据
    read_user_playtime(m_steam_path);

    // 获取所有库文件夹
    m_library_paths = get_library_paths(m_steam_path);
    std::cout << "[信息] 找到 " << m_library_paths.size() << " 个库文件夹\n";

    // 扫描每个库文件夹中的游戏
    m_games.clear();
    std::set<uint32_t> seen_appids;   // 去重：同一 appid 只保留一次
    std::error_code ec;

    for (const auto& lib_path : m_library_paths) {
        if (!fs::exists(lib_path, ec)) continue;

        for (const auto& entry : fs::directory_iterator(lib_path, ec)) {
            std::string filename = entry.path().filename().string();
            
            // 匹配 appmanifest_*.acf
            if (filename.find("appmanifest_") == 0 && 
                filename.find(".acf") != std::string::npos) {
                
                std::string fullpath = entry.path().string();
                std::replace(fullpath.begin(), fullpath.end(), '\\', '/');
                
                GameInfo info = parse_appmanifest(fullpath, lib_path);
                if (info.appid != 0 && seen_appids.find(info.appid) == seen_appids.end()) {
                    seen_appids.insert(info.appid);
                    // DLC 检测: 检查 common/<installdir> 目录是否存在且包含有效内容
                    if (!info.install_dir.empty()) {
                        std::string common_path = info.library_path + "/common/" + info.install_dir + "/";
                        std::error_code ec2;
                        bool exists = fs::exists(common_path, ec2);
                        bool has_exe_anywhere = false;
                        if (exists) {
                            // 递归搜索 exe (最多 3 层)
                            std::function<void(const fs::path&, int)> scan_dir;
                            scan_dir = [&](const fs::path& dir, int depth) {
                                if (depth > 3 || has_exe_anywhere) return;
                                for (const auto& f : fs::directory_iterator(dir, ec2)) {
                                    if (has_exe_anywhere) break;
                                    if (f.is_regular_file(ec2)) {
                                        std::string ext = f.path().extension().string();
                                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                                        if (ext == ".exe") { has_exe_anywhere = true; break; }
                                    } else if (f.is_directory(ec2)) {
                                        scan_dir(f.path(), depth + 1);
                                    }
                                }
                            };
                            scan_dir(common_path, 0);
                        }
                        // 有 exe = 独立游戏; 目录不存在或没 exe = DLC/工具
                        if (!exists || !has_exe_anywhere) info.is_dlc = true;
                    } else {
                        // installdir 为空 → DLC
                        info.is_dlc = true;
                    }
                    m_games.push_back(info);
                }
            }
        }
    }

    // 按名称排序
    std::sort(m_games.begin(), m_games.end(), 
        [](const GameInfo& a, const GameInfo& b) {
            return a.name < b.name;
        });

    // 调试: 输出前 10 个游戏名 + 验证 4 个目标 appid 的中文名
    std::cout << "[成功] 共发现 " << m_games.size() << " 个 App\n";
    int log_count = (int)m_games.size() < 10 ? (int)m_games.size() : 10;
    for (int i = 0; i < log_count; ++i) {
        std::cout << "  [" << i << "] appid=" << m_games[i].appid << " name=" << m_games[i].name << "\n";
    }
    {
        // 调试: 把所有游戏名写到文件, 验证中文名
        FILE* dbf = fopen("E:\\temp\\steam_chinese_name_check.txt", "w");
        if (dbf) {
            fprintf(dbf, "Total games: %zu\n", m_games.size());
            // 测试的 4 个目标 appid: 逆转裁判(975810), VR2(1348700), 戴森球(1361210), 帕鲁(2344520)
            std::vector<uint32_t> test_apps = {975810, 1348700, 1361210, 1623730, 2344520};
            for (uint32_t appid : test_apps) {
                bool found = false;
                for (const auto& g : m_games) {
                    if (g.appid == appid) {
                        std::string hex;
                        for (char c : g.name) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "%02X ", (unsigned char)c);
                            hex += buf;
                        }
                        fprintf(dbf, "[FOUND] appid=%u  name='%s'  bytes=[%s]\n", g.appid, g.name.c_str(), hex.c_str());
                        found = true;
                        break;
                    }
                }
                if (!found) fprintf(dbf, "[MISSING] appid=%u  not in m_games\n", appid);
            }
            // 也输出前 50 个名字 (按 appid 排序)
            fprintf(dbf, "\n--- First 50 game names (sorted) ---\n");
            std::vector<GameInfo> sorted = m_games;
            std::sort(sorted.begin(), sorted.end(), [](const GameInfo& a, const GameInfo& b){
                return a.appid < b.appid;
            });
            for (size_t i = 0; i < sorted.size() && i < 50; ++i) {
                fprintf(dbf, "  appid=%u  name='%s'\n", sorted[i].appid, sorted[i].name.c_str());
            }
            fclose(dbf);
        }
    }
    return m_games;
}

// ============================================================
void SteamScanner::refresh() {
    scan_all_games();
}

const GameInfo* SteamScanner::find_game(uint32_t appid) const {
    for (const auto& g : m_games) {
        if (g.appid == appid) return &g;
    }
    return nullptr;
}

GameInfo* SteamScanner::find_game(uint32_t appid) {
    for (auto& g : m_games) {
        if (g.appid == appid) return &g;
    }
    return nullptr;
}


// ============================================================
// 把 AppInfoEntry 转换成 GameInfo
// ============================================================
GameInfo SteamScanner::appinfo_to_gameinfo(const AppInfoEntry& app) {
    GameInfo info;
    info.appid        = app.appid;
    info.name         = BinaryVDFReader::get_app_localized_name(app, "schinese");
    info.install_dir  = BinaryVDFReader::get_app_installdir(app);
    info.last_updated = app.last_updated;
    info.state_flags  = (int)app.info_state;
    info.size_on_disk = 0;
    info.is_dlc       = false;
    info.is_installed = false;

    // 类型判断
    std::string type = BinaryVDFReader::get_app_type(app);
    info.app_type = type;
    // 转换为小写, 便于对比
    std::string type_lc = type;
    for (auto& c : type_lc) c = (char)tolower((unsigned char)c);
    if (type_lc == "dlc") {
        info.is_dlc = true;
    } else if (type_lc == "tool") {
        // 工具 (Dedicated Server / SDK / 游戏支持实用工具)
        info.is_dlc = true;
    } else if (type_lc == "music" || type_lc == "video" || type_lc == "config") {
        info.is_dlc = true;
    } else if (type_lc == "game" || type_lc.empty()) {
        info.is_dlc = false;
    } else {
        info.is_dlc = true;
    }

    // 合并游玩时间
    auto it = m_playtime_data.find(info.appid);
    if (it != m_playtime_data.end()) {
        info.total_playtime_min = it->second.first;
        info.last_played        = it->second.second;
    }
    // 合并离线游玩时长 (PlaytimeDisconnected)
    auto dcit = m_playtime_disconnected.find(info.appid);
    if (dcit != m_playtime_disconnected.end()) {
        info.playtime_disconnected_min = dcit->second;
    }
    return info;
}

// ============================================================
// 通过解密 appinfo.vdf 扫描所有 Steam 已知 App
// 这种方式不需要 libararyfolders.vdf / appmanifest_*.acf 存在
// 可以列出已卸载但 Steam 仍然记录的所有 App
// ============================================================
std::vector<GameInfo> SteamScanner::scan_from_appinfo(bool show_tools) {
    m_steam_path = find_steam_path();
    m_show_tools = show_tools;
    if (m_steam_path.empty()) {
        std::cerr << "[错误] 未找到 Steam 安装。请确认 Steam 已安装。\n";
        return {};
    }
    std::cout << "[信息] Steam 安装路径: " << m_steam_path << "\n";

    // 先读取游玩时间
    read_user_playtime(m_steam_path);

    // 库文件夹列表 (用于判断是否已安装)
    m_library_paths = get_library_paths(m_steam_path);
    std::cout << "[信息] 找到 " << m_library_paths.size() << " 个库文件夹\n";

    // 打开 appinfo.vdf
    std::string appinfo_path = m_steam_path + "/appcache/appinfo.vdf";
    BinaryVDFReader reader;
    if (!reader.open_appinfo(appinfo_path)) {
        std::cerr << "[错误] 解析 appinfo.vdf 失败: " << reader.last_error() << "\n";
        return {};
    }

    m_games.clear();
    std::set<uint32_t> seen_appids;
    std::error_code ec;

    for (const auto& app : reader.apps()) {
        if (app.appid == 0) continue;
        if (seen_appids.find(app.appid) != seen_appids.end()) continue;
        seen_appids.insert(app.appid);

        GameInfo info = appinfo_to_gameinfo(app);
        // 过滤 DLC / 音乐 / 视频 / 配置
        if (info.is_dlc && info.app_type != "Tool" && info.app_type != "tool") continue;
        // 工具 (Dedicated Server / SDK / 其它工具) 根据 m_show_tools 决定
        if (info.app_type == "Tool" || info.app_type == "tool") {
            if (!m_show_tools) continue;
        }
        if (info.name.empty()) continue; // 跳过无名 App

        // 检查是否已安装: 对每个库文件夹, 查看 common/<installdir>/ 是否存在
        if (!info.install_dir.empty()) {
            for (const auto& lib : m_library_paths) {
                std::string common_path = lib + "/common/" + info.install_dir;
                if (fs::exists(common_path, ec)) {
                    info.is_installed = true;
                    info.library_path = lib;
                    // 尝试读取对应的 appmanifest_*.acf 获取 size/last_updated
                    std::string manifest_path = lib + "/appmanifest_" + std::to_string(info.appid) + ".acf";
                    if (fs::exists(manifest_path, ec)) {
                        try {
                            VDFNode root = VDFParser::parse_file(manifest_path);
                            if (root.has("AppState")) {
                                const VDFNode& state = root["AppState"];
                                info.size_on_disk = state.get_uint64("SizeOnDisk");
                                uint64_t lu = state.get_uint64("LastUpdated");
                                if (lu > 0) info.last_updated = lu;
                            }
                        } catch (...) {}
                    }
                    break;
                }
            }
        } else {
            // appinfo.vdf 没有 installdir 字段 (现代格式)
            // 尝试找匹配的 appmanifest_*.acf
            for (const auto& lib : m_library_paths) {
                std::string manifest_path = lib + "/appmanifest_" + std::to_string(info.appid) + ".acf";
                if (fs::exists(manifest_path, ec)) {
                    try {
                        VDFNode root = VDFParser::parse_file(manifest_path);
                        if (root.has("AppState")) {
                            const VDFNode& state = root["AppState"];
                            info.install_dir = state.get_str("installdir");
                            info.size_on_disk = state.get_uint64("SizeOnDisk");
                            uint64_t lu = state.get_uint64("LastUpdated");
                            if (lu > 0) info.last_updated = lu;
                            info.is_installed = true;
                            info.library_path = lib;
                        }
                    } catch (...) {}
                    if (info.is_installed) break;
                }
            }
        }

        m_games.push_back(info);
    }

    // 按名称排序
    std::sort(m_games.begin(), m_games.end(),
        [](const GameInfo& a, const GameInfo& b) {
            return a.name < b.name;
        });

    std::cout << "[信息] 通过 appinfo.vdf 共发现 " << m_games.size() << " 个 App\n";
    return m_games;
}