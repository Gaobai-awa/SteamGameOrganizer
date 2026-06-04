#include "steam_watcher.h"
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>

// ============================================================
SteamWatcher::SteamWatcher() {}

void SteamWatcher::set_steam_path(const std::string& path) {
    m_steam_path = path;
}

// 查找 Steam 进程
uint32_t SteamWatcher::find_steam_process() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);

    uint32_t pid = 0;
    if (Process32FirstW(hSnap, &pe)) {
        do {
            // steam.exe (英文) 或 steam.exe 的任何语言变体
            if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    m_steam_pid = pid;
    return pid;
}

// 查找 Steam 主窗口
void* SteamWatcher::find_steam_window() {
    m_steam_hwnd = FindWindowW(L"SDL_app", L"Steam");
    if (!m_steam_hwnd) {
        m_steam_hwnd = FindWindowW(L"vguiPopupWindow", L"Steam");
    }
    return m_steam_hwnd;
}

bool SteamWatcher::is_steam_running() {
    return find_steam_process() != 0;
}

// 启动 Steam
bool SteamWatcher::start_steam() {
    if (is_steam_running()) return true;
    if (m_steam_path.empty()) return false;

    std::string exe_path = m_steam_path + "/steam.exe";
    // 转换为宽字符
    int len = MultiByteToWideChar(CP_UTF8, 0, exe_path.c_str(), -1, nullptr, 0);
    std::wstring wexe(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, exe_path.c_str(), -1, &wexe[0], len);
    wexe.resize(len - 1);

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpFile = wexe.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            // 不等完成, 立即返回
            CloseHandle(sei.hProcess);
            return true;
        }
    }
    return false;
}

// 等待 Steam 就绪
bool SteamWatcher::wait_for_steam(int timeout_ms) {
    int waited = 0;
    int interval = 500;

    // 等待进程出现
    while (waited < timeout_ms) {
        if (find_steam_process()) {
            // 找到进程, 再等窗口
            int waited2 = 0;
            while (waited2 < 15000) {
                if (find_steam_window()) {
                    // 再等一会儿确保完全加载
                    Sleep(2000);
                    return true;
                }
                Sleep(500);
                waited2 += 500;
            }
            return true; // 有进程没窗口也行
        }
        Sleep(interval);
        waited += interval;
    }
    return find_steam_process() != 0;
}
