# WS63 救援项目交付说明

## 当前状态

- 救援端固件已编译并已烧录到 COM9，串口已验证可以连接 WiFi 并向华为云 MQTT 上报。
- 受灾端固件已编译，固件中已经包含 GY-MAX30102 血氧心率模块驱动，串口为 COM12。
- 现在救援端收到的 `spo2=88~92`、`hr=116~131` 是旧受灾端固件发出的固定假数据。

## 固件路径

- 救援端：`D:\table\jiuyuan\xingshan\fbb_ws63-jiuyuanduan\fbb_ws63-master\src\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg`
- 受灾端：`D:\table\jiuyuan\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src\output\ws63\fwpkg\ws63-liteos-app\ws63-liteos-app_all.fwpkg`

## 一键脚本

在 PowerShell 中运行：

```powershell
Set-ExecutionPolicy -Scope Process Bypass
```

重新编译两端：

```powershell
& "D:\table\jiuyuan\delivery\build_all.ps1"
```

烧录救援端到 COM9：

```powershell
& "D:\table\jiuyuan\delivery\burn_rescuer_com9.ps1"
```

烧录受灾端到 COM12：

```powershell
& "D:\table\jiuyuan\delivery\burn_victim_com12.ps1"
```

监视串口：

```powershell
& "D:\table\jiuyuan\delivery\monitor_com9.ps1" -Seconds 60
& "D:\table\jiuyuan\delivery\monitor_com9.ps1" -Port COM12 -Seconds 60
```

## 必须按这个顺序验证

1. 先拔掉救援端 USB。
2. 把受灾端单独插到电脑，确认它是 COM12。
3. 运行 `burn_victim_com12.ps1` 烧录受灾端。
4. 运行 `monitor_com9.ps1 -Port COM12` 看受灾端日志。
5. 看到 MAX30102 初始化成功后，再插回救援端验证接收数据。

## 受灾端期望日志

正常启动会看到：

```text
[RESCUE_VICTIM] max30102 task created
[RESCUE_VICTIM] max30102 task start
[MAX30102] part id=0x15, expect=0x15
[MAX30102] init ok
```

没放手指或信号弱会看到：

```text
[RESCUE_VICTIM] max30102 ready, no finger or weak signal
```

接线或供电错误会看到：

```text
[MAX30102] read part id failed, check VCC/GND/SCL/SDA addr=0x57
```

## GY-MAX30102 接线

当前代码配置：

- I2C 总线：`1`
- SDA：芯片 GPIO `15`，板子丝印通常为 `UART1_TXD`
- SCL：芯片 GPIO `16`，板子丝印通常为 `UART1_RXD`
- I2C 地址：`0x57`
- PART ID：`0x15`

如果模块灯不亮，优先检查：

- VCC/GND 是否接反或没接稳；
- 模块是否支持 3.3V 供电；
- SDA 是否接到板子 `UART1_TXD`；
- SCL 是否接到板子 `UART1_RXD`；
- OLED 和 MAX30102 共用 I2C 时，SCL/SDA 必须并联到同一组引脚，地址不能冲突。

## 华为云状态

救援端串口已验证出现：

```text
[RESCUE_CLOUD] mqtt property publish ok:victim_state
[RESCUE_CLOUD] mqtt publish ok:sle_report
```

这表示华为云 MQTT 上报链路已经打通。如果控制台仍显示未激活，优先刷新页面、确认设备 ID 与产品模型属性一致。
