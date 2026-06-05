#include "gui_app.h"
#include <windowsx.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <functional>

namespace fs = std::filesystem;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ============================================================
//  Helper: get exe directory (UTF-8)
static std::string get_exe_dir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* last = wcsrchr(path, L'\\');
    if (last) *last = L'\0';
    char buf[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, MAX_PATH, nullptr, nullptr);
    std::string result(buf);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

static std::ofstream open_ofstream(const std::string& path_utf8, std::ios::openmode mode = std::ios::trunc) {
    int len = MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, nullptr, 0);
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, &wpath[0], len);
    wpath.resize(len - 1);
    return std::ofstream(wpath.c_str(), mode);
}

static std::ifstream open_ifstream(const std::string& path_utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, nullptr, 0);
    std::wstring wpath(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path_utf8.c_str(), -1, &wpath[0], len);
    wpath.resize(len - 1);
    return std::ifstream(wpath.c_str());
}

// ============================================================
//  Constructor / Destructor
SteamLauncherGUI::SteamLauncherGUI() = default;
SteamLauncherGUI::~SteamLauncherGUI() {
    cleanup_device();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

// ============================================================
//  Win32 message handler (ImGui hookup)
static SteamLauncherGUI* g_gui_instance = nullptr;
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    if (msg == WM_SIZE && g_gui_instance && g_gui_instance->m_pd3dDevice != nullptr) {
        if (wp != SIZE_MINIMIZED) {
            RECT rc; GetClientRect(hwnd, &rc);
            g_gui_instance->m_width  = rc.right - rc.left;
            g_gui_instance->m_height = rc.bottom - rc.top;
        }
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================
//  init
bool SteamLauncherGUI::init(HINSTANCE hInstance) {
    OutputDebugStringA("init: enter\n");
    m_hInstance = hInstance;
    m_data_dir  = get_exe_dir() + "/data";
    g_gui_instance = this;
    fs::create_directories(m_data_dir);
    OutputDebugStringA("init: data dir created\n");

    if (!create_window(hInstance)) { OutputDebugStringA("init: create_window failed\n"); return false; }
    OutputDebugStringA("init: window created\n");
    if (!create_device())       { OutputDebugStringA("init: create_device failed\n"); return false; }
    OutputDebugStringA("init: device created\n");

    // 读取系统 DPI, 用于字体缩放
    {
        HDC screen = GetDC(nullptr);
        int dpi = GetDeviceCaps(screen, LOGPIXELSX);  // 96 = 100%
        ReleaseDC(nullptr, screen);
        m_dpi_scale = (dpi > 96) ? (dpi / 96.0f) : 1.0f;
        char dbg[64]; snprintf(dbg, sizeof(dbg), "init: dpi=%d scale=%.2f\n", dpi, m_dpi_scale);
        OutputDebugStringA(dbg);
    }

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;  // no imgui.ini

    // 字体加载策略 (按优先级尝试):
    // 1. 项目自带的 luoliti.ttf (打包在 bin/ 目录, 用相对路径, 不依赖系统)
    // 2. Windows 系统自带的 msyh.ttc/msyh.ttf/simhei.ttf/segoeui.ttf (后备)
    // 优先用 GetGlyphRangesChineseFull() (覆盖全部 2 万 + 简体中文字符)
    // 如果失败降级到 ChineseSimplifiedCommon (3500 常用字)
    const char* font_candidates[] = {
        "fonts/luoliti.ttf",
        "luoliti.ttf",
        "../fonts/luoliti.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    bool font_loaded = false;
    for (auto p : font_candidates) {
        if (fs::exists(p)) {
            ImFontConfig cfg;
            cfg.OversampleH = 1;
            cfg.OversampleV = 1;
            cfg.PixelSnapH = true;
            // 字号: 18px (够大, 拖大窗口不糊)
            float font_size = 18.0f * m_dpi_scale;
            ImFont* f = io.Fonts->AddFontFromFileTTF(p, font_size, &cfg,
                io.Fonts->GetGlyphRangesChineseFull());
            if (f) {
                font_loaded = true;
                char dbg[512]; snprintf(dbg, sizeof(dbg), "init: font loaded ChineseFull from %s\n", p);
                OutputDebugStringA(dbg);
                break;
            }
            // 降级: ChineseSimplifiedCommon (3500 常用字)
            f = io.Fonts->AddFontFromFileTTF(p, 18.0f, &cfg,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            if (f) {
                font_loaded = true;
                OutputDebugStringA("init: font loaded Common 18px (fallback)\n");
                break;
            }
        }
    }
    if (!font_loaded) {
        ImFont* f = io.Fonts->AddFontDefault();
        if (!f) io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0f * m_dpi_scale);
    }
    OutputDebugStringA("init: font setup done\n");
    snprintf(m_steam_path_buf, sizeof(m_steam_path_buf), "%s", m_steam_path.c_str());

    // Setup platform/renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);

    apply_theme();

    // Load data layer
    m_settings = AppSettings::load(m_data_dir + "/settings.json");
    // 默认 appinfo 解析 (2), 仅当配置文件中是 0 时才修改
    if (m_settings.game_scan_method != 0 && m_settings.game_scan_method != 2) m_settings.game_scan_method = 2;
    if (!m_settings.steam_path.empty()) m_steam_path = m_settings.steam_path;
    else m_steam_path = SteamScanner::find_steam_path();

    if (m_settings.game_scan_method == 2) {
        m_scanner.scan_from_appinfo(m_settings.show_tools);
    } else {
        m_scanner.scan_all_games();
    }
    load_all();
    load_remarks();
    m_icons.init(m_steam_path);
    m_steam_watcher.set_steam_path(m_steam_path);
    upload_icons_to_imgui();
    refresh_game_list();
    update_status_text();

    ShowWindow(m_hwnd, SW_SHOWNORMAL);     // 通常窗口化 (16:9 默认), 作者喜爱
    UpdateWindow(m_hwnd);
    return true;
}

// ============================================================
//  Win32 window
bool SteamLauncherGUI::create_window(HINSTANCE hInstance) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SteamLauncherGUI_v2";
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        char buf[128]; snprintf(buf, sizeof(buf), "RegisterClassExW failed: %lu\n", err);
        OutputDebugStringA(buf);
        return false;
    }

    m_hwnd = CreateWindowExW(0, L"SteamLauncherGUI_v2",
        L"SteamLauncher - ImGui Edition",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, m_width, m_height,
        nullptr, nullptr, hInstance, nullptr);
    if (!m_hwnd) {
        DWORD err = GetLastError();
        char buf[128]; snprintf(buf, sizeof(buf), "CreateWindowExW failed: %lu\n", err);
        OutputDebugStringA(buf);
        return false;
    }
    return true;
}

// ============================================================
//  DX11 device + swap chain
bool SteamLauncherGUI::create_device() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = m_hwnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
    if (FAILED(hr)) return false;

    create_render_target();
    return true;
}

void SteamLauncherGUI::cleanup_device() {
    cleanup_render_target();
    if (m_pSwapChain)       { m_pSwapChain->Release();       m_pSwapChain       = nullptr; }
    if (m_pd3dDeviceContext){ m_pd3dDeviceContext->Release();m_pd3dDeviceContext= nullptr; }
    if (m_pd3dDevice)       { m_pd3dDevice->Release();       m_pd3dDevice       = nullptr; }
}

bool SteamLauncherGUI::create_render_target() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (FAILED(m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) return false;
    HRESULT hr = m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTarget);
    pBackBuffer->Release();
    return SUCCEEDED(hr);
}

void SteamLauncherGUI::cleanup_render_target() {
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
}

// ============================================================
//  ImGui theme (dark hacker style)
void SteamLauncherGUI::apply_theme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding     = ImVec2(12, 12);
    s.FramePadding      = ImVec2(16, 10);  // 更大的按钮
    s.ItemSpacing       = ImVec2(12, 8);
    s.ItemInnerSpacing  = ImVec2(8, 6);
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 8.0f;
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;
    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = Theme::Text();
    c[ImGuiCol_TextDisabled]         = Theme::TextDim();
    c[ImGuiCol_WindowBg]             = Theme::Bg();
    c[ImGuiCol_ChildBg]              = Theme::BgPanel();
    c[ImGuiCol_PopupBg]              = ImVec4(0.094f, 0.094f, 0.125f, 0.98f);
    c[ImGuiCol_Border]               = Theme::Border();
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = Theme::BgHeader();
    c[ImGuiCol_FrameBgHovered]       = Theme::Hover();
    c[ImGuiCol_FrameBgActive]        = Theme::Active();
    c[ImGuiCol_TitleBg]              = Theme::Bg();
    c[ImGuiCol_TitleBgActive]        = Theme::BgPanel();
    c[ImGuiCol_TitleBgCollapsed]     = Theme::Bg();
    c[ImGuiCol_MenuBarBg]            = Theme::BgPanel();
    c[ImGuiCol_ScrollbarBg]          = Theme::Bg();
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.30f, 0.36f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.50f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.65f, 1.0f);
    c[ImGuiCol_CheckMark]            = Theme::Accent2();
    c[ImGuiCol_SliderGrab]           = Theme::Accent();
    c[ImGuiCol_SliderGrabActive]     = Theme::Accent2();
    c[ImGuiCol_Button]               = Theme::BgHeader();
    c[ImGuiCol_ButtonHovered]        = Theme::Hover();
    c[ImGuiCol_ButtonActive]         = ImVec4(0.55f, 0.45f, 0.95f, 1.0f);
    c[ImGuiCol_Header]               = Theme::Hover();
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.20f, 0.30f, 1.0f);
    c[ImGuiCol_HeaderActive]         = Theme::Active();
    c[ImGuiCol_Separator]            = Theme::Border();
    c[ImGuiCol_SeparatorHovered]     = Theme::Accent();
    c[ImGuiCol_SeparatorActive]      = Theme::Accent2();
    c[ImGuiCol_ResizeGrip]           = Theme::Border();
    c[ImGuiCol_ResizeGripHovered]    = Theme::Accent();
    c[ImGuiCol_ResizeGripActive]     = Theme::Accent2();
    c[ImGuiCol_Tab]                  = Theme::BgHeader();
    c[ImGuiCol_TabHovered]           = Theme::Accent();
    c[ImGuiCol_TabActive]            = Theme::Accent2();
    c[ImGuiCol_TabUnfocused]         = Theme::BgHeader();
    c[ImGuiCol_TabUnfocusedActive]   = Theme::BgPanel();
    c[ImGuiCol_PlotLines]            = Theme::Accent();
    c[ImGuiCol_PlotLinesHovered]     = Theme::Accent2();
    c[ImGuiCol_PlotHistogram]        = Theme::Accent();
    c[ImGuiCol_PlotHistogramHovered] = Theme::Accent2();
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.25f, 0.25f, 0.50f, 0.6f);
    c[ImGuiCol_DragDropTarget]       = Theme::Accent2();
    c[ImGuiCol_NavHighlight]         = Theme::Accent2();
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.6f);
}

// ============================================================
//  main run loop
int SteamLauncherGUI::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        // Handle resize
        if (m_pd3dDevice && (m_width > 0 && m_height > 0)) {
            // (we recompute on WM_SIZE; the device needs no reset for our case)
        }

        // Render
        new_frame();
        draw_topbar();
        draw_left_categories();
        draw_center_gamelist();
        draw_right_detail();
        draw_statusbar();
        draw_context_menu();
        draw_settings_dialog();
        draw_hidden_games_dialog();
        draw_new_category_dialog();
        draw_rename_category_dialog();
        draw_add_to_category_popup();
        draw_remove_from_category_popup();
        draw_about_popup();
        draw_confirm_popup();
        draw_remark_dialog();
        end_frame();
    }
    return (int)msg.wParam;
}

void SteamLauncherGUI::new_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void SteamLauncherGUI::end_frame() {
    ImGui::Render();
    const float clear_color[4] = { 0.071f, 0.071f, 0.094f, 1.0f };
    m_pd3dDeviceContext->OMSetRenderTargets(1, &m_pRenderTarget, nullptr);
    m_pd3dDeviceContext->ClearRenderTargetView(m_pRenderTarget, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_pSwapChain->Present(1, 0);  // vsync
}

// ============================================================
//  top bar (toolbar)
void SteamLauncherGUI::draw_topbar() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)m_width, 64));
    ImGui::Begin("##topbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 0));

    // Search box
    static char search_buf[256] = {};
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputTextWithHint("##search", "\u641C\u7D22\u6E38\u620F...", search_buf, sizeof(search_buf),
        ImGuiInputTextFlags_EnterReturnsTrue)) {
        m_filter_text = search_buf;
        m_filtered_dirty = true;
    }
    if (strcmp(search_buf, m_filter_text.c_str()) != 0) {
        // also update if user pasted etc
    }
    // Sync filter
    m_filter_text = search_buf;
    if (m_filtered_dirty) refresh_game_list();

    ImGui::SameLine();
    if (ImGui::Button("\u5237\u65B0")) {  // 刷新
        if (m_settings.game_scan_method == 2) m_scanner.scan_from_appinfo(m_settings.show_tools);
        else m_scanner.scan_all_games();
        m_filtered_dirty = true;
        refresh_game_list();
        upload_icons_to_imgui();
        show_toast("\u5237\u65B0\u5B8C\u6210");
    }
    ImGui::SameLine();

    if (!m_batch_mode) {
        if (ImGui::Button("\u591A\u9009")) {  // 多选
            m_batch_mode = true;
            m_batch_selected.clear();
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, Theme::Accent());
        if (ImGui::Button("\u53D6\u6D88\u591A\u9009")) {  // 取消多选
            m_batch_mode = false;
            m_batch_selected.clear();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("\u5168\u9009")) {  // 全选
            m_batch_selected.clear();
            for (int i = 0; i < (int)m_filtered_indices.size(); ++i)
                m_batch_selected.insert(i);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("\u5220\u9664")) {  // 删除
            if (!m_batch_selected.empty()) {
                char buf[128]; snprintf(buf, sizeof(buf), "\u786E\u5B9A\u9690\u85CF %zu \u4E2A\u6E38\u620F\u5417?", m_batch_selected.size());
                m_show_confirm = true;
                snprintf(m_confirm_title, sizeof(m_confirm_title), "\u786E\u8BA4");
                snprintf(m_confirm_msg, sizeof(m_confirm_msg), "%s", buf);
                m_confirm_callback = [this](bool ok) {
                    if (ok) {
                        for (int i : m_batch_selected) {
                            if (i < 0 || i >= (int)m_filtered_indices.size()) continue;
                            int gi = m_filtered_indices[i];
                            if (gi < 0 || gi >= (int)m_scanner.games().size()) continue;
                            m_settings.hidden_appids.insert(m_scanner.games()[gi].appid);
                        }
                        m_settings.save(m_data_dir + "/settings.json");
                        m_batch_mode = false;
                        m_batch_selected.clear();
                        m_filtered_dirty = true;
                        refresh_game_list();
                        show_toast("\u5DF2\u9690\u85CF");
                    }
                };
            }
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("+ \u5206\u7C7B")) ImGui::OpenPopup("popup_add_cat_batch");  // + 分类
        if (ImGui::BeginPopup("popup_add_cat_batch")) { draw_batch_submenu_add(); ImGui::EndPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("- \u5206\u7C7B")) ImGui::OpenPopup("popup_remove_cat_batch");  // - 分类
        if (ImGui::BeginPopup("popup_remove_cat_batch")) { draw_batch_submenu_remove(); ImGui::EndPopup(); }
    }

    ImGui::SameLine();
    if (ImGui::Button("\u542F\u52A8")) {  // 启动
        if (m_selected_game >= 0 && m_selected_game < (int)m_filtered_indices.size()) {
            int gi = m_filtered_indices[m_selected_game];
            if (gi < (int)m_scanner.games().size()) {
                GameInfo* g = &m_scanner.games()[gi];
                int sid = m_playtime.start_session(g->appid);
                GameLauncher::smart_launch(*g, m_steam_path);
                m_playtime.end_session(sid);
                save_all();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("\u5206\u7C7B\u7BA1\u7406")) m_show_settings = false, ImGui::OpenPopup("popup_cat_manage");  // 分类管理
    if (ImGui::BeginPopup("popup_cat_manage")) {
        for (auto& c : m_categories.categories()) {
            char buf[128]; snprintf(buf, sizeof(buf), "\u5220\u9664: %s", c.name.c_str());
            if (ImGui::MenuItem(buf)) {
                m_categories.delete_category(c.name);
                save_all();
                m_filtered_dirty = true;
                refresh_game_list();
            }
        }
        if (m_categories.categories().empty()) ImGui::TextDisabled("( \u65E0\u5206\u7C7B )");
        ImGui::Separator();
        if (ImGui::MenuItem("\u65B0\u5EFA\u5206\u7C7B...")) {
            m_show_new_category = true;
            m_new_category_name[0] = '\0';
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("\u7EDF\u8BA1")) {  // 统计
        struct StatItem { uint32_t appid; int64_t total; std::string name; int sessions; };
        std::vector<StatItem> stats;
        for (const auto& g : m_scanner.games()) {
            int64_t t = m_playtime.get_total_playtime(g.appid);
            auto s = m_playtime.get_sessions_for_game(g.appid);
            if (t > 0) stats.push_back({g.appid, t, g.name, (int)s.size()});
        }
        std::sort(stats.begin(), stats.end(), [](auto& a, auto& b){ return a.total > b.total; });

        std::wostringstream woss;
        woss << L"\u3000\u3000\u3000\u3000\u3000\u3000\u3000\u3000\u3000\u3000\u6E38\u73A9\u65F6\u957F\u6392\u884C\n\n";
        for (size_t i = 0; i < stats.size() && i < 20; ++i) {
            int h = (int)(stats[i].total / 3600);
            int m = (int)((stats[i].total % 3600) / 60);
            woss << L"  " << (i+1) << L". " << to_wide(stats[i].name) << L"\n"
                 << L"     " << h << L"\u5C0F\u65F6" << m << L"\u5206  |  " << stats[i].sessions << L"\u6B21\u4F1A\u8BDD\n\n";
        }
        if (stats.empty()) woss << L"\u8FD8\u6CA1\u6709\u8FFD\u8E2A\u6570\u636E\u3002";
        MessageBoxW(m_hwnd, woss.str().c_str(), L"\u65F6\u957F\u7EDF\u8BA1", MB_OK | MB_ICONINFORMATION);
    }
    ImGui::SameLine();
    if (ImGui::Button("\u8BBE\u7F6E")) m_show_settings = true;  // 设置
    ImGui::SameLine();
    if (ImGui::Button("\u5DF2\u9690\u85CF")) m_show_hidden_dialog = true;  // 已隐藏

    // 右对齐
    ImGui::SameLine(ImGui::GetWindowWidth() - 90);
    if (ImGui::Button("\u5173\u4E8E")) m_show_about = true;  // 关于

    ImGui::PopStyleVar(2);
    ImGui::End();
}

// ============================================================
//  left panel: categories tree
void SteamLauncherGUI::draw_left_categories() {
    float left_w = m_width * 0.16f;  // 跟随窗口宽度 (1280->205, 1920->307)
    ImGui::SetNextWindowPos(ImVec2(0, 64));
    ImGui::SetNextWindowSize(ImVec2(left_w, (float)m_height - 64 - 32));
    ImGui::Begin("\u5206\u7C7B\u6811", nullptr,  // 分类树
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    // 全部游戏 / 已隐藏
    bool all_sel = m_filter_category.empty();
    if (ImGui::Selectable("[*] \u5168\u90E8\u6E38\u620F", all_sel, 0, ImVec2(0, 24))) {
        m_filter_category.clear();
        m_filtered_dirty = true;
        refresh_game_list();
    }
    if (ImGui::Selectable("[H]  \u5DF2\u9690\u85CF\u7684\u6E38\u620F", m_filter_category == "__hidden__", 0, ImVec2(0, 24))) {
        m_filter_category = "__hidden__";
        m_filtered_dirty = true;
        refresh_game_list();
    }
    ImGui::Separator();

    // 树形分类
    draw_category_tree_node("");  // root

    // 右键空白处
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        // 如果点中的不是分类项, 打开空白处菜单
        if (!ImGui::IsAnyItemHovered()) {
            m_ctx_category_name.clear();
            ImGui::OpenPopup("popup_cat_blank");
        }
    }
    if (ImGui::BeginPopup("popup_cat_blank")) {
        if (ImGui::MenuItem("\u65B0\u5EFA\u4E3B\u5206\u7C7B...")) {  // 新建主分类
            m_show_new_category = true;
            m_new_category_name[0] = '\0';
            m_new_cat_parent.clear();  // 主分类
        }
        if (ImGui::MenuItem("\u65B0\u5EFA\u9876\u7EA7\u5206\u7C7B...")) {  // 新建顶级分类
            m_show_new_category = true;
            m_new_category_name[0] = '\0';
            m_new_cat_parent.clear();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

// 递归绘制分类树
void SteamLauncherGUI::draw_category_tree_node(const std::string& parent_name) {
    int idx = 0;
    for (auto& c : m_categories.categories()) {
        if (c.parent != parent_name) continue;
        ImGui::PushID(idx);
        // 计算子分类数
        int child_count = 0;
        for (auto& c2 : m_categories.categories()) if (c2.parent == c.name) ++child_count;

        bool is_current = (m_filter_category == c.name);
        bool is_expanded = (m_expanded_cats.count(c.name) > 0);
        const char* mark = is_current ? "[*]" : "[ ]";
        const char* arrow = (child_count > 0) ? (is_expanded ? "[-]" : "[+]") : "   ";

        char label[256];
        snprintf(label, sizeof(label), "%s %s %s (%zu)", mark, arrow, c.name.c_str(), c.game_ids.size());

        // 用 Selectable. 选中后用 InvisibleButton 覆盖抓右键.
        // 重要: Selectable 高度是 ImGui::GetFrameHeight(), 之后用
        // SameLine 绘制文字. InvisibleButton 在 Selectable 之上.
        ImGui::Selectable(label, is_current,
            ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowDoubleClick);
        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                m_ctx_category_name = c.name;
                m_pending_cat_rightclick = c.name;
            }
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && child_count > 0) {
                if (is_expanded) m_expanded_cats.erase(c.name);
                else m_expanded_cats.insert(c.name);
            }
        }
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            // 单击
            m_filter_category = c.name;
            m_filtered_dirty = true;
            refresh_game_list();
            if (child_count > 0) m_expanded_cats.insert(c.name);
        }

        // 展开: 递归绘制子分类
        if (is_expanded && child_count > 0) {
            ImGui::Indent(16.0f);
            draw_category_tree_node(c.name);
            ImGui::Unindent(16.0f);
        }
        ImGui::PopID();
        ++idx;
    }

    // 右键菜单: 用 m_pending_cat_rightclick 触发 (确保下一帧)
    if (!m_pending_cat_rightclick.empty()) {
        ImGui::OpenPopup("popup_cat_item");
        m_pending_cat_rightclick.clear();
    }
    if (ImGui::BeginPopup("popup_cat_item")) {
        if (!m_ctx_category_name.empty()) {
            if (ImGui::MenuItem("\u65B0\u5EFA\u5B50\u5206\u7C7B...")) {
                m_show_new_category = true;
                m_new_cat_parent = m_ctx_category_name;
                m_new_category_name[0] = '\0';
                // 父级自动展开以便用户看到子分类
                m_expanded_cats.insert(m_ctx_category_name);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("\u91CD\u547D\u540D...")) {
                m_show_rename_category = true;
                snprintf(m_rename_category_buf, sizeof(m_rename_category_buf), "%s", m_ctx_category_name.c_str());
            }
            if (ImGui::MenuItem("\u5220\u9664...")) {
                m_show_confirm = true;
                snprintf(m_confirm_title, sizeof(m_confirm_title), "\u786E\u8BA4\u5220\u9664\u5206\u7C7B");
                snprintf(m_confirm_msg, sizeof(m_confirm_msg), "\u786E\u5B9A\u5220\u9664\u5206\u7C7B \"%s\" ?", m_ctx_category_name.c_str());
                m_confirm_callback = [this](bool ok) {
                    if (ok) {
                        std::string n = m_ctx_category_name;
                        m_categories.delete_category(n);
                        m_ctx_category_name.clear();
                        m_expanded_cats.erase(n);
                        save_all();
                        m_filtered_dirty = true;
                        refresh_game_list();
                    }
                };
            }
        }
        ImGui::EndPopup();
    }
}
// ============================================================
//  center: game list
void SteamLauncherGUI::draw_center_gamelist() {
    float left_w = m_width * 0.16f;  // 跟随窗口宽度 (1280->205, 1920->307)
    float right_w = m_width * 0.20f;
    ImGui::SetNextWindowPos(ImVec2(left_w, 64));
    ImGui::SetNextWindowSize(ImVec2((float)m_width - left_w - right_w, (float)m_height - 64 - 32));
    ImGui::Begin("\u6E38\u620F\u5217\u8868", nullptr,  // 游戏列表
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    // table
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 6));
    int flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    // 注意: 始终 5 列 (icon+名称 / AppID / 时长 / 状态 / 最后启动)
    // 批量模式的多选框用第一列里嵌一个 Checkbox, 不增加列数
    if (ImGui::BeginTable("##gamelist", 6, flags)) {
        // 表头: 不使用 ImGui 默认字符 (msyh 字体不支持 ↑↓), 自己画
        ImGui::TableSetupColumn("\u540D\u79F0", ImGuiTableColumnFlags_WidthStretch, 2.6f);
        ImGui::TableSetupColumn("AppID",        ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("\u65F6\u957F", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("\u6210\u5C31", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("\u72B6\u6001", ImGuiTableColumnFlags_WidthStretch, 0.8f);
        ImGui::TableSetupColumn("\u6700\u540E\u542F\u52A8", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableHeadersRow();
        // 排序: 点击表头切换 (用 TableSetColumnIndex + IsItemClicked)
        for (int col = 0; col < 6; ++col) {
            ImGui::TableSetColumnIndex(col);
            char sort_arrow = ' ';
            if (m_sort_column == col) sort_arrow = m_sort_ascending ? '^' : 'v';
            // 不可见 button 覆盖表头区用来抓点击
            char hdr_id[16]; snprintf(hdr_id, sizeof(hdr_id), "##hdr%d", col);
            if (ImGui::InvisibleButton(hdr_id, ImVec2(ImGui::GetColumnWidth(), ImGui::GetFrameHeight()))) {
                if (m_sort_column == col) m_sort_ascending = !m_sort_ascending;
                else { m_sort_column = col; m_sort_ascending = true; }
                m_filtered_dirty = true;
                refresh_game_list();
            }
            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        for (int i = 0; i < (int)m_filtered_indices.size(); ++i) {
            int gi = m_filtered_indices[i];
            if (gi < 0 || gi >= (int)m_scanner.games().size()) continue;
            const GameInfo& g = m_scanner.games()[gi];
            ImGui::TableNextRow();

            // 第 0 列: 图标 + (批量模式时)多选框 + 名称
            ImGui::TableSetColumnIndex(0);
            if (m_batch_mode) {
                bool checked = m_batch_selected.count(i) > 0;
                if (ImGui::Checkbox(("##sel_" + std::to_string(i)).c_str(), &checked)) {
                    if (checked) m_batch_selected.insert(i);
                    else m_batch_selected.erase(i);
                }
                ImGui::SameLine();
            }
            ImTextureID tex = get_or_upload_icon(g.appid);
            if (tex) {
                ImGui::Image(tex, ImVec2(24, 24));
            } else {
                ImGui::Dummy(ImVec2(24, 24));
            }
            ImGui::SameLine();
            bool is_sel = (m_selected_game == i);
            // 显示带备注的名称
            std::string display_name = g.name;
            auto rit = m_remarks.find(g.appid);
            if (rit != m_remarks.end() && !rit->second.empty()) {
                display_name = "[" + rit->second + "] " + g.name;
            }
            if (ImGui::Selectable(display_name.c_str(), is_sel,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                m_selected_game = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    int sid = m_playtime.start_session(g.appid);
                    GameLauncher::smart_launch(g, m_steam_path);
                    m_playtime.end_session(sid);
                    save_all();
                }
            }
            // 右键: 在 Selectable 区域(整个行) 上点右键
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                m_selected_game = i;
                m_pending_rightclick_idx = i;
            }
            

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", g.appid);

            ImGui::TableSetColumnIndex(2);
            int64_t pt = g.total_playtime_min;
            if (pt > 0) {
                int h = (int)(pt / 60);
                int m = (int)(pt % 60);
                ImGui::Text("%dh %dm", h, m);
            } else {
                ImGui::TextDisabled("-");
            }

                        // 第 3 列: 成就数 (nAchieved/nTotal)
            ImGui::TableSetColumnIndex(3);
            if (g.achievements_total > 0) {
                ImGui::TextColored(Theme::Accent(), "%d/%d",
                    g.achievements_unlocked, g.achievements_total);
            } else {
                ImGui::TextDisabled("-");
            }

            // 第 4 列: 安装状态
            ImGui::TableSetColumnIndex(4);
            bool installed = !g.install_dir.empty();
            if (installed) {
                ImGui::TextColored(Theme::Green(), "\u5DF2\u5B89\u88C5");  // 已安装
            } else {
                ImGui::TextColored(Theme::Red(), "\u672A\u5B89\u88C5");  // 未安装
            }

            ImGui::TableSetColumnIndex(5);
            if (g.last_played > 0) {
                // 格式化为 YYYY-MM-DD HH:MM
                time_t t = (time_t)g.last_played;
                struct tm tm_buf;
                localtime_s(&tm_buf, &t);
                char date_str[32];
                snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %02d:%02d",
                    tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                    tm_buf.tm_hour, tm_buf.tm_min);
                ImGui::TextUnformatted(date_str);
            } else {
                ImGui::TextDisabled("-");
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

// ============================================================
//  right: detail panel
void SteamLauncherGUI::draw_right_detail() {
    float left_w = m_width * 0.16f;  // 跟随窗口宽度 (1280->205, 1920->307)
    float right_w = m_width * 0.20f;
    ImGui::SetNextWindowPos(ImVec2((float)m_width - right_w, 64));
    ImGui::SetNextWindowSize(ImVec2(right_w, (float)m_height - 64 - 32));
    ImGui::Begin("\u8BE6\u60C5", nullptr,  // 详情
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    if (m_selected_game >= 0 && m_selected_game < (int)m_filtered_indices.size()) {
        int gi = m_filtered_indices[m_selected_game];
        if (gi >= 0 && gi < (int)m_scanner.games().size()) {
            const GameInfo& g = m_scanner.games()[gi];
            ImTextureID tex = get_or_upload_icon(g.appid);
            if (tex) {
                ImGui::Image(tex, ImVec2(64, 64));
            } else {
                ImGui::Dummy(ImVec2(64, 64));
            }
            ImGui::TextWrapped("%s", g.name.c_str());
            ImGui::Separator();
            ImGui::TextDisabled("AppID: %u", g.appid);
            if (!g.install_dir.empty()) {
                ImGui::TextDisabled("\u5B89\u88C5\u76EE\u5F55: %s", g.install_dir.c_str());  // 安装目录
            }
            // 游戏时长 (从 localconfig.vdf 读 Steam 官方数据, 每个游戏独立)
            int64_t pt = g.total_playtime_min;
            if (pt > 0) {
                int h = (int)(pt / 60);
                int m = (int)(pt % 60);
                ImGui::TextDisabled("\u603B\u65F6\u957F: %d\u5C0F\u65F6%d\u5206 (%d \u5206\u949F)", h, m, (int)pt);
            } else {
                ImGui::TextDisabled("\u603B\u65F6\u957F: -");
            }
            // 离线游玩时长 (Steam 离线模式累计)
            if (g.playtime_disconnected_min > 0) {
                int h = (int)(g.playtime_disconnected_min / 60);
                int m = (int)(g.playtime_disconnected_min % 60);
                ImGui::TextDisabled("\u79BB\u7EBF\u65F6\u957F: %d\u5C0F\u65F6%d\u5206", h, m);
            }
            // 最后启动时间 (Unix timestamp 转本地时间)
            if (g.last_played > 0) {
                time_t t = (time_t)g.last_played;
                char buf[64];
                struct tm tm_buf;
                localtime_s(&tm_buf, &t);
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm_buf);
                ImGui::TextDisabled("\u6700\u540E\u542F\u52A8: %s", buf);
            }
            // 成就数 (nAchieved/nTotal)
            if (g.achievements_total > 0) {
                ImGui::TextColored(Theme::Accent(), "\u6210\u5C31: %d/%d",
                    g.achievements_unlocked, g.achievements_total);
            }
            auto cats = m_categories.get_categories_for_game(g.appid);
            if (!cats.empty()) {
                ImGui::TextDisabled("\u5206\u7C7B:");  // 分类
                ImGui::SameLine();
                for (size_t k = 0; k < cats.size(); ++k) {
                    if (k) ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, Theme::Accent());
                    ImGui::SmallButton(cats[k].c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
    } else {
        ImGui::TextDisabled("\u9009\u62E9\u4E00\u4E2A\u6E38\u620F\u67E5\u770B\u8BE6\u60C5");  // 选择一个游戏查看详情
    }
    ImGui::End();
}

// ============================================================
//  status bar
void SteamLauncherGUI::draw_statusbar() {
    ImGui::SetNextWindowPos(ImVec2(0, (float)m_height - 32));
    ImGui::SetNextWindowSize(ImVec2((float)m_width, 32));
    ImGui::Begin("##status", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::Text("%s", m_status_text);

    // 右侧: 全部游戏总时长 + 滑条 (相对最长游戏)
    int64_t total_min = 0;
    for (const auto& g : m_scanner.games()) {
        total_min += g.total_playtime_min;
    }
    int64_t max_min = 1;
    for (const auto& g : m_scanner.games()) {
        if (g.total_playtime_min > max_min) max_min = g.total_playtime_min;
    }
    int total_h = (int)(total_min / 60);
    int total_m = (int)(total_min % 60);
    char total_str[64];
    snprintf(total_str, sizeof(total_str), "总时长: %dh %dm", total_h, total_m);

    float slider_w = 200.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - slider_w - 220);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Theme::Accent2());
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.15f, 1.0f));
    ImGui::ProgressBar((float)total_min / (float)max_min, ImVec2(slider_w, 14), total_str);
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("总时长 (相对最长游戏: %lldh %lldm)",
            max_min / 60, max_min % 60);
    }

    // Toast notification
    if (!m_toast_msg.empty() && ImGui::GetTime() < m_toast_until) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Accent2());
        ImGui::Text("[i] %s", m_toast_msg.c_str());  // ✨
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

// ============================================================
//  context menu (right-click on game)
void SteamLauncherGUI::draw_context_menu() {
    if (m_pending_rightclick_idx >= 0) {
        ImGui::OpenPopup("popup_game_context");
        m_pending_rightclick_idx = -1;
    }

    if (ImGui::BeginPopup("popup_game_context")) {
        bool has_sel = (m_selected_game >= 0 && m_selected_game < (int)m_filtered_indices.size());
        if (ImGui::MenuItem("\u542F\u52A8", nullptr, nullptr, has_sel)) {  // 启动
            if (has_sel) {
                int gi = m_filtered_indices[m_selected_game];
                if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                    const GameInfo& g = m_scanner.games()[gi];
                    int sid = m_playtime.start_session(g.appid);
                    GameLauncher::smart_launch(g, m_steam_path);
                    m_playtime.end_session(sid);
                    save_all();
                }
            }
        }
        if (ImGui::MenuItem("\u6253\u5F00 Steam \u5546\u5E97", nullptr, nullptr, has_sel)) {
            if (has_sel) {
                int gi = m_filtered_indices[m_selected_game];
                if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                    std::string url = "https://store.steampowered.com/app/" + std::to_string(m_scanner.games()[gi].appid);
                    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
        }
        if (ImGui::MenuItem("\u6253\u5F00\u6587\u4EF6\u5939", nullptr, nullptr, has_sel)) {
            if (has_sel) {
                int gi = m_filtered_indices[m_selected_game];
                if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                    const GameInfo& g = m_scanner.games()[gi];
                    if (!g.install_dir.empty()) {
                        std::string game_path = g.library_path + "/common/" + g.install_dir + "/";
                        std::replace(game_path.begin(), game_path.end(), '/', '\\');
                        ShellExecuteA(nullptr, "open", "explorer.exe", game_path.c_str(), nullptr, SW_SHOWNORMAL);
                    }
                }
            }
        }
        ImGui::Separator();
        // 添加到分类
        if (ImGui::BeginMenu("\u52A0\u5165\u5206\u7C7B")) {  // 加入分类
            for (auto& c : m_categories.categories()) {
                if (ImGui::MenuItem(c.name.c_str())) {
                    if (has_sel) {
                        int gi = m_filtered_indices[m_selected_game];
                        if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                            m_categories.add_game_to_category(m_scanner.games()[gi].appid, c.name);
                            save_all();
                            m_filtered_dirty = true;
                            refresh_game_list();
                        }
                    }
                }
            }
            if (m_categories.categories().empty()) ImGui::TextDisabled("( \u65E0\u5206\u7C7B )");
            if (ImGui::MenuItem("\u65B0\u5EFA\u5206\u7C7B...")) {
                m_show_new_category = true; m_new_category_name[0] = '\0';
            }
            ImGui::EndMenu();
        }
        // 从分类移出
        if (ImGui::BeginMenu("\u79FB\u51FA\u5206\u7C7B")) {
            if (has_sel) {
                int gi = m_filtered_indices[m_selected_game];
                if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                    auto cats = m_categories.get_categories_for_game(m_scanner.games()[gi].appid);
                    for (auto& cn : cats) {
                        if (ImGui::MenuItem(cn.c_str())) {
                            m_categories.remove_game_from_category(m_scanner.games()[gi].appid, cn);
                            save_all();
                            m_filtered_dirty = true;
                            refresh_game_list();
                        }
                    }
                    if (cats.empty()) ImGui::TextDisabled("( \u65E0 )");
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        // 备注
        if (ImGui::MenuItem("\u5907\u6CE8...", nullptr, nullptr, has_sel)) {  // 备注...
            if (has_sel) {
                int gi = m_filtered_indices[m_selected_game];
                if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                    uint32_t appid = m_scanner.games()[gi].appid;
                    auto it = m_remarks.find(appid);
                    std::string current = (it != m_remarks.end()) ? it->second : "";
                    // 复用重命名对话框作为备注对话框
                    m_show_remark = true;
                    snprintf(m_remark_buf, sizeof(m_remark_buf), "%s", current.c_str());
                    m_ctx_remark_appid = appid;
                }
            }
        }
        // 删除(隐藏)游戏
        // 在"已隐藏"视图下显示"恢复显示", 其他视图显示"隐藏这个游戏"
        if (m_filter_category == "__hidden__") {
            if (ImGui::MenuItem("\u6062\u590D\u663E\u793A", nullptr, nullptr, has_sel)) {
                if (has_sel) {
                    int gi = m_filtered_indices[m_selected_game];
                    if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                        m_settings.hidden_appids.erase(m_scanner.games()[gi].appid);
                        m_settings.save(m_data_dir + "/settings.json");
                        m_filtered_dirty = true;
                        refresh_game_list();
                        show_toast("\u5DF2\u6062\u590D");
                    }
                }
            }
        } else {
            if (ImGui::MenuItem("\u9690\u85CF\u8FD9\u4E2A\u6E38\u620F", nullptr, nullptr, has_sel)) {
                if (has_sel) {
                    int gi = m_filtered_indices[m_selected_game];
                    if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                        m_settings.hidden_appids.insert(m_scanner.games()[gi].appid);
                        m_settings.save(m_data_dir + "/settings.json");
                        m_filtered_dirty = true;
                        refresh_game_list();
                        show_toast("\u5DF2\u9690\u85CF");
                    }
                }
            }
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_new_category_dialog() {
    if (m_show_new_category) {
        ImGui::OpenPopup("\u65B0\u5EFA\u5206\u7C7B");
    }
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("\u65B0\u5EFA\u5206\u7C7B", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // 用专门的 m_new_cat_parent 而不是 m_ctx_category_name
        // (m_ctx_category_name 在其他场景会被重设, 会污染新建逻辑)
        static std::string s_parent = "";
        if (m_show_new_category) {
            // 刚被打开, 同步 parent
            s_parent = m_new_cat_parent;
        }
        if (!s_parent.empty()) {
            ImGui::Text("\u5C06\u4F5C\u4E3A \"%s\" \u7684\u5B50\u5206\u7C7B", s_parent.c_str());
        } else {
            ImGui::TextDisabled("\u5C06\u521B\u5EFA\u4E3B\u5206\u7C7B");
        }
        ImGui::InputText("\u540D\u79F0", m_new_category_name, sizeof(m_new_category_name));
        if (ImGui::Button("\u786E\u5B9A", ImVec2(140, 0))) {
            std::string name = m_new_category_name;
            if (!name.empty()) {
                bool ok = m_categories.create_category(name, s_parent);
                if (ok) {
                    save_all();
                    m_filtered_dirty = true;
                    refresh_game_list();
                    m_show_new_category = false;
                    m_new_cat_parent.clear();
                    m_new_category_name[0] = '\0';
                    s_parent.clear();
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("\u53D6\u6D88", ImVec2(140, 0))) {
            m_show_new_category = false;
            m_new_cat_parent.clear();
            m_new_category_name[0] = '\0';
            s_parent.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_rename_category_dialog() {
    if (m_show_rename_category) {
        ImGui::OpenPopup("\u91CD\u547D\u540D");
    }
    ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("\u91CD\u547D\u540D", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("\u65B0\u540D\u79F0", m_rename_category_buf, sizeof(m_rename_category_buf));
        if (ImGui::Button("\u786E\u5B9A", ImVec2(120, 0))) {
            std::string new_name = m_rename_category_buf;
            if (!new_name.empty() && new_name != m_ctx_category_name) {
                m_categories.rename_category(m_ctx_category_name, new_name);
                save_all();
                m_filtered_dirty = true;
                refresh_game_list();
            }
            m_show_rename_category = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("\u53D6\u6D88", ImVec2(120, 0))) {
            m_show_rename_category = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_remark_dialog() {
    if (m_show_remark) {
        ImGui::OpenPopup("\u5907\u6CE8");  // 备注
    }
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("\u5907\u6CE8", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("\u6E38\u620F\u540D\u5C06\u663E\u793A\u4E3A: [\u5907\u6CE8\u5185\u5BB9]\u6E38\u620F\u540D");
        ImGui::InputText("##remark", m_remark_buf, sizeof(m_remark_buf));
        if (ImGui::Button("\u4FDD\u5B58", ImVec2(120, 0))) {
            std::string s = m_remark_buf;
            if (!s.empty()) {
                m_remarks[m_ctx_remark_appid] = s;
            } else {
                m_remarks.erase(m_ctx_remark_appid);
            }
            save_remarks();
            m_filtered_dirty = true;
            refresh_game_list();
            m_show_remark = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("\u53D6\u6D88", ImVec2(120, 0))) {
            m_show_remark = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_add_to_category_popup() {}
void SteamLauncherGUI::draw_remove_from_category_popup() {}

void SteamLauncherGUI::draw_batch_submenu_add() {
    for (auto& c : m_categories.categories()) {
        if (ImGui::MenuItem(c.name.c_str())) {
            for (int i : m_batch_selected) {
                if (i >= 0 && i < (int)m_filtered_indices.size()) {
                    int gi = m_filtered_indices[i];
                    if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                        m_categories.add_game_to_category(m_scanner.games()[gi].appid, c.name);
                    }
                }
            }
            save_all();
            m_filtered_dirty = true;
            refresh_game_list();
        }
    }
    if (m_categories.categories().empty()) ImGui::TextDisabled("( \u65E0\u5206\u7C7B )");
    ImGui::Separator();
    if (ImGui::MenuItem("\u65B0\u5EFA\u5206\u7C7B...")) {  // 新建分类
        m_show_new_category = true; m_new_category_name[0] = '\0';
    }
}

void SteamLauncherGUI::draw_batch_submenu_remove() {
    if (m_batch_selected.empty()) { ImGui::TextDisabled("( \u8BF7\u5148\u52FE\u9009\u6E38\u620F )"); return; }
    // 共同分类
    std::vector<std::string> common;
    bool first = true;
    for (int i : m_batch_selected) {
        if (i < 0 || i >= (int)m_filtered_indices.size()) continue;
        int gi = m_filtered_indices[i];
        if (gi < 0 || gi >= (int)m_scanner.games().size()) continue;
        auto cats = m_categories.get_categories_for_game(m_scanner.games()[gi].appid);
        if (first) { common = cats; first = false; }
        else {
            std::vector<std::string> inter;
            for (auto& cn : common) {
                if (std::find(cats.begin(), cats.end(), cn) != cats.end()) inter.push_back(cn);
            }
            common = inter;
        }
    }
    for (auto& cn : common) {
        if (ImGui::MenuItem(cn.c_str())) {
            for (int i : m_batch_selected) {
                if (i < 0 || i >= (int)m_filtered_indices.size()) continue;
                int gi = m_filtered_indices[i];
                if (gi >= 0 && gi < (int)m_scanner.games().size()) {
                    m_categories.remove_game_from_category(m_scanner.games()[gi].appid, cn);
                }
            }
            save_all();
            m_filtered_dirty = true;
            refresh_game_list();
        }
    }
    if (common.empty()) ImGui::TextDisabled("( \u65E0\u5171\u540C\u5206\u7C7B )");
}

void SteamLauncherGUI::draw_settings_dialog() {
    if (m_show_settings) {
        ImGui::OpenPopup("\u8BBE\u7F6E");
    }
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("\u8BBE\u7F6E", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("\u8DEF\u5F84\u8BBE\u7F6E");  // 路径设置
        ImGui::Separator();

        // Steam 路径
        ImGui::PushItemWidth(320);
        ImGui::InputText("Steam \u8DEF\u5F84", m_steam_path_buf, sizeof(m_steam_path_buf));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("\u81EA\u52A8\u83B7\u53D6", ImVec2(100, 0))) {  // 自动获取
            std::string auto_path = SteamScanner::find_steam_path();
            if (!auto_path.empty()) {
                snprintf(m_steam_path_buf, sizeof(m_steam_path_buf), "%s", auto_path.c_str());
            } else {
                show_toast("\u672A\u68C0\u6D4B\u5230 Steam");
            }
        }
        if (ImGui::Button("\u5E94\u7528\u8DEF\u5F84", ImVec2(120, 0))) {  // 应用路径
            if (strcmp(m_steam_path_buf, m_steam_path.c_str()) != 0) {
                m_steam_path = m_steam_path_buf;
                m_settings.steam_path = m_steam_path;
                m_settings.save(m_data_dir + "/settings.json");
                m_steam_watcher.set_steam_path(m_steam_path);
                m_icons.init(m_steam_path);
                show_toast("\u8DEF\u5F84\u5DF2\u66F4\u65B0");
            }
        }

        ImGui::Separator();
        ImGui::Text("\u626B\u63CF\u65B9\u5F0F");  // 扫描方式
        // 0 = 检测游戏文件夹 (无法检测未安装的游戏)  2 = appinfo 解析
        int sm = m_settings.game_scan_method;
        if (ImGui::RadioButton("\u68C0\u6D4B\u6E38\u620F\u6587\u4EF6\u5939 (\u4E0D\u542B\u672A\u5B89\u88C5)", sm == 0)) m_settings.game_scan_method = 0;
        if (ImGui::RadioButton("appinfo \u89E3\u6790", sm == 2)) m_settings.game_scan_method = 2;
        ImGui::Checkbox("\u663E\u793A\u5DE5\u5177\u578B\u6E38\u620F", &m_settings.show_tools);
        ImGui::Separator();
        if (ImGui::Button("\u4FDD\u5B58\u5E76\u91CD\u65B0\u626B\u63CF", ImVec2(180, 0))) {
            m_settings.save(m_data_dir + "/settings.json");
            // 用设置中的方法扫描
            if (m_settings.game_scan_method == 2) m_scanner.scan_from_appinfo(m_settings.show_tools);
            else m_scanner.scan_all_games();  // 0 = 检测游戏文件夹
            m_filtered_dirty = true;
            refresh_game_list();
            upload_icons_to_imgui();
            m_show_settings = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("\u53D6\u6D88", ImVec2(120, 0))) {
            m_show_settings = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_hidden_games_dialog() {
    if (m_show_hidden_dialog) {
        ImGui::OpenPopup("\u5DF2\u9690\u85CF\u7684\u6E38\u620F");  // 已隐藏的游戏
    }
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("\u5DF2\u9690\u85CF\u7684\u6E38\u620F", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::vector<uint32_t> hidden(m_settings.hidden_appids.begin(), m_settings.hidden_appids.end());
        for (uint32_t id : hidden) {
            std::string name = std::to_string(id);
            if (auto* g = m_scanner.find_game(id)) name = g->name;
            ImGui::BulletText("%s (AppID %u)", name.c_str(), id);
            ImGui::SameLine();
            char buf[64]; snprintf(buf, sizeof(buf), "\u6062\u590D##%u", id);  // 恢复
            if (ImGui::SmallButton(buf)) {
                m_settings.hidden_appids.erase(id);
                m_settings.save(m_data_dir + "/settings.json");
                m_filtered_dirty = true;
                refresh_game_list();
            }
        }
        if (hidden.empty()) ImGui::TextDisabled("( \u65E0 )");
        ImGui::Separator();
        if (!hidden.empty()) {
            if (ImGui::Button("\u6E05\u7A7A\u5168\u90E8")) {  // 清空全部
                m_settings.hidden_appids.clear();
                m_settings.save(m_data_dir + "/settings.json");
                m_filtered_dirty = true;
                refresh_game_list();
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("\u5173\u95ED", ImVec2(120, 0))) {  // 关闭
            m_show_hidden_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_about_popup() {
    if (m_show_about) {
        ImGui::OpenPopup("\u5173\u4E8E");  // 关于
    }
    if (ImGui::BeginPopupModal("\u5173\u4E8E", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Steam \u6E38\u620F\u542F\u52A8\u5668 v1.0");  // Steam 游戏启动器 v1.0
        ImGui::Text("\u57FA\u4E8E ImGui + DirectX 11 \u91CD\u6784");  // 基于 ImGui + DirectX 11 重构
        ImGui::Text("\u539F\u9879\u76EE\u4FDD\u7559\u6240\u6709\u529F\u80FD");  // 原项目保留所有功能
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            m_show_about = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SteamLauncherGUI::draw_confirm_popup() {
    if (m_show_confirm) {
        ImGui::OpenPopup(m_confirm_title);
    }
    if (ImGui::BeginPopupModal(m_confirm_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", m_confirm_msg);
        if (ImGui::Button("\u786E\u5B9A", ImVec2(120, 0))) {  // 确定
            m_confirm_result = true;
            if (m_confirm_callback) m_confirm_callback(true);
            m_confirm_callback = nullptr;
            m_show_confirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("\u53D6\u6D88", ImVec2(120, 0))) {  // 取消
            m_confirm_result = false;
            if (m_confirm_callback) m_confirm_callback(false);
            m_confirm_callback = nullptr;
            m_show_confirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================
//  data layer
void SteamLauncherGUI::load_all() {
    std::string cat_path = m_data_dir + "/categories.json";
    std::ifstream f = open_ifstream(cat_path);
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    size_t pos = 0;
    auto skip_ws = [](const std::string& s, size_t& p) {
        while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p;
    };
    auto read_str = [&](const std::string& s, size_t& p) -> std::string {
        if (p >= s.size() || s[p] != '"') return "";
        ++p; std::string v;
        while (p < s.size() && s[p] != '"') { v += s[p]; ++p; }
        if (p < s.size()) ++p;
        return v;
    };
    skip_ws(content, pos);
    if (pos < content.size() && content[pos] == '{') ++pos;
    while (pos < content.size()) {
        skip_ws(content, pos);
        if (content[pos] == '}') break;
        std::string key = read_str(content, pos);
        if (key.empty()) break;
        skip_ws(content, pos);
        if (pos < content.size() && content[pos] == ':') ++pos;
        skip_ws(content, pos);
        if (key == "categories" && content[pos] == '[') {
            ++pos;
            while (pos < content.size()) {
                skip_ws(content, pos);
                if (content[pos] == ']') { ++pos; break; }
                if (content[pos] == ',') { ++pos; continue; }
                if (content[pos] == '{') {
                    ++pos;
                    Category cat;
                    while (pos < content.size()) {
                        skip_ws(content, pos);
                        if (content[pos] == '}') { ++pos; break; }
                        if (content[pos] == ',') { ++pos; continue; }
                        std::string fld = read_str(content, pos);
                        skip_ws(content, pos);
                        if (pos < content.size() && content[pos] == ':') ++pos;
                        skip_ws(content, pos);
                        if (fld == "name") cat.name = read_str(content, pos);
                        else if (fld == "games" && content[pos] == '[') {
                            ++pos;
                            while (pos < content.size()) {
                                skip_ws(content, pos);
                                if (content[pos] == ']') { ++pos; break; }
                                if (content[pos] == ',') { ++pos; continue; }
                                std::string n;
                                while (pos < content.size() && (isdigit(content[pos]))) { n += content[pos]; ++pos; }
                                if (!n.empty()) cat.game_ids.insert((uint32_t)std::stoi(n));
                            }
                        }
                    }
                    if (!cat.name.empty()) m_categories.create_category(cat.name);
                    for (uint32_t id : cat.game_ids) m_categories.add_game_to_category(id, cat.name);
                }
            }
        }
    }
    m_playtime.load(m_data_dir + "/playtime.json");
}

void SteamLauncherGUI::load_remarks() {
    std::ifstream f = open_ifstream(m_data_dir + "/remarks.json");
    if (!f) return;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    size_t pos = 0;
    auto skip_ws = [](const std::string& s, size_t& p) {
        while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p;
    };
    skip_ws(content, pos);
    if (pos < content.size() && content[pos] == '{') ++pos;
    while (pos < content.size()) {
        skip_ws(content, pos);
        if (pos >= content.size() || content[pos] == '}') break;
        // 找键
        size_t key_start = pos;
        while (pos < content.size() && content[pos] != ':' && content[pos] != '\n') ++pos;
        std::string key = content.substr(key_start, pos - key_start);
        // 去除空白
        while (!key.empty() && (key.back()==' '||key.back()=='\t'||key.back()=='\n'||key.back()=='\r')) key.pop_back();
        if (pos < content.size() && content[pos] == ':') ++pos;
        skip_ws(content, pos);
        // 找值 (字符串)
        if (pos < content.size() && content[pos] == '"') {
            ++pos;
            std::string val;
            while (pos < content.size() && content[pos] != '"') {
                if (content[pos] == '\\' && pos+1 < content.size()) {
                    char nc = content[pos+1];
                    if (nc == 'n') val += '\n';
                    else if (nc == 't') val += '\t';
                    else if (nc == 'r') val += '\r';
                    else if (nc == '\\') val += '\\';
                    else if (nc == '"') val += '"';
                    else val += nc;
                    pos += 2;
                } else {
                    val += content[pos++];
                }
            }
            if (pos < content.size()) ++pos;
            try {
                uint32_t appid = (uint32_t)std::stoul(key);
                m_remarks[appid] = val;
            } catch (...) {}
        }
        // 跳到下一个键
        while (pos < content.size() && content[pos] != ',' && content[pos] != '}') ++pos;
        if (pos < content.size() && content[pos] == ',') ++pos;
    }
}

void SteamLauncherGUI::save_remarks() {
    std::ofstream f = open_ofstream(m_data_dir + "/remarks.json");
    if (!f) return;
    f << "{\n";
    bool first = true;
    for (auto& [appid, remark] : m_remarks) {
        if (!first) f << ",\n";
        first = false;
        // escape
        std::string esc;
        for (char c : remark) {
            if (c == '"') esc += "\\\"";
            else if (c == '\\') esc += "\\\\";
            else if (c == '\n') esc += "\\n";
            else esc += c;
        }
        f << "  \"" << appid << "\": \"" << esc << "\"";
    }
    f << "\n}\n";
    f.close();
}

void SteamLauncherGUI::save_all() {
    m_categories.save(m_data_dir + "/categories.json");
    m_playtime.save(m_data_dir + "/playtime.json");
}

void SteamLauncherGUI::refresh_game_list() {
    rebuild_filtered_indices();
    update_status_text();
}

void SteamLauncherGUI::update_status_text() {
    std::ostringstream oss;
    oss << "  [i]  \u5171 " << m_scanner.games().size() << " \u4E2A\u6E38\u620F  |  "  // ✨ 共 个游戏
        << m_categories.categories().size() << " \u4E2A\u5206\u7C7B  |  \u663E\u793A "  // 个分类 | 显示
        << m_filtered_indices.size() << " \u4E2A";
    if (m_batch_mode) {
        oss << "  |  \u52FE\u9009 " << m_batch_selected.size();  // 勾选
    }
    snprintf(m_status_text, sizeof(m_status_text), "%s", oss.str().c_str());
}

void SteamLauncherGUI::rebuild_filtered_indices() {
    m_filtered_indices.clear();
    const auto& games = m_scanner.games();
    for (int i = 0; i < (int)games.size(); ++i) {
        const auto& g = games[i];
        if (m_filter_category == "__hidden__") {
            if (m_settings.hidden_appids.count(g.appid) == 0) continue;
        } else if (!m_filter_category.empty()) {
            const auto* ids = m_categories.get_games_in_category(m_filter_category);
            if (!ids || ids->count(g.appid) == 0) continue;
        } else {
            if (m_settings.hidden_appids.count(g.appid) > 0) continue;
        }
        if (!m_filter_text.empty()) {
            std::string low = g.name;
            std::string lt  = m_filter_text;
            std::transform(low.begin(), low.end(), low.begin(), ::tolower);
            std::transform(lt.begin(), lt.end(), lt.begin(), ::tolower);
            if (low.find(lt) == std::string::npos) continue;
        }
        m_filtered_indices.push_back(i);
    }
    // 排序: 0=名称 1=AppID 2=时长 3=成就 4=状态 5=最后启动
    if (m_sort_column >= 0 && m_sort_column <= 5) {
        std::sort(m_filtered_indices.begin(), m_filtered_indices.end(),
            [this, &games](int a, int b) {
                const GameInfo& ga = games[a];
                const GameInfo& gb = games[b];
                int cmp = 0;
                switch (m_sort_column) {
                    case 0: {
                        auto ra = m_remarks.find(ga.appid);
                        auto rb = m_remarks.find(gb.appid);
                        std::string na = (ra != m_remarks.end() && !ra->second.empty()) ? (ra->second + "|" + ga.name) : ga.name;
                        std::string nb = (rb != m_remarks.end() && !rb->second.empty()) ? (rb->second + "|" + gb.name) : gb.name;
                        cmp = strcmp(na.c_str(), nb.c_str());
                        break;
                    }
                    case 1: cmp = (int)ga.appid - (int)gb.appid; break;
                    case 2: cmp = (int)(ga.total_playtime_min - gb.total_playtime_min); break;
                    case 3: {
                        // 成就: 未解锁的排最后; 然后按 (已解锁/总) 进度
                        int progA = (ga.achievements_total > 0) ? (ga.achievements_unlocked * 1000 / ga.achievements_total) : -1;
                        int progB = (gb.achievements_total > 0) ? (gb.achievements_unlocked * 1000 / gb.achievements_total) : -1;
                        cmp = progA - progB;
                        break;
                    }
                    case 4: cmp = (int)(!ga.install_dir.empty()) - (int)(!gb.install_dir.empty()); break;
                    case 5: cmp = (ga.last_played < gb.last_played) ? -1 : ((ga.last_played > gb.last_played) ? 1 : 0); break;
                }
                if (cmp == 0) cmp = (int)ga.appid - (int)gb.appid;
                return m_sort_ascending ? (cmp < 0) : (cmp > 0);
            });
    }
    m_filtered_dirty = false;
}

// ============================================================
//  icons
void SteamLauncherGUI::upload_icons_to_imgui() {
    // Icons already in m_icons; we lazily upload on demand
}

ImTextureID SteamLauncherGUI::get_or_upload_icon(uint32_t appid) {
    auto it = m_icon_textures.find(appid);
    if (it != m_icon_textures.end()) return it->second;

    // Use IconLoader to get a HBITMAP, then upload
    const GameInfo* ginfo = nullptr;
    for (const auto& g : m_scanner.games()) {
        if (g.appid == appid) { ginfo = &g; break; }
    }
    if (!ginfo) return nullptr;
    HBITMAP hbm = m_icons.get_icon(appid, ginfo->install_dir, ginfo->library_path);
    if (!hbm) return nullptr;

    BITMAP bm;
    if (!GetObject(hbm, sizeof(bm), &bm)) return nullptr;
    int w = bm.bmWidth, h = bm.bmHeight;
    if (w <= 0 || h <= 0) return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biBitCount    = 32;
    std::vector<uint8_t> pixels(w * h * 4);
    HDC hdc = GetDC(nullptr);
    GetDIBits(hdc, hbm, 0, h, pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);

    // Premultiply alpha
    for (int i = 0; i < w * h; ++i) {
        uint8_t* p = &pixels[i*4];
        uint8_t a = p[3];
        p[0] = (uint8_t)(p[0] * a / 255);
        p[1] = (uint8_t)(p[1] * a / 255);
        p[2] = (uint8_t)(p[2] * a / 255);
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = w;
    desc.Height             = h;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem             = pixels.data();
    sub.SysMemPitch         = w * 4;
    ID3D11Texture2D* pTex = nullptr;
    if (FAILED(m_pd3dDevice->CreateTexture2D(&desc, &sub, &pTex))) return nullptr;
    ID3D11ShaderResourceView* pSRV = nullptr;
    if (FAILED(m_pd3dDevice->CreateShaderResourceView(pTex, nullptr, &pSRV))) {
        pTex->Release();
        return nullptr;
    }
    pTex->Release();
    ImTextureID id = (ImTextureID)pSRV;
    m_icon_textures[appid] = id;
    return id;
}

void SteamLauncherGUI::show_toast(const std::string& msg, float duration) {
    m_toast_msg = msg;
    m_toast_until = (float)ImGui::GetTime() + duration;
}

void SteamLauncherGUI::confirm(const char* title, const char* msg, std::function<void(bool)> cb) {
    m_show_confirm = true;
    snprintf(m_confirm_title, sizeof(m_confirm_title), "%s", title);
    snprintf(m_confirm_msg, sizeof(m_confirm_msg), "%s", msg);
    m_confirm_result = false;
    m_confirm_callback = cb;
}
