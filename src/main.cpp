// ============================================================
// 第三方 Steam 游戏启动器
// 
// 功能:
//   • 从 Steam 本地文件读取已安装游戏信息
//   • 使用 steam://rungameid/<appid> 协议启动游戏
//   • 多对多游戏分类系统
//   • 独立游玩时长追踪
//   • 控制台 UI 界面
//
// 编码说明:
//   • VDF/ACF 文件: UTF-8 (Steam 原生格式)
//   • 文件路径: Windows API CreateFileW (支持中文路径)
//   • 控制台输出: SetConsoleOutputCP(CP_UTF8) + ANSI 转义码
//   • JSON 存储: UTF-8 不带 BOM
//
// 构建:
//   cmake -B build -G "Visual Studio 17 2022"
//   cmake --build build --config Release
// ============================================================

#include "console_ui.h"
#include <iostream>
#include <conio.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    // 设置控制台编码为 UTF-8 (确保所有输出正确显示中文字符)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 启用 ANSI 转义码 (Windows 10 版本 1511+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    try {
        ConsoleUI ui;
        
        if (!ui.init()) {
            return 1;
        }

        ui.run();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n[严重错误] " << e.what() << "\n";
        std::cerr << "按任意键退出...";
        _getch();
        return 1;
    }
}
