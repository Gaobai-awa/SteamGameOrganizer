#include "dll_injector.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <sstream>
#include <iostream>

bool DLLInjector::inject(uint32_t pid, const std::string& dll_path) {
    // 打开目标进程
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);

    if (!hProcess) {
        m_error = "无法打开进程 (PID: " + std::to_string(pid) + ")";
        return false;
    }

    // 转换 DLL 路径为宽字符
    int len = MultiByteToWideChar(CP_UTF8, 0, dll_path.c_str(), -1, nullptr, 0);
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, dll_path.c_str(), -1, &wpath[0], len);
    wpath.resize(len - 1);

    // 在目标进程中分配内存
    size_t path_size = (wpath.size() + 1) * sizeof(wchar_t);
    void* remote_mem = VirtualAllocEx(hProcess, nullptr, path_size,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_mem) {
        m_error = "VirtualAllocEx 失败";
        CloseHandle(hProcess);
        return false;
    }

    // 写入 DLL 路径
    if (!WriteProcessMemory(hProcess, remote_mem, wpath.c_str(), path_size, nullptr)) {
        m_error = "WriteProcessMemory 失败";
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 获取 kernel32.dll 的 LoadLibraryW 地址
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC pLoadLibraryW = GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
        m_error = "GetProcAddress(LoadLibraryW) 失败";
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 创建远程线程调用 LoadLibraryW
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibraryW, remote_mem, 0, nullptr);

    if (!hThread) {
        m_error = "CreateRemoteThread 失败";
        VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // 等待线程完成
    WaitForSingleObject(hThread, 10000);

    // 获取退出码 (DLL 入口点返回值)
    DWORD exit_code = 0;
    GetExitCodeThread(hThread, &exit_code);

    // 清理
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remote_mem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (exit_code == 0) {
        m_error = "DLL 加载失败 (LoadLibraryW 返回 0)";
        return false;
    }

    return true;
}

bool DLLInjector::eject(uint32_t pid, const std::string& dll_name) {
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_READ,
        FALSE, pid);

    if (!hProcess) {
        m_error = "无法打开进程";
        return false;
    }

    // 在目标进程中找到已加载的 DLL 模块
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        m_error = "EnumProcessModules 失败";
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hTargetMod = nullptr;
    for (DWORD i = 0; i < cbNeeded / sizeof(HMODULE); ++i) {
        wchar_t mod_name[MAX_PATH];
        if (GetModuleBaseNameW(hProcess, hMods[i], mod_name, MAX_PATH)) {
            if (_wcsicmp(mod_name, L"steam_hook.dll") == 0) {
                hTargetMod = hMods[i];
                break;
            }
        }
    }

    if (!hTargetMod) {
        m_error = "在目标进程中未找到 DLL";
        CloseHandle(hProcess);
        return false;
    }

    // 获取 kernel32.dll 的 FreeLibrary 地址
    FARPROC pFreeLibrary = GetProcAddress(
        GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        (LPTHREAD_START_ROUTINE)pFreeLibrary, hTargetMod, 0, nullptr);

    if (!hThread) {
        m_error = "CreateRemoteThread(FreeLibrary) 失败";
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    CloseHandle(hProcess);
    return true;
}
