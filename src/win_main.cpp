// Win32 GUI 入口
#include "gui_app.h"
#include <windows.h>
#include <cstring>

// 简易 debug 日志, 可由 -debug 启动参数打开
static FILE* g_debug_log = nullptr;

static void debug_log(const char* fmt, ...) {
    if (!g_debug_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_debug_log, fmt, ap);
    va_end(ap);
    fflush(g_debug_log);
}

// 全局 debug log 入口, 给 SteamLauncherGUI::init 等用
extern "C" void steam_debug_log(const char* fmt, ...) {
    if (!g_debug_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_debug_log, fmt, ap);
    va_end(ap);
    fflush(g_debug_log);
}

static bool has_arg(int argc, const char* const* argv, const char* arg) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], arg) == 0) return true;
        // 也允许 "--debug" 形式
        if (arg[0] == '-' && argv[i][0] == '-' && strcmp(argv[i] + 1, arg + 1) == 0) return true;
    }
    return false;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    // 解析命令行参数 (wWinMain 不传 argc/argv, 需要自己解析)
    // 用 CommandLineToArgvW 拿到参数数组
    int argc = 0;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    const char* argv[64] = { nullptr };
    static char argbuf[64][256];
    for (int i = 0; i < argc && i < 64; ++i) {
        WideCharToMultiByte(CP_UTF8, 0, argvw[i], -1, argbuf[i], 256, nullptr, nullptr);
        argv[i] = argbuf[i];
    }
    if (argvw) LocalFree(argvw);

    bool debug_mode = has_arg(argc, argv, "-debug") || has_arg(argc, argv, "--debug");
    if (debug_mode) {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        // 写日志到文件 (附加)
        g_debug_log = fopen("E:\\temp\\steam_launcher_debug.log", "a");
        if (g_debug_log) {
            fprintf(g_debug_log, "\n=== SteamLauncher 启动 %s ===\n", __DATE__ " " __TIME__);
            fflush(g_debug_log);
        }
        printf("[debug] 控制台已启用, 日志写到 E:\\temp\\steam_launcher_debug.log\n");
        debug_log("[debug] 命令行 argc=%d\n", argc);
        for (int i = 0; i < argc; ++i) {
            debug_log("  argv[%d] = %s\n", i, argv[i]);
        }
    }

    SteamLauncherGUI app;
    if (!app.init(hInstance)) {
        if (debug_mode) {
            printf("[error] init 失败\n");
            if (g_debug_log) fclose(g_debug_log);
            FreeConsole();
        }
        MessageBoxW(nullptr, L"\u521D\u59CB\u5316\u5931\u8D25! \u8BF7\u786E\u8BA4 Steam \u5DF2\u5B89\u88C5\u4E14\u6709\u6E38\u620F\u3002",
                   L"\u9519\u8BEF", MB_ICONERROR);
        return 1;
    }
    int rc = app.run();
    if (debug_mode) {
        printf("[debug] 退出 rc=%d\n", rc);
        if (g_debug_log) fclose(g_debug_log);
        FreeConsole();
    }
    return rc;
}
