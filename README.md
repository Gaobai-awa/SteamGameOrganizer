# Steam 游戏管理器 (SteamGameOrganizer)

> 🤖 **本项目代码与文档由 AI 辅助生成**。开发者审阅了所有改动并对最终结果负责。

一个开源的、专注于**游戏库整理**与**辅助启动**的 Windows 工具。基于 C++17 + Dear ImGui + DirectX 11 构建。

**这是 Steam 官方客户端的伴侣 / 扩展工具，不是替代品**——你仍然需要 Steam 客户端在后台运行来启动游戏、下载更新、同步成就。Steam 游戏管理器做的事情是：

- **让 Steam 客户端的库管理功能更强大**（子分类、批量操作、备注）
- **提供更现代、更轻量的界面**（基于 ImGui，启动 < 1 秒，内存 ~80MB）
- **补齐 Steam 客户端缺失的功能**（每款游戏独立备注、批量分类、可视化时长统计）

---

## 功能

### 游戏库管理
- **完整扫描** `appinfo.vdf` 和 `libraryfolders.vdf` — 自动发现所有 Steam 库文件夹下的游戏、工具、DLC
- **中文名支持** — 自动从 `appinfo.vdf` 读取本地化名称，看到 `逆转裁判` 而不是 `Phoenix Wright`
- **多语言切换** — 自动适配系统语言（schinese / tchinese / english / japanese）
- **多扫描模式**：
  - `libraryfolders + appinfo`（推荐，完整且快）
  - `appinfo only`（只读 appinfo.vdf）
  - OpenSteamTool DLL 注入（实验性）

### 分类系统
- **无限层级子分类** — Steam 客户端只支持扁平分类；这里支持 `单人 → RPG → 剧情向`
- **批量分类管理** — 一次选 50+ 款游戏，统一加 / 移除 / 重命名分类
- **右键快速标记** — 选中游戏 → 右键 → 加入/移出分类（带子菜单）
- **分类管理对话框** — 创建、重命名、删除、嵌套子分类
- **按分类过滤** — 点左栏分类名，主列表自动筛选

### 启动与统计
- **智能启动** — 调用 Steam URI 协议启动游戏（与 Steam 客户端相同的行为）
- **独立时长统计** — 不依赖 Steam 客户端，自己记录每次启动会话
- **启动次数** — 每个游戏独立的会话计数器
- **最后启动时间** — 在列表中显示
- **图标显示** — 从 Steam 缓存读取库图标，ImGui 渲染

### 搜索与过滤
- **实时文本搜索** — 输入即过滤，匹配中英文名
- **分类过滤** — 单击左栏分类筛选
- **隐藏游戏** — 不删除游戏，只在主列表隐藏（独立对话框统一管理）
- **排序** — 按名称 / AppID / 时长 / 状态，点击列头切换
- **已删除/未安装** 游戏状态正确显示

### Steam 进程工具（高级）
- **Steam 进程监控** — 实时检测 Steam 客户端是否在运行
- **DLL 注入** — 提供 `steam_hook.dll` 注入到 Steam 客户端（高级用户）
- **协议处理器** — 与 Steam `steam://` 协议协同

### 用户体验
- **ImGui + DirectX 11** 渲染 — 即时启动，GPU 友好
- **暗色主题** — 自定义配色，圆角边框
- **中文字体** — 自动加载 `msyh.ttc` / `simhei.ttf`（全字符集）
- **高 DPI 适配** — 字体随系统 DPI 缩放

---

## 与 Steam 官方客户端的对比

下表列出 **Steam 官方客户端不提供**或**做得不好**的功能——本工具是这些方面的扩展。

| 功能 | Steam 官方客户端 | Steam 游戏管理器 |
|---|---|---|
| 用户自定义子分类 | ❌ 只能一层 | ✅ 无限层级 |
| 批量分类管理 | ❌ 一款一款点 | ✅ 一次选 N 款统一操作 |
| 每游戏独立备注 | ❌ | ✅ 在右侧详情面板添加 |

**Steam 官方客户端仍然负责**（本工具不重复造轮子）：
- 启动游戏本身（通过 `steam://` 协议）
- 下载/更新游戏
- 云存档同步
- Steam 成就解锁
- Steam 好友 / 聊天
- Steam 创意工坊
- 商店购买

---

## 工作方式

```
┌────────────────────────────────────────────┐
│          Steam 游戏管理器（本工具）            │
│   - 扫描 appinfo.vdf / libraryfolders.vdf  │
│   - 读取你的自定义分类 / 备注 / 时长           │
│   - 启动时调用 steam:// 协议                 │
└────────────────┬───────────────────────────┘
                 │ 通过 steam:// 协议
                 ▼
┌────────────────────────────────────────────┐
│            Steam 官方客户端                  │
│   - 真正启动游戏、加载 DLL                   │
│   - 同步云存档、解锁成就                      │
│   - 需要保持运行                              │
└────────────────────────────────────────────┘
```

**简言之**：本工具是**前端**——更好的 UI、更强的分类；Steam 客户端是**后端**——启动游戏、运行游戏。两者协同工作。

---

## 架构

```
src/
├── main.cpp                 # 入口
├── win_main.cpp             # WinMain + 窗口类
├── gui_app.{h,cpp}          # ImGui + DX11 + 主 UI
├── steam_scanner.{h,cpp}    # appinfo.vdf / libraryfolders.vdf 扫描
├── binary_vdf.{h,cpp}       # Steam 二进制 VDF 解析
├── vdf_parser.{h,cpp}       # 文本 VDF 解析
├── category_manager.{h,cpp} # 分类 CRUD
├── playtime_tracker.{h,cpp} # 时长会话
├── icon_loader.{h,cpp}      # 图标提取
├── game_launcher.{h,cpp}    # 进程启动
├── dll_injector.{h,cpp}     # DLL 注入
├── steam_watcher.{h,cpp}    # Steam 进程监控
├── achievement_manager.{h,cpp}
├── console_ui.{h,cpp}       # 旧版控制台 UI
├── app_settings.h           # JSON 配置
├── game_data.h              # GameInfo / Category 结构
├── imgui/                   # Dear ImGui 1.91.0 + DX11/Win32 后端
└── dll/                     # steam_hook.dll 源码
```

---

## 数据存储

所有用户数据保存在 `<exe_dir>/data/`：

- `data/settings.json` — Steam 路径、扫描方式、隐藏列表、主题
- `data/categories.json` — 你的分类树 + 游戏归属
- `data/playtime.json` — 会话日志 + 总时长
- `data/remarks.json` — 每款游戏的备注

全部是纯 JSON——可以直接手编、备份、迁移。

---

## 构建

### 依赖（Windows）
- Visual Studio 2022（MSVC v14.5+）
- Windows 10/11 SDK
- Dear ImGui 1.91.0（已内置在 `src/imgui/`）

### 编译
```bat
build_msvc.bat
```

输出：
- `bin/SteamLauncher.exe`（约 2 MB）
- `bin/steam_hook.dll`（约 125 KB）

### 调试模式
```bat
SteamLauncher.exe -debug
```
会在 `E:\temp\steam_launcher_debug.log` 写诊断日志。

---

## Steam 路径自动检测

按以下顺序查找：
1. 注册表 `HKCU\Software\Valve\Steam\SteamPath`
2. `C:\Program Files (x86)\Steam\`
3. `C:\Program Files\Steam\`
4. 任意盘符根目录下含 `steam.exe` 的 `Steam` 子目录

如果都找不到，到 **设置 → Steam 路径** 手动指定。

---

## 协议

MIT。详见 `LICENSE`。

---

## 致谢

- [Dear ImGui](https://github.com/ocornut/imgui) — Omar Cornut，MIT 协议
- [ValveKeyValue](https://github.com/ValveResourceFormat/ValveKeyValue) — VDF 格式参考
- [JackalClient](https://github.com/JackalClient/JackalClient) — UI/UX 灵感

---

## 免责声明

本项目**与 Valve Corporation / Steam 没有任何关联**。"Steam" 及 Steam 标志是 Valve Corporation 的商标。本工具是非官方社区项目，仅供个人使用。

---

🤖 **本项目代码与文档由 AI 辅助生成**。开发者审阅了所有改动并对最终结果负责。
