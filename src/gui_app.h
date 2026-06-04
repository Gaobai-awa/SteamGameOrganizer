#pragma once
// ============================================================
// ImGui-based GUI for Steam Game Launcher
// Uses Dear ImGui (v1.91.0) + DirectX 11 + Win32 host window
//
// 编码策略:
//   后端 -> UTF-8 std::string
//   前端 -> ImGui::Text 等使用 UTF-8 (ImGui 内置 utf-8 支持)
// ============================================================

#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <functional>
#include "game_data.h"
#include "steam_scanner.h"
#include "category_manager.h"
#include "playtime_tracker.h"
#include "game_launcher.h"
#include "icon_loader.h"
#include "app_settings.h"
#include "steam_watcher.h"
#include "dll_injector.h"

// ImGui + DX11
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi1_2.h>

// Forward declare from imgui_impl_win32
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// UTF-8 <-> UTF-16 转换辅助 (for ShellExecuteW, etc.)
inline std::wstring to_wide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

inline std::string to_utf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

// ============================================================================
//  ImGui theme constants (dark hacker style inspired by JackalClient)
// ============================================================================
namespace Theme {
    inline ImVec4 Bg()         { return ImVec4(0.071f, 0.071f, 0.094f, 1.0f); }  // 18,18,24
    inline ImVec4 BgPanel()    { return ImVec4(0.094f, 0.094f, 0.125f, 1.0f); }  // 24,24,32
    inline ImVec4 BgHeader()    { return ImVec4(0.118f, 0.118f, 0.157f, 1.0f); }  // 30,30,40
    inline ImVec4 Accent()     { return ImVec4(0.424f, 0.361f, 0.906f, 1.0f); }  // 108,92,231  purple
    inline ImVec4 Accent2()    { return ImVec4(0.000f, 0.824f, 1.000f, 1.0f); }  // 0,210,255   cyan
    inline ImVec4 Text()       { return ImVec4(0.937f, 0.949f, 0.957f, 1.0f); }  // 240,242,244
    inline ImVec4 TextDim()    { return ImVec4(0.620f, 0.620f, 0.671f, 1.0f); }  // 158,158,171
    inline ImVec4 Border()     { return ImVec4(0.220f, 0.220f, 0.275f, 1.0f); }  // 56,56,70
    inline ImVec4 Hover()      { return ImVec4(0.157f, 0.157f, 0.196f, 1.0f); }  // 40,40,50
    inline ImVec4 Active()     { return ImVec4(0.196f, 0.196f, 0.255f, 1.0f); }  // 50,50,65
    inline ImVec4 Red()        { return ImVec4(0.937f, 0.325f, 0.314f, 1.0f); }
    inline ImVec4 Green()      { return ImVec4(0.298f, 0.835f, 0.412f, 1.0f); }
}

class SteamLauncherGUI {
public:
    SteamLauncherGUI();
    ~SteamLauncherGUI();

    bool init(HINSTANCE hInstance);
    int  run();

    // Make accessible to the static WndProc
    HINSTANCE   m_hInstance = nullptr;
    HWND        m_hwnd = nullptr;
    float       m_dpi_scale = 1.0f;  // 系统 DPI 缩放
    int         m_width = 1280;
    int         m_height = 760;

    // DX11 (public so static WndProc can access on same thread)
    ID3D11Device*           m_pd3dDevice        = nullptr;
    ID3D11DeviceContext*    m_pd3dDeviceContext = nullptr;
    IDXGISwapChain*         m_pSwapChain       = nullptr;
    ID3D11RenderTargetView* m_pRenderTarget    = nullptr;

private:
    // ----- Win32 host window + DX11 -----
    bool create_window(HINSTANCE hInstance);
    bool create_device();
    void cleanup_device();
    bool create_render_target();
    void cleanup_render_target();

    // ----- ImGui frame -----
    void new_frame();
    void end_frame();
    void apply_theme();

    // ----- UI panels -----
    void draw_topbar();
    void draw_left_categories();
    void draw_category_tree_node(const std::string& parent_name);
    void draw_center_gamelist();
    void draw_right_detail();
    void draw_statusbar();
    void draw_context_menu();
    void draw_settings_dialog();
    void draw_hidden_games_dialog();
    void draw_new_category_dialog();
    void draw_rename_category_dialog();
    void draw_remark_dialog();
    void draw_add_to_category_popup();
    void draw_remove_from_category_popup();
    void draw_batch_submenu_add();
    void draw_batch_submenu_remove();
    void draw_about_popup();
    void draw_confirm_popup();

    // ----- data layer -----
    void load_all();
    void save_all();
    void load_remarks();
    void save_remarks();
    void refresh_game_list();
    void update_status_text();
    void rebuild_filtered_indices();

    // ----- icon upload to ImGui -----
    void upload_icons_to_imgui();

    // ----- helpers -----
    ImTextureID get_or_upload_icon(uint32_t appid);
    void        show_toast(const std::string& msg, float duration = 2.0f);
    void        confirm(const char* title, const char* msg, std::function<void(bool)> cb);

    // Data layer (UTF-8)
    SteamScanner    m_scanner;
    CategoryManager m_categories;
    PlaytimeTracker m_playtime;
    IconLoader      m_icons;
    AppSettings     m_settings;
    // 备注: appid -> remark
    std::unordered_map<uint32_t, std::string> m_remarks;
    SteamWatcher    m_steam_watcher;
    DLLInjector     m_dll_injector;
    std::string     m_steam_path;
    std::string     m_data_dir;

    // Filter
    std::string m_filter_category;
    std::string m_filter_text;
    char        m_filter_text_buf[256] = {};

    // 排序状态
    int         m_sort_column = -1;
    int         m_pending_rightclick_idx = -1;
    std::string m_pending_cat_rightclick;  // 待打开右键菜单的分类名  // 待打开右键菜单的行       // -1 = 默认顺序
    bool        m_sort_ascending = true;  // true = 升序, false = 降序

    // UI state
    bool        m_batch_mode = false;
    std::set<int> m_batch_selected;  // indices into m_filtered_indices
    int         m_selected_game = -1;
    int         m_selected_category_node = -1;

    // Dialog state
    bool        m_show_settings = false;
    bool        m_show_remark = false;
    std::string m_new_cat_parent;
    std::set<std::string> m_expanded_cats;  // 展开的分类  // 新建分类时的父分类名(空=主分类)
    char        m_remark_buf[256] = {};
    uint32_t    m_ctx_remark_appid = 0;
    bool        m_show_hidden_dialog = false;
    bool        m_show_new_category = false;
    bool        m_show_rename_category = false;
    bool        m_show_about = false;
    bool        m_show_confirm = false;
    char        m_confirm_title[128] = {};
    char        m_confirm_msg[512] = {};
    bool        m_confirm_result = false;
    std::function<void(bool)> m_confirm_callback;

    // Cached for new category dialog
    char        m_new_category_name[128] = {};
    char        m_rename_category_buf[128] = {};
    std::string m_ctx_category_name;

    // Cached for filtered games
    std::vector<int> m_filtered_indices;
    bool m_filtered_dirty = true;

    // ImGui texture IDs for game icons (appid -> ImTextureID)
    std::unordered_map<uint32_t, ImTextureID> m_icon_textures;

    // Status / toast
    char        m_status_text[512] = {};
    char        m_steam_path_buf[MAX_PATH] = {};
    std::string m_toast_msg;
    float       m_toast_until = 0.0f;

    static constexpr int ICON_SIZE = 32;
};
