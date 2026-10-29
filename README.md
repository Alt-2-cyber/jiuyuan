# 基于星闪互联的灾后救援系统

这是一个面向灾后搜救场景的星闪 NearLink 项目方案，目标是实现：

- 受灾人员佩戴星闪生命监护终端。
- 救援人员使用带屏幕的星闪导航终端定位受灾人员。
- 救援终端显示目标距离、方向箭头、生命状态和现场环境风险。
- Web 指挥后台基于 Spring Boot、Vue3、MySQL 展示全局态势并分析救援优先级。

## 文档入口

- [完整开发文档](docs/starflash-rescue-development-guide.md)
- [当前五节点固件角色说明](救援系统代码角色说明.md)

## 源码与仓库范围

- `backend/`：Spring Boot 后端及数据库脚本。
- `frontend/`：Vue3 指挥后台。
- `smart/`：鸿蒙 ArkTS 应用。
- `xingshan/fbb_ws63-shouzaiduan/`：当前地面、天空及三个受灾端共用的五节点固件工程。
- `xingshan/fbb_ws63-jiuyuanduan/`：保留的旧救援端工程，角色区别见上方说明。
- `delivery/`、`scripts/`：构建、烧录、监控及交付脚本。
- `无人机视频/yolo_people_annotate.py`：无人机视频人员识别脚本。

仓库保留源码、SDK 必要库、许可证和开发文档；视频、原始录制、文档成品、
虚拟环境、编译器安装包、构建缓存及比赛提交副本留在本地，不随源码上传。
第三方 SDK 和模型按各自原许可证使用；SDK 许可证位于对应工程根目录。
构建 WS63 固件前请按 SDK 文档安装 HiSpark Studio 与配套工具链。

部署变量参考 `.env.example`，在终端或服务环境中设置，项目不会自动加载该示例文件。
真实口令、云平台密钥和设备本地设置不得写入提交。
固件中的 WiFi/MQTT 配置由对应源码旁的 `*.local.h` 提供；从仓库克隆后，
先将相邻的 `*.example.h` 复制为同名 `*.local.h`，填好参数再编译。
本机已有参数与整理前的文件分别保存在被忽略的本地头文件及 `.publication-private/` 中。

## 已完成内容

- `backend`：Spring Boot 3 管理端接口服务，内置演练态势数据、人员、设备、告警、任务和调度接口。
- `frontend`：Vue3 + TypeScript 管理后台，包含态势总览、受灾人员、救援人员、任务调度、告警中心、设备管理、数据分析、系统配置等页面。
- `backend/src/main/resources/schema-mysql.sql`：MySQL 建表脚本，便于后续接真实数据库。

## 本地运行

后端：

```bash
cd backend
mvn spring-boot:run
```

后端默认地址：

```text
http://localhost:8088
```

前端：

```bash
cd frontend
npm install
npm run dev
```

前端默认地址：

```text
http://localhost:5173
```

常用接口：

```text
GET  /api/overview
GET  /api/map
GET  /api/victims
GET  /api/rescuers
GET  /api/devices
GET  /api/alerts
GET  /api/tasks
POST /api/tasks/{taskId}/assign
POST /api/device-ingest/rescue-terminal
```

## 推荐落地路线

1. 两块星闪板子完成互联、广播、测距和屏幕距离显示。
2. 接入血氧、心率、血压趋势、温湿度、可燃气体等传感器。
3. 实现救援人员终端的方向箭头、环境报警和任务锁定。
4. 建设 Spring Boot 后端、MySQL 数据库、Vue3 指挥后台。
5. 加入现场网关、多锚点定位、断网缓存、救援复盘和工程校准流程。

## 技术栈

- 设备端：星闪 NearLink、PPG 生命体征传感器、IMU、环境传感器、屏幕 UI。
- 后端：Spring Boot 3、MySQL 8、Redis、MQTT、WebSocket。
- 前端：Vue3、TypeScript、Pinia、ECharts、地图/平面图组件。
- 算法：测距滤波、IMU 融合、目标方向估计、救援优先级评分。
