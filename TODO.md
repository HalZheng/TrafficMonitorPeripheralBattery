# 待完成任务

> 1.0.0 已完成通用化重构；下面是后续路线。开发前请先看
> `docs/ARCHITECTURE.md`、`docs/ADDING_A_DEVICE.md` 和
> `skills/adapt-wireless-battery/SKILL.md`。

## 1.0.0 已完成

- 标准仓库结构：`include/`、`src/core`、`src/protocols`、`src/plugin`、
  `tools/hid-probe`、`tests`、`docs`、`skills`、CMake、GitHub Actions。
- 通用 HID 传输层，所有读写带超时，不阻塞 UI。
- 协议注册表与设备注册表；G03 Pro（Juxing）和 iLovBee B87（BYCOMBO）。
- 设备项 ID 改为 `g03_pro` / `ilovbee_b87`，标签改为 `G03` / `B87`。
- INI 设置：刷新间隔、最大设备数、每设备启用/名称/标签/优先级/颜色。
- 插件命令：“立即刷新”“打开设置文件”。
- `hid-probe` 通用 HID 枚举/发送/被动读取/feature 工具。
- 协议回放测试、CMake 构建、x64/x86 产物、GitHub Actions。
- AI 适配 skill 及抓包/协议模板/检查清单。

## 1.1.0 已完成

- Flydigi Direwolf 4 已接入：通过飞智空间站服务命名管道读取 0–6 档电量，
  XInput 的 WIRED/FULL 不可靠，已弃用。
- `FlydigiPipeProtocol`、protobuf 解析和单元测试。
- 文档补充 Flydigi 管道路径、依赖和限制。
- 实机验证待安装飞智空间站服务后进行；当前 Direwolf 项显示 `--`。

## P0 - 后续设备

### Flydigi 无依赖 USB 厂商接口（可选）

- 当前依赖飞智空间站服务。
- Direwolf 4 的厂商 XInput 接口是 `Class 0xFF/0x5D/0x01`，Windows 下被 Xbox
  驱动占用；直接读取需要 WinUSB/libusb 驱动层方案，可能影响 XInput。
- 若未来实现，可发送 15 字节 `A5 10 ... B5` info 请求，解析 32 字节回复的
  byte 23 `battery_raw`。
- 验收：不安装飞智空间站也能读电量；不影响手柄输入。

### 其他设备

- 按 `skills/adapt-wireless-battery/SKILL.md` 流程适配。
- 需要确认品牌/型号/连接方式，并抓包验证。

## 1.2.0 已完成

- 自绘电池图标 + 电量条 + 百分比，参考官方 Battery 插件的
  `IsCustomDraw` / `GetItemWidthEx` / `DrawItem` 思路。
- 每设备 `color` 生效；低电量（≤20%）红色，充电绿色；深浅色主题适配。
- `display_mode=bar` 为默认，`text` 保留为旧样式。
- 真正的 Win32 设置对话框：刷新间隔、最大设备数、显示模式、设备列表、
  启用/名称/标签/优先级/颜色，保存写回 INI。
- 通过 `OnExtenedInfo` 接入主程序设置：任务栏/主窗口“不显示百分号”、
  任务栏窗口显示状态、标签/数值文字颜色。
- 修复 UTF-8 BOM 导致的 INI 重复段落问题。
- 插件元数据补齐：`TMI_URL` 指向仓库，DLL 带版本资源，版本号统一由
  CMake 生成。
- 新增 `settings_tests`，覆盖 INI 默认值、存盘回读和重复段落回归。
- README 增加任务栏实拍图。
- HID 路径缓存：首次枚举后复用设备路径，失联时才重新枚举。
- 离线设备指数退避（最高 300 秒），手动“立即刷新”会清空退避。
- INI 改为临时文件原子替换；正常启动不再重写已有配置，保留手写注释。

## P1 - 显示与设置体验

### 1. 自绘电量条（1.2.0 已完成）

- 已实现：标签 + 电池外框 + 电量填充 + 百分比。
- 后续可选：充电闪电动画、图标资源、更多电池样式。

### 2. 真正的选项对话框（1.2.0 已完成）

- 已实现 Win32 对话框，替代原来的“打开 INI”。
- 控件：刷新间隔、最大设备数、显示模式、设备启用列表、名称/标签、
  优先级、颜色。
- 保存后提示重启 TrafficMonitor（TrafficMonitor 缓存 item 指针）。
- 验收：不手改 INI 也能完成常用配置。

### 3. 优先级策略

- 当前 `priority_mode=fixed` 按优先级排序。
- 增加 `lowest`（电量最低优先）和 `charging`（充电中优先）。
- 由于 TrafficMonitor 在启动时缓存 item 指针，动态排序需要：
  - 启动时按固定顺序创建 item，运行时只更新值；或
  - 合并为单个自绘 item，在内部排序多个设备。
- 验收：设置后顺序符合预期，重启后仍保持。

## P2 - 可扩展性与稳定性

### 4. 外部设备注册表

- 允许用户通过 `devices.json` / `devices.ini` 添加简单 HID 设备，无需重新编译。
- 字段：VID/PID、usage page/usage、协议 ID、查询帧、响应前缀、电量偏移/缩放、校验。
- 复杂协议仍用 C++ reader。
- 验收：一个简单新设备只改配置即可显示。

### 5. 多设备与热插拔

- 同一 VID/PID 多台设备按路径/序列号区分（当前一个描述符对应一台）。
- 接收器拔出/重插、系统睡眠恢复后自动重连。
- 验收：拔插后一个轮询周期内恢复。

### 6. 诊断与日志

- 可选日志：设备枚举、查询失败、原始回包十六进制。
- `hid-probe` 增加报告描述符/value caps dump。
- 验收：新增设备适配时可按日志复现。

## P3 - 交付与生态

### 7. 安装/自启

- 一键安装脚本：按 TrafficMonitor 位数复制 DLL。
- TrafficMonitor 开机自启（可选）。
- 卸载说明。

### 8. 发布

- `scripts/package.ps1` 生成 `dist/PeripheralBatteryPlugin-<version>.zip`。
- GitHub Actions 上传 x64/x86 产物。
- 发布说明模板、版本号同步检查。

### 9. 文档/测试

- 每台设备在 `docs/PROTOCOLS.md` 中有完整抓包和字节说明。
- 为每个解析器保留回放 fixture。
- 增加更多协议家族（Nordic/Telink/BLE GATT 电池服务等）。

## 待确认

- 飞智冰原狼具体型号和连接方式？
- 默认最大设备数用 2、3 还是 4？
- 默认优先级用固定顺序还是低电量优先？
- 是否需要支持蓝牙 BLE 电池服务（Windows 已能显示部分设备电量）？
