#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <gdiplus.h>

// ============================================================
// Steam 游戏图标加载器
//
// 加载顺序:
//   1. %TEMP%/SteamLauncher/icons/<appid>.png (本地缓存)
//   2. steam/appcache/librarycache/<appid>/ (Steam 库缓存)
//   3. 游戏 exe 图标 (SHGetFileInfo)
//
// 所有图标统一缓存到 %TEMP% 目录
// ============================================================

class IconLoader {
public:
    IconLoader();
    ~IconLoader();

    // 初始化 (需要 Steam 路径)
    void init(const std::string& steam_path);

    // 获取游戏的 32x32 图标句柄 (用于 ListView 图像列表)
    // 自动从缓存/Steam/exe 加载
    HBITMAP get_icon(uint32_t appid, const std::string& install_dir, const std::string& library_path);

    // 预加载一批游戏图标 (后台线程)
    void preload_icons(const std::vector<struct GameInfo>& games);

    // 检查缓存是否存在
    bool is_cached(uint32_t appid) const;

    // 获取缓存目录
    static std::string get_cache_dir();

private:
    // 从 Steam librarycache 目录找图标
    bool load_from_steam_cache(uint32_t appid, const std::string& out_path);

    // 从游戏 exe 提取图标
    bool load_from_game_exe(uint32_t appid, const std::string& install_dir, 
                            const std::string& library_path, const std::string& out_path);

    // 加载图片文件并缩放到 32x32，保存为 PNG
    bool resize_and_save(const std::string& src_path, const std::string& dst_path, int size = 32);

    // 从 PNG 文件加载 HBITMAP
    static HBITMAP load_png_as_bitmap(const std::string& path, int size = 32);

    std::string m_steam_path;
    std::string m_cache_dir;
    ULONG_PTR   m_gdiplus_token = 0;
    
    // 默认图标 (无图标时使用)
    HBITMAP m_default_icon = nullptr;
};
