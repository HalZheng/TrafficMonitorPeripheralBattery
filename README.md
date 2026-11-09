# TrafficMonitorPeripheralBattery

一个用于 [TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 的无线外设电量插件。它直接读取 2.4G / USB 接收器的厂商 HID 报告，把鼠标、键盘、手柄的电量常显在任务栏，不需要悬浮窗。

> A TrafficMonitor plugin that shows wireless mouse, keyboard and gamepad battery levels directly in the Windows taskbar. It reads vendor HID reports from the USB receiver and does not require the vendor's background software.

![任务栏效果](docs/images/taskbar.png)

上图是任务栏实际效果：`G03` 鼠标、`B87` 键盘显示实时电量，未安装飞智空间站时 `Direwolf` 显示 `--`。

## 支持设备

| 设备 | VID:PID | HID 接口 | 状态 |
|---|---|---|---|
| AUSDOM G03 Pro 8K | `363C:FF99` | `FFA0/0002`，输出报告 11 | 实机验证 |
| iLovBee B87 | `3554:FA09` | `FF02/0002`，20 字节 | 实机验证 |
| Flydigi Direwolf 4 | `045E:028E`（XInput） | 飞智空间站命名管道 | 需安装飞智空间站；0–6 档 |

> Flydigi Direwolf 4 的 XInput 电量不可靠（接收器常报 WIRED/FULL）。插件改为读取飞智空间站后台服务的命名管道 `\\.\pipe\fcs.sock`，因此需要安装官方飞智空间站并保持后台服务运行。空间站 App 打开时会占用管道，关闭 App 后后台服务仍可读取。任务栏显示的是 0–6 档换算的近似百分比，悬停提示会给出 `5/6` 这样的原始档位。

协议细节和字节偏移见 [docs/PROTOCOLS.md](docs/PROTOCOLS.md)。

## 功能

- 任务栏直接显示多个设备的电量，不需要鼠标悬浮。
- 电池图标 + 电量条 + 百分比，低电量红色、充电绿色，支持深浅色主题。
- 后台线程轮询，不阻塞 TrafficMonitor UI。
- HID 路径缓存，只有设备失联时才重新枚举；离线设备自动指数退避，多设备时更省资源。
- 设备休眠/关机时显示 `--`，不会卡住宿主。
- 每个设备可单独启用/禁用、改名、设置标签、优先级和颜色。
- 插件自带设置对话框：刷新间隔、最大设备数、显示模式、设备列表、颜色。
- 尊重 TrafficMonitor 主程序的“不显示百分号”和任务栏文字颜色设置。
- 可配置刷新间隔和最大显示设备数。
- 插件菜单提供“立即刷新”和“打开设置文件”。
- 插件管理器中的“选项”直接打开图形设置对话框，无需手改 INI。

## 安装

1. 确认你的 `TrafficMonitor.exe` 是 x64 还是 x86。
2. 把 `PeripheralBatteryPlugin-x64.dll` 或 `PeripheralBatteryPlugin-x86.dll` 复制到 TrafficMonitor 的 `plugins` 目录。
3. 将文件名改为 `PeripheralBatteryPlugin.dll`。
4. 重启 TrafficMonitor。
5. 在任务栏窗口右键 →“显示设置”，勾选需要显示的电量项目。

如果使用预编译包，直接解压 `dist/PeripheralBatteryPlugin-1.2.0.zip`，取对应位数的 DLL。

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
对话框，可以修改刷新间隔、最大设备数、显示模式，以及每个设备的启用状态、
名称、标签、优先级和颜色；这些改动会写回 INI。

INI 仍然可以直接编辑，适合批量修改或纳入版本管理。修改后重启
TrafficMonitor 生效（TrafficMonitor 会在启动时缓存插件项目指针）。

```ini
[plugin]
refresh_interval_seconds=30
max_devices=4
priority_mode=fixed
show_offline=true
display_mode=bar

[device:g03_pro]
enabled=true
name=AUSDOM G03 Pro
label=G03
priority=10
color=#4FC3F7

[device:ilovbee_b87]
enabled=true
name=iLovBee B87
label=B87
priority=20
color=#81C784
```

- `refresh_interval_seconds`：轮询间隔，5–3600 秒。
- `max_devices`：最多显示几个设备，1–8。
- `priority_mode`：目前为 `fixed`（按 `priority` 排序）；`lowest` / `charging` 为后续扩展。
- `show_offline`：离线时显示 `--`；设为 `false` 则显示空值。
- `display_mode`：`bar`（默认，自绘电池图标/电量条）或 `text`（旧的纯文本百分比）。
- `color`：bar 模式的电池填充色；低电量（≤20%）自动变红，充电自动变绿。
- 修改设置后需要重启 TrafficMonitor 生效（TrafficMonitor 会缓存插件项目指针）。

插件会跟随 TrafficMonitor 主程序的显示设置：任务栏/主窗口的“不显示百分号”
（`task_bar_hide_percent`、`hide_percent`）会让插件去掉 `%`，主程序的标签和
数值文字颜色也会用于自绘电池。插件自己的 `color` 只控制电池填充色，两者不冲突。

详细说明见 [docs/SETTINGS.md](docs/SETTINGS.md)。

## 新增设备

1. 用 `bin\x64\hid-probe.exe --list` 找到接收器的厂商 HID 接口。
2. 抓取官方驱动或网页驱动的请求/响应报文。
3. 在 `src/core/DeviceRegistry.cpp` 添加设备描述。
4. 在 `src/protocols/` 添加或扩展协议解析器。
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
src/protocols/*            设备协议族（Juxing、BYCOMBO、未来 Flydigi）
        │
        ▼
src/core/DeviceRegistry    设备表（VID/PID、usage、协议、优先级）
src/core/DeviceManager     后台轮询线程 + 快照
        │
        ▼
src/plugin/*               TrafficMonitor ITMPlugin / IPluginItem / 设置
```

更多说明见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 测试

```powershell
bin\x64\protocol_tests.exe
bin\x64\settings_tests.exe
```

测试覆盖：

- BYCOMBO 20 字节请求的长度、报告 ID、命令和校验和。
- iLovBee B87 回包电量解析。
- BYCOMBO 0x4A / 0x0A 兼容帧。
- Juxing 请求和 G03 Pro 回包电量解析。
- Flydigi 空间站 protobuf 回包解析。
- 设置 INI 的默认值、存盘回读，以及重复 `[plugin]` / `[device:...]` 段落的回归检查。

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
