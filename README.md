# TrafficMonitorPeripheralBattery

一个用于 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 的无线外设电量插件。它直接读取 2.4G / USB 接收器的厂商 HID 报告，把鼠标、键盘、手柄的电量常显在任务栏，不需要悬浮窗。

> A TrafficMonitor plugin that shows wireless mouse, keyboard and gamepad battery levels directly in the Windows taskbar. It reads vendor HID reports from the USB receiver and does not require the vendor's background software.

![任务栏效果](docs/images/taskbar.png)

上图为 1.4.0 的任务栏效果：`G03` 鼠标、`B87` 键盘、`A9+` 鼠标显示实时电量。1.3.0 起未连接的设备不再占位，因此未开机的手柄不会显示 `--`。

## 支持设备

| 设备 | VID:PID | HID 接口 | 状态 |
|---|---|---|---|
| AUSDOM G03 Pro 8K | `363C:FF99` | `FFA0/0002`，输出报告 11 | 实机验证 |
| iLovBee B87 | `3554:FA09` | `FF02/0002`，20 字节 | 实机验证 |
| ATK 蜻蜓 A9 Plus 2.0 | `373B:10C9` | `FF02/0002`，17 字节集合 | 实机验证 |
| Flydigi Direwolf 4 | `045E:028E`（XInput） | 飞智空间站命名管道 | 需安装飞智空间站；0–6 档 |

> Flydigi Direwolf 4 的 XInput 电量不可靠（接收器常报 WIRED/FULL）。插件改为读取飞智空间站后台服务的命名管道 `\\.\pipe\fcs.sock`，因此需要安装官方飞智空间站并保持后台服务运行。空间站 App 打开时会占用管道，关闭 App 后后台服务仍可读取。任务栏显示的是 0–6 档换算的近似百分比，悬停提示会给出 `5/6` 这样的原始档位。

> 红龙 TL84（`25A7:FA70`）目前**读不到电量**：这颗 Compx 接收器只有一个
> 8 字节 feature 寄存器（内容恒定的固件参数）和一个不推送数据的 8 字节输入
> 集合，官方驱动也只提供改键 / 灯光 / 宏，没有电量页面。排查依据和复核脚本
> 见 [docs/PROTOCOLS.md](docs/PROTOCOLS.md) 与 `bin/probe-tl84.ps1`。

协议细节和字节偏移见 [docs/PROTOCOLS.md](docs/PROTOCOLS.md)。

## 功能

- 任务栏直接显示多个设备的电量，不需要鼠标悬浮。
- 电池颜色按电量百分比分级：≥60% 绿、21–59% 橙、≤20% 红，充电中青蓝、离线灰；支持深浅色主题。
- `stacked` 模式（推荐）：纵向 3 行——CPU、内存、以及水平并排的两台设备电池图标；每行约 10.7px，字号 9px。三行共用一套网格，进度条与电池左右边缘严格对齐，默认不显示百分比、不浪费横向空间。
- `hybrid` 模式：纵向 4 行（CPU / 内存 / 两台设备），上两行官方长条、下两行迷你电池；行高只有 8px、字为 7px，适合设备多、不太在意数字辨识度的场景。
- CPU/内存直接取 TrafficMonitor 每个监控周期推送的数值，无需额外采集线程；数值同时写进鼠标提示。
- `hybrid` 按「最宽一行」计算宽度，比横排明显更省任务栏横向空间。
- 后台线程轮询，不阻塞 TrafficMonitor UI。
- HID 路径缓存，只有设备失联时才重新枚举；离线设备自动指数退避，多设备时更省资源。
- 设备休眠/关机时保留最后一次电量并加离线角标，不会卡住宿主；`keep_last_known=false` 可恢复显示 `--`。
- 低电量通知：实时放电读数降到阈值（默认 10%）时弹一次任务栏通知；缓存读数和充电中的设备不触发，回血 5% 后才会再次提醒。
- 动态发现：只显示当前连接的设备，接收器拔掉并超过宽限期后自动移除，无需重启。
- 每台设备可单独隐藏、改名、设置标签、优先级，并可勾选「按电量自动着色」或指定固定色，保存后立即生效。
- 插件自带设置对话框：刷新间隔、最大设备数、显示模式、已发现设备列表、自动/固定颜色；「高级设置...」二级对话框覆盖其余全局选项。
- 尊重 TrafficMonitor 主程序的“不显示百分号”和任务栏文字颜色设置。
- 可配置刷新间隔和最大显示设备数。
- 插件菜单提供“立即刷新”“打开设置文件”和“诊断信息”（可复制的版本、路径、设置与逐设备状态报告）。
- 插件管理器中的“选项”直接打开图形设置对话框，无需手改 INI。

## 安装

1. 确认你的 `TrafficMonitor.exe` 是 x64 还是 x86。
2. 把 `PeripheralBatteryPlugin-x64.dll` 或 `PeripheralBatteryPlugin-x86.dll` 复制到 TrafficMonitor 的 `plugins` 目录。
3. 将文件名改为 `PeripheralBatteryPlugin.dll`。
4. 重启 TrafficMonitor。
5. 在任务栏窗口右键 →“显示设置”，勾选「无线外设电量」这一个项目（从 1.2.x 升级的用户需要重新勾选一次）。

如果使用预编译包，解压 `dist` 下最新版本的压缩包，取对应位数的 DLL。

## 构建

要求：

- Visual Studio 2022 或 2026，安装“使用 C++ 的桌面开发”工作负载。
- CMake 3.20+（VS 自带 CMake 也可）。

```powershell
scripts\build.ps1
```

只构建 x64：

```powershell
scripts\build.ps1 -SkipX86
```

输出：

```text
bin\x64\PeripheralBatteryPlugin.dll
bin\x64\hid-probe.exe
bin\x64\protocol_tests.exe
bin\x64\settings_tests.exe
bin\x64\display_tests.exe
bin\x86\PeripheralBatteryPlugin.dll
```

手动构建：

```powershell
cmake --preset x64
cmake --build --preset x64
ctest --test-dir build\x64 -C Release --output-on-failure
```

如果本机没有 Visual Studio 2026，把 `CMakePresets.json` 中的生成器改为
`Visual Studio 17 2022`，或用 `cmake -G "Visual Studio 17 2022" -A x64`。

## 设置

插件首次加载时会在 TrafficMonitor 插件配置目录创建
`PeripheralBatteryPlugin.ini`。在插件管理器里点“选项”会打开图形设置
对话框：只列出**当前连接的设备和已配置过的设备**，不会把整个设备注册表
一股脑列出来；支持重新扫描、上移/下移排序，以及每台设备的「在任务栏显示」
开关、名称、标签、优先级和「按电量自动着色」。其余全局开关（离线保留、
低电量通知、行样式、颜色阈值等）在「高级设置...」二级对话框中修改。

所有改动保存后**立即生效，不需要重启 TrafficMonitor**。INI 也可以直接编辑，
保存后约 2 秒内自动重载。INI 只为已发现或用户改过的设备写段，新增适配设备
不会让配置文件膨胀。

```ini
[plugin]
refresh_interval_seconds=30
max_devices=6
priority_mode=fixed
show_offline=true
display_mode=stacked
presence_grace_seconds=300
keep_last_known=true
low_battery_notify=true
low_battery_notify_percent=10
hybrid_rows=4
row_bar_width=48
row_style=solid
stacked_show_percent=false
battery_low_percent=20
battery_good_percent=60
cpu_orange_percent=50
cpu_red_percent=80
color_high=#4CAF50
color_medium=#FF9800
color_low=#F44336
color_charging=#03A9F4

[device:g03_pro]
hidden=false
name=AUSDOM G03 Pro
label=G03
priority=10
color=auto
```

- `refresh_interval_seconds`：轮询间隔，5–3600 秒。
- `max_devices`：最多显示几个设备，1–8。
- `priority_mode`：目前为 `fixed`（按 `priority` 排序）；`lowest` / `charging` 为后续扩展。
- `show_offline`：离线时显示 `--`；设为 `false` 则显示空值。
- `display_mode`：`bar`（默认，横排电池图标）、`text`（纯文本拼接）、`hybrid`（纵向 4 行）、`stacked`（纵向 3 行，CPU/内存 + 并排两台设备，推荐）。
- `presence_grace_seconds`：从未上报过电量的设备在接收器断开后仍保留并显示 `--` 的秒数，默认 300，`0` 表示立即移除。
- `keep_last_known`：`true`（默认）时设备休眠后继续显示最后一次电量并加离线角标；`false` 恢复旧行为（宽限期内 `--`，之后移除）。
- `low_battery_notify` / `low_battery_notify_percent`：是否在实时放电读数降到阈值（默认 10%）时弹一次任务栏通知；缓存读数和充电中不触发，回血 5% 后才会再次提醒。
- `hidden`：`true` 表示设备已连接但不显示在任务栏；旧版 `enabled=false` 会自动迁移为 `hidden=true`。
- `color`：`auto`（默认，按电量百分比着色）或 `#RRGGBB`（固定色）。旧版轮转调色板的默认色会在首次加载时自动迁移为 `auto`；手动选过的自定义色保留。
- `hybrid_rows`：总行数，2–4（默认 4）。前两行固定是 CPU/内存，其余按 `priority` 取设备，最多 2 台；设备不足时行数减少、每行变高。
- `row_bar_width`：每行进度条宽度（96 DPI 下的像素），16–160，默认 48。
- `row_style`：`solid`（默认，无描边、无内缩、无上下留白，色块占满整行）或 `framed`（旧样式的 1px 白框 + 2px 内缩，可一键回退）。
- `stacked_show_percent`：`false`（默认，stacked 三行都不显示百分比，最紧凑）；设为 `true` 时每个电池组和系统行都会多出一列数值，整体自动变宽并保持同一套网格对齐。
- `battery_low_percent` / `battery_good_percent`：电池红/橙/绿分界，默认 20 / 60。
- `cpu_orange_percent` / `cpu_red_percent`：CPU、内存占用率橙/红分界，默认 50 / 80。
- `color_high` / `color_medium` / `color_low` / `color_charging`：绿、橙、红、充电四档颜色。
- 所有设置改动立即生效，无需重启 TrafficMonitor。

> **`hybrid` 模式需要关闭宿主的水平排列**：在任务栏窗口右键 →「显示设置」→
> 取消勾选「水平排列」。宿主 `TASKBAR_WND_HEIGHT` 固定为 `DPI(32)`，插件通过
> `IsDoubleLineExclusive()` 独占双行拿到完整 32px，才能分成 4 行。若仍开启水平
> 排列（只有 21px），插件会自动降级为 4 条无文字的纯色进度条，数值仍可在鼠标
> 提示中查看。100% DPI 下 4 行每行约 8px，因此行内小字为 7px，这是宿主的
> 物理高度限制；设备较少时（2–3 行）字号会自动放大到 9px。

> 去掉内外边距和描边**不能**让每行超过 8px——`h` 是宿主给的固定值。但
> `row_style=solid` 把原本被 1px 描边、2px 内缩和上下留白吃掉的像素还给色块，
> 填充高度从约 2px 变成约 7px，颜色分级明显更醒目，也更接近官方状态条的
> 纯填充样式。想真正让每行变高，只能减少行数（`hybrid_rows=3` → 约 10.7px）
> 或提高系统 DPI 缩放（宿主 `DPI(32)` 会跟着放大）。

> **`stacked` 是更清楚的折中**：把 32px 拆成 10 / 10 / 12px 三行，前两行系统
> 长条用 9px 字，第三行放两个独立的 16×10 电池图标 + 百分比（余数给第三行，
> 图标不会被裁切）。设备不足 2 台时自动收缩为 2 行、每行 16px；一台都没有时
> 只画 CPU 和内存。三行共用一套网格：标签列 + 4px 间隙 + 内容区，系统长条的
> 左端与第一个电池图标左端相同、右端与最后一个电池图标（或数值）右端相同，
> 因此两组电池之后不会再留下空列。默认不显示百分比时实测约 **98px**，
> 比 `hybrid` 的约 100px 还窄，比原来 3 台横排的约 194px 省一半；
> 打开 `stacked_show_percent=true` 后约 152px，所有元素仍按同一网格对齐。
> 只剩一台设备时网格会跟着收窄，不会留半列空白。

插件会跟随 TrafficMonitor 主程序的显示设置：任务栏/主窗口的“不显示百分号”
（`task_bar_hide_percent`、`hide_percent`）会让插件去掉 `%`，主程序的标签和
数值文字颜色也会用于自绘电池。设备的 `color` 只控制电池填充色（默认 `auto`
按电量着色），与宿主的文字颜色设置互不冲突。

详细说明见 [docs/SETTINGS.md](docs/SETTINGS.md)。

## 新增设备

1. 用 `bin\x64\hid-probe.exe --list` 找到接收器的厂商 HID 接口。
2. 抓取官方驱动或网页驱动的请求/响应报文。
3. 在 `src/core/DeviceRegistry.cpp` 添加设备描述；若接收器有多个同 usage 的
   厂商集合（如 ATK `373B:10C9` 同时有 20 字节和 17 字节的 `FF02/0002`），
   用 `reportLength` 指定协议实际使用的报告长度。
4. 在 `src/protocols/` 添加或扩展协议解析器；若不是 HID 传输，覆写
   `IProtocol::IsPresent()` 提供在线判定。
5. 在 `tests/protocol_tests.cpp` 加入抓包回放测试。
6. 更新 `docs/PROTOCOLS.md`、README 和 CHANGELOG。

完整流程见 [docs/ADDING_A_DEVICE.md](docs/ADDING_A_DEVICE.md)。

## 给 AI 用的适配 Skill

本仓库包含一个面向 AI coding agent 的 skill：

- [skills/adapt-wireless-battery/SKILL.md](skills/adapt-wireless-battery/SKILL.md)

它描述了从设备识别、HID 枚举、抓包、协议推导、实现、测试到文档更新的完整流程，并附带：

- `references/capture-guide.md`：WebHID / USBPcap 抓包方法。
- `references/protocol-report-template.md`：协议记录模板。
- `references/device-descriptor-template.md`：设备描述和协议骨架。
- `references/checklist.md`：适配验收清单。

## 架构

```text
hid-probe / tests
        │
        ▼
src/core/HidTransport      枚举、打开、带超时的 HID 读写
        │
        ▼
src/protocols/*            设备协议族 + IsPresent() 在线判定
        │
        ▼
src/core/DeviceRegistry    设备表（VID/PID、usage、协议、优先级）
src/core/DeviceManager     发现 → 活跃设备池 → 轮询线程 + 快照
        │
        ▼
src/plugin/BatteryGroupItem  唯一自绘项目，内部渲染 N 台设备
src/plugin/Plugin            设置热生效、INI 热重载、项目 ID 恒定
```

更多说明见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 测试

```powershell
bin\x64\protocol_tests.exe
bin\x64\settings_tests.exe
bin\x64\display_tests.exe
```

测试覆盖：

- BYCOMBO 20 字节请求的长度、报告 ID、命令和校验和。
- iLovBee B87 回包电量解析。
- BYCOMBO 0x4A / 0x0A 兼容帧。
- Juxing 请求和 G03 Pro 回包电量解析。
- Compx 17 字节请求、校验和、电量/电压/充电标志解析与坏帧拒绝。
- Flydigi 空间站 protobuf 回包解析。
- 设置 INI 的默认值、存盘回读、惰性落盘、`enabled` 迁移、重复段落回归。
- `ClampValues()` 范围限制与低/高分界顺序，以及低电量通知状态机（每次掉电只提醒一次、回血滞回、缓存/关闭/重新启用）。
- 设备在线判定、失联宽限期、最后读数缓存、单项目文本拼接与宽度随设备数增长。
- 显示层：`stacked` 网格宽度不变量、百分比显示开关、缓存读数角标、深浅色下的颜色分级，以及 GDI 绘制冒烟测试。

## 许可证

本项目使用 MIT License，见 [LICENSE](LICENSE)。

`include/PluginInterface.h` 来自 TrafficMonitor，版权所有 (c) 2019
zhongyang219，使用 Anti-996 License 1.0。详见 [NOTICE.md](NOTICE.md) 和
[LICENSES/Anti-996-1.0.txt](LICENSES/Anti-996-1.0.txt)。

## 致谢

- [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 插件宿主与接口。
- [Volt](https://github.com/435311382/Volt)、[wireless-battery-tray](https://github.com/kun-qaq/wireless-battery-tray)
  提供的 Compx / BYCOMBO 协议参考。
- 所有提交设备抓包和测试结果的贡献者。
