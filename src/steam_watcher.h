#pragma once
#include <string>
#include <cstdint>

// ============================================================
// Steam 进程管理器
// 功能: 检测 Steam 是否运行, 自动启动 Steam, 等待就绪
// ============================================================
class SteamWatcher {
public:
    SteamWatcher();

    // 设置 Steam 安装路径
    void set_steam_path(const std::string& path);

    // 检测 Steam 是否正在运行
    bool is_steam_running();

    // 如果 Steam 未运行, 启动它 (返回 true 表示启动成功)
    bool start_steam();

    // 等待 Steam 就绪 (超时毫秒, 返回 true 表示就绪)
    bool wait_for_steam(int timeout_ms = 30000);

    // 获取 Steam 进程 ID
    uint32_t steam_pid() const { return m_steam_pid; }

    // 获取 Steam 主窗口句柄
    void* steam_hwnd() const { return m_steam_hwnd; }

    // 查找 Steam 进程 (公开，可用于外部调用)
    uint32_t find_steam_process();

private:
    std::string m_steam_path;
    uint32_t    m_steam_pid = 0;
    void*       m_steam_hwnd = nullptr;
    void*    find_steam_window();
};
