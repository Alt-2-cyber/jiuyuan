# WS63 五节点救援网络

## 设备映射

| 串口 | 角色 | 固件身份 | 上游 |
| --- | --- | --- | --- |
| COM14 | 地面救援端 | GROUND | 无 |
| COM11 | 天空搜救端 | SKY | GROUND |
| COM12 | 受灾端 1 | VD-001 | SKY |
| COM9 | 受灾端 2 | VD-002 | VD-001 |
| COM15 | 受灾端 3 | VD-003 | VD-002 |

链路采用确定性的五节点多跳结构：

```text
GROUND <- SKY <- VD-001 <- VD-002 <- VD-003
```

受灾端优先读取 GPIO15/16 上的 MAX30102。传感器不可用或没有有效手指信号时，固件会上报带 `"source":"SIM"` 标记的演示数据，不会冒充真实测量值。

## 构建

```powershell
Set-ExecutionPolicy -Scope Process Bypass
& "D:\table\jiuyuan\delivery\build_rescue_network.ps1"
```

## 烧录

```powershell
& "D:\table\jiuyuan\delivery\burn_rescue_network.ps1"
```

## 验证

```powershell
& "D:\table\jiuyuan\delivery\verify_rescue_network.ps1" -Seconds 30
```

验证脚本要求地面端日志出现 `[GROUND_RX]`，并同时出现 `VD-001`、`VD-002`、`VD-003`。
