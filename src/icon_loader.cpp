#include "icon_loader.h"
#include "game_data.h"
#include <filesystem>
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;
using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")

// 前向声明
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

// ============================================================
IconLoader::IconLoader() {}

IconLoader::~IconLoader() {
    if (m_gdiplus_token) {
        GdiplusShutdown(m_gdiplus_token);
    }
    if (m_default_icon) {
        DeleteObject(m_default_icon);
    }
}

void IconLoader::init(const std::string& steam_path) {
    m_steam_path = steam_path;
    
    // 初始化 GDI+
    GdiplusStartupInput gdi_input;
    GdiplusStartup(&m_gdiplus_token, &gdi_input, nullptr);

    // 缓存目录: %TEMP%/SteamLauncher/icons/
    m_cache_dir = get_cache_dir();
    std::error_code ec;
    fs::create_directories(m_cache_dir, ec);
}

std::string IconLoader::get_cache_dir() {
    wchar_t temp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, temp_path);
    
    char buf[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, temp_path, -1, buf, sizeof(buf), nullptr, nullptr);
    std::string result(buf);
    std::replace(result.begin(), result.end(), '\\', '/');
    if (!result.empty() && result.back() != '/') result += '/';
    result += "SteamLauncher/icons/";
    return result;
}

// ============================================================
// 获取图标的主入口
// ============================================================
HBITMAP IconLoader::get_icon(uint32_t appid, const std::string& install_dir, 
                              const std::string& library_path) {
    std::string cache_path = m_cache_dir + std::to_string(appid) + ".png";

    // 1. 检查本地缓存
    if (fs::exists(cache_path)) {
        HBITMAP bmp = load_png_as_bitmap(cache_path, 32);
        if (bmp) return bmp;
    }

    // 2. 尝试从 Steam librarycache 加载
    if (load_from_steam_cache(appid, cache_path)) {
        HBITMAP bmp = load_png_as_bitmap(cache_path, 32);
        if (bmp) return bmp;
    }

    // 3. 尝试从游戏 exe 提取图标
    if (load_from_game_exe(appid, install_dir, library_path, cache_path)) {
        HBITMAP bmp = load_png_as_bitmap(cache_path, 32);
        if (bmp) return bmp;
    }

    // 4. 返回默认图标
    return m_default_icon;
}

// ============================================================
// 检查缓存
// ============================================================
bool IconLoader::is_cached(uint32_t appid) const {
    return fs::exists(m_cache_dir + std::to_string(appid) + ".png");
}

// ============================================================
// 从 Steam librarycache 目录查找图标
// ============================================================
bool IconLoader::load_from_steam_cache(uint32_t appid, const std::string& out_path) {
    std::string steam_cache = m_steam_path + "/appcache/librarycache/" + std::to_string(appid) + "/";
    std::error_code ec;
    
    if (!fs::exists(steam_cache, ec)) return false;

    // 优先找 logo_*.png (透明背景，适合列表)
    for (const auto& entry : fs::directory_iterator(steam_cache, ec)) {
        std::string fn = entry.path().filename().string();
        std::string ext = entry.path().extension().string();
        
        // 优先: logo PNG
        if (fn.find("logo_") == 0 && (ext == ".png" || ext == ".PNG")) {
            return resize_and_save(entry.path().string(), out_path, 32);
        }
    }

    // 其次: library_600x900 (裁剪中间部分)
    for (const auto& entry : fs::directory_iterator(steam_cache, ec)) {
        std::string fn = entry.path().filename().string();
        std::string ext = entry.path().extension().string();
        
        if (fn.find("library_600x900") == 0 && (ext == ".jpg" || ext == ".JPG")) {
            return resize_and_save(entry.path().string(), out_path, 32);
        }
    }

    // 再其次: 任何 jpg/png
    for (const auto& entry : fs::directory_iterator(steam_cache, ec)) {
        std::string ext = entry.path().extension().string();
        if (ext == ".jpg" || ext == ".JPG" || ext == ".png" || ext == ".PNG") {
            return resize_and_save(entry.path().string(), out_path, 32);
        }
    }

    return false;
}

// ============================================================
// 从游戏 exe 提取图标
// ============================================================
bool IconLoader::load_from_game_exe(uint32_t appid, const std::string& install_dir,
                                     const std::string& library_path, const std::string& out_path) {
    // 游戏安装目录: library_path/../common/<install_dir>/
    std::string lib_dir = library_path;
    // library_path 是 .../steamapps, 所以 common 在 ../common/
    size_t pos = lib_dir.rfind("/steamapps");
    if (pos == std::string::npos) {
        pos = lib_dir.rfind("\\steamapps");
    }
    std::string common_dir;
    if (pos != std::string::npos) {
        common_dir = lib_dir.substr(0, pos) + "/common/";
    } else {
        common_dir = library_path + "/common/";
    }
    std::replace(common_dir.begin(), common_dir.end(), '\\', '/');

    std::string game_dir = common_dir + install_dir + "/";
    std::error_code ec;
    if (!fs::exists(game_dir, ec)) return false;

    // 扫描 exe 文件，取最大的 (通常是主程序)
    std::string best_exe;
    uintmax_t best_size = 0;
    
    for (const auto& entry : fs::directory_iterator(game_dir, ec)) {
        if (entry.is_regular_file(ec)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".exe") {
                uintmax_t sz = entry.file_size(ec);
                if (sz > best_size) {
                    best_size = sz;
                    best_exe = entry.path().string();
                }
            }
        }
    }

    if (best_exe.empty()) return false;

    // 从 exe 提取图标
    HICON hIcon = nullptr;
    
    // 方法1: SHGetFileInfo (获取系统图标)
    SHFILEINFOA sfi = {};
    if (SHGetFileInfoA(best_exe.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {
        hIcon = sfi.hIcon;
    }

    if (!hIcon) {
        // 方法2: ExtractIcon
        hIcon = ExtractIconA(GetModuleHandle(nullptr), best_exe.c_str(), 0);
    }

    if (!hIcon) return false;

    // 把 HICON 转成 PNG 保存
    bool result = false;
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        // 创建 32x32 位图
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, 32, 32);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        // 填充背景
        RECT rect = {0, 0, 32, 32};
        HBRUSH bgBrush = CreateSolidBrush(RGB(240, 240, 240));
        FillRect(memDC, &rect, bgBrush);
        DeleteObject(bgBrush);

        // 绘制图标
        DrawIconEx(memDC, 0, 0, hIcon, 32, 32, 0, nullptr, DI_NORMAL);

        // 保存为 PNG (使用 GDI+)
        Bitmap bmp(32, 32, PixelFormat32bppARGB);
        Graphics gfx(&bmp);
        HDC bmpDC = gfx.GetHDC();
        BitBlt(bmpDC, 0, 0, 32, 32, memDC, 0, 0, SRCCOPY);
        gfx.ReleaseHDC(bmpDC);

        // 获取 PNG 编码器
        CLSID pngClsid;
        GetEncoderClsid(L"image/png", &pngClsid);

        std::wstring wpath(out_path.begin(), out_path.end());
        Status st = bmp.Save(wpath.c_str(), &pngClsid, nullptr);
        result = (st == Ok);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        ReleaseDC(nullptr, hdc);
    }

    DestroyIcon(hIcon);
    return result;
}

// ============================================================
// 缩放图片并保存
// ============================================================
bool IconLoader::resize_and_save(const std::string& src_path, const std::string& dst_path, int size) {
    std::wstring wsrc(src_path.begin(), src_path.end());
    std::wstring wdst(dst_path.begin(), dst_path.end());

    Bitmap srcBmp(wsrc.c_str());
    if (srcBmp.GetLastStatus() != Ok) return false;

    // 裁剪为正方形 (取中间)
    int srcW = srcBmp.GetWidth();
    int srcH = srcBmp.GetHeight();
    int cropSize = (std::min)(srcW, srcH);
    int cropX = (srcW - cropSize) / 2;
    int cropY = (srcH - cropSize) / 2;

    Bitmap dstBmp(size, size, PixelFormat32bppARGB);
    Graphics gfx(&dstBmp);
    gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    gfx.DrawImage(&srcBmp, Rect(0, 0, size, size), cropX, cropY, cropSize, cropSize, UnitPixel);

    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    Status st = dstBmp.Save(wdst.c_str(), &pngClsid, nullptr);
    return (st == Ok);
}

// ============================================================
// 从 PNG 加载为 HBITMAP
// ============================================================
HBITMAP IconLoader::load_png_as_bitmap(const std::string& path, int size) {
    std::wstring wpath(path.begin(), path.end());
    Bitmap bmp(wpath.c_str());
    if (bmp.GetLastStatus() != Ok) return nullptr;

    HBITMAP hBitmap = nullptr;
    bmp.GetHBITMAP(Color(0, 0, 0, 0), &hBitmap);
    return hBitmap;
}

// ============================================================
// 获取 PNG 编码器 CLSID
// ============================================================
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pCodecInfo = (ImageCodecInfo*)malloc(size);
    GetImageEncoders(num, size, pCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pCodecInfo[j].Clsid;
            free(pCodecInfo);
            return j;
        }
    }
    free(pCodecInfo);
    return -1;
}
