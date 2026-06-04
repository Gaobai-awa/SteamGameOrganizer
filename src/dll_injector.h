#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

// ============================================================
// DLL 注入器
// 使用 CreateRemoteThread + LoadLibraryW 方式注入
// ============================================================
class DLLInjector {
public:
    DLLInjector() = default;

    // 注入 DLL 到目标进程
    // pid: 目标进程ID
    // dll_path: DLL 的完整路径 (UTF-8)
    // 返回 true 表示注入成功
    bool inject(uint32_t pid, const std::string& dll_path);

    // 弹出注入后的 DLL
    bool eject(uint32_t pid, const std::string& dll_name);

    // 获取错误信息
    const std::string& last_error() const { return m_error; }

private:
    std::string m_error;

    // 在目标进程中调用 LoadLibraryW
    bool call_load_library(HANDLE hProcess, const std::wstring& wpath);
};
