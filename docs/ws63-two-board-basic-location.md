# 两块 WS63 基础定位版说明

## 1. 目标

只使用两块 WS63 星闪板子，先完成灾后救援的最小闭环：

- 受灾端：周期发送模拟生命体征数据。
- 救援端：自动扫描受灾端、连接、接收生命体征。
- 救援端：读取 RSSI，估算距离等级和靠近/远离趋势。
- 串口输出：先不依赖屏幕，能在串口助手里看到定位基础结果。

注意：两块板子不能直接得到稳定二维坐标，也不能天然给出绝对方向。基础版定位是：

```text
发现目标 + 粗距离估算 + 靠近/远离趋势
```

后续接屏幕时，可以把 `distance` 和 `trend` 显示成：

```text
VD-001
Distance: 3.8 m
Trend: CLOSING
SpO2: 88%
HR: 116 bpm
```

## 2. 已修改文件

受灾端工程：

```text
xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/vendor/BearPi-Pico_H3863/products/sle_uart/sle_uart.c
```

救援端工程：

```text
xingshan/fbb_ws63-jiuyuanduan/fbb_ws63-master/vendor/BearPi-Pico_H3863/products/sle_uart/sle_uart.c
xingshan/fbb_ws63-jiuyuanduan/fbb_ws63-master/vendor/BearPi-Pico_H3863/products/sle_uart/sle_uart_client/sle_uart_client.c
xingshan/fbb_ws63-jiuyuanduan/fbb_ws63-master/vendor/BearPi-Pico_H3863/products/sle_uart/sle_uart_client/sle_uart_client.h
```

## 3. 角色配置

## 3.0 HiSpark Studio 导入工程

如果点击 IDE 顶部“重编译”没有反应，先确认当前打开的是本项目里的 `.hiproj`，不要使用欢迎页里旧的历史工程。

救援端导入：

```text
D:\table\jiuyuan\xingshan\fbb_ws63-jiuyuanduan\fbb_ws63-master\src\jiuyuan-rescuer.hiproj
```

受灾端导入：

```text
D:\table\jiuyuan\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src\jiuyuan-victim.hiproj
```

导入后再执行：

```text
工程配置 / MenuConfig -> 选择 SLE UART 示例 -> 保存 -> 重编译
```

### 3.1 受灾端

目录：

```text
xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master
```

SLE UART 选择：

```text
CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER=y
CONFIG_SAMPLE_SUPPORT_NORMAL_TYPE=y
```

受灾端会作为 SLE server 广播，连接后每秒发送一条模拟数据：

```json
{"type":"VICTIM","id":"VD-001","seq":0,"spo2":88,"hr":116,"sbp":96,"battery":82,"sos":1}
```

串口日志关键字：

```text
[RESCUE_VICTIM]
```

### 3.2 救援端

目录：

```text
xingshan/fbb_ws63-jiuyuanduan/fbb_ws63-master
```

SLE UART 选择：

```text
CONFIG_SAMPLE_SUPPORT_SLE_UART_CLIENT=y
CONFIG_SAMPLE_SUPPORT_NORMAL_TYPE=y
```

救援端会扫描名为 `sle_uart_server` 的受灾端，连接后每秒读取 RSSI，并输出距离估算。

串口日志关键字：

```text
[RESCUE_TRACK]
```

## 4. 串口输出示例

救援端串口预期输出：

```text
[RESCUE_TRACK] target found by scan rssi=-58 distance=8.4m level=NEAR
[RESCUE_TRACK] rssi=-61 distance=10.8m level=NEAR
[RESCUE_TRACK] rssi=-61 distance=10.8m level=NEAR trend=STABLE payload={"type":"VICTIM","id":"VD-001","seq":12,"spo2":90,"hr":124,"sbp":100,"battery":80,"sos":1}
```

`trend` 含义：

| 值 | 含义 |
| --- | --- |
| CLOSING | 救援端正在接近受灾端 |
| AWAY | 救援端正在远离受灾端 |
| STABLE | 距离变化不明显 |

`level` 含义：

| 值 | 含义 |
| --- | --- |
| VERY_NEAR | 很近，约 3m 内 |
| NEAR | 较近，适合继续搜索 |
| MID | 中等距离 |
| FAR | 较远或遮挡严重 |
| UNKNOWN | RSSI 无效 |

## 5. 距离估算逻辑

基础版用 RSSI 估算距离，代码在救援端：

```text
rescue_estimate_distance_dm()
```

这个估算不是精确测距，受墙体、人体遮挡、天线方向影响很大。它适合做“越走越近/越走越远”的搜救趋势判断。

如果后续你拿到 SLP 或 HADM 测距 API，可以直接替换这个函数的数据来源：

```text
RSSI distance estimate
=> SLP ranging distance
```

前端、串口显示和任务逻辑都不用大改。

## 6. 两块板子的演示方法

1. 烧录受灾端固件到第一块 WS63。
2. 烧录救援端固件到第二块 WS63。
3. 打开两个串口助手，波特率按工程配置，通常是 `115200`。
4. 先启动受灾端，再启动救援端。
5. 救援端看到 `target found by scan` 后，开始移动救援端。
6. 观察 `distance` 和 `trend`：
   - 靠近受灾端时，趋势应更常出现 `CLOSING`。
   - 远离受灾端时，趋势应更常出现 `AWAY`。

## 7. 下一步接屏幕

救援端现在已经能得到：

```text
id
spo2
hr
sbp
battery
sos
rssi
distance
level
trend
```

接 OLED/TFT 后，只需要在 `rescue_print_target_state()` 附近把最新状态保存到全局结构体，再由屏幕刷新任务显示即可。

推荐屏幕内容：

```text
TARGET VD-001
DIST  10.8 m
TREND CLOSING
SpO2  90%
HR    124 bpm
SOS   YES
```

## 8. 后续升级路线

基础版：

```text
SLE 连接 + RSSI 粗距离 + 趋势
```

增强版：

```text
SLE 连接 + SLP/HADM 测距 + 屏幕箭头
```

工程版：

```text
两板测距 + IMU 运动轨迹 + 后台 WebSocket + 多锚点定位
```
