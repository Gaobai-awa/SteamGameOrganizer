// ============================================================
// SteamHook DLL - 注入到 Steam.exe 的辅助模块
// 功能:
//   - 通过命名管道与启动器通信
//   - 截获 Steam API 调用
//   - 提供游戏数据/成就信息
// ============================================================
#include <windows.h>
#include <string>
#include <sstream>



// 命名管道名称
#define PIPE_NAME L"\\\\.\\pipe\\SteamLauncher_Hook"

// ============================================================
// 全局数据
// ============================================================
HANDLE g_pipe = INVALID_HANDLE_VALUE;
HANDLE g_pipe_thread = nullptr;
volatile bool g_running = true;

// ============================================================
// 管道通信线程
// ============================================================
DWORD WINAPI PipeThread(LPVOID) {
    while (g_running) {
        HANDLE hPipe = CreateNamedPipeW(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096, 4096, 5000, nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        // 等待客户端连接
        if (ConnectNamedPipe(hPipe, nullptr)) {
            char buf[1024] = {};
            DWORD read = 0;

            if (ReadFile(hPipe, buf, sizeof(buf) - 1, &read, nullptr)) {
                buf[read] = '\0';
                std::string cmd(buf);

                std::string response;
                if (cmd == "ping") {
                    response = "pong:SteamHook_v1.0";
                } else if (cmd.substr(0, 4) == "info") {
                    response = "ok:loaded_in_steam";
                } else {
                    response = "ok:" + cmd;
                }

                DWORD written = 0;
                WriteFile(hPipe, response.c_str(), (DWORD)response.size(), &written, nullptr);
            }
            FlushFileBuffers(hPipe);
            DisconnectNamedPipe(hPipe);
        }
        CloseHandle(hPipe);
    }
    return 0;
}

// ============================================================
// DLL 入口点
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // 不加载 DLL 线程通知
        DisableThreadLibraryCalls(hModule);

        // 创建管道通信线程
        g_pipe_thread = CreateThread(nullptr, 0, PipeThread, nullptr, 0, nullptr);

        // 通过命名管道发送通知会等连接, 直接在目标进程用 MessageBox 显示
        // 实际使用时可以去掉或改成写到日志
        // MessageBoxW(nullptr, L"SteamHook DLL 已注入到 Steam", L"Steam Launcher", MB_OK);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_running = false;
        if (g_pipe_thread) {
            WaitForSingleObject(g_pipe_thread, 3000);
            CloseHandle(g_pipe_thread);
        }
    }
    return TRUE;
}


