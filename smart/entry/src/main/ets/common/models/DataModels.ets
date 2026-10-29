/**
 * 数据模型 - 与 Spring Boot 后端 Java Records 完全对应
 */

/** 风险等级 */
export enum RiskLevel {
  RED = 'RED',
  ORANGE = 'ORANGE',
  YELLOW = 'YELLOW',
  GREEN = 'GREEN',
  GRAY = 'GRAY'
}

/** 设备状态 */
export enum DeviceStatus {
  ONLINE = 'ONLINE',
  WARNING = 'WARNING',
  OFFLINE = 'OFFLINE'
}

/** 任务状态 */
export enum TaskStatus {
  PENDING = 'PENDING',
  ASSIGNED = 'ASSIGNED',
  IN_PROGRESS = 'IN_PROGRESS',
  PAUSED = 'PAUSED',
  COMPLETED = 'COMPLETED',
  CANCELLED = 'CANCELLED'
}

/** 生命体征 */
export interface VitalSigns {
  spo2: number;           // 血氧饱和度 (%)
  heartRate: number;      // 心率 (bpm)
  systolic: number;       // 收缩压 (mmHg)
  diastolic: number;      // 舒张压 (mmHg)
  temperature: number;    // 体温 (℃)
  motionState: string;    // 活动状态
  signalQuality: number;  // 信号质量 (0-1)
  battery: number;        // 电量 (%)
}

/** 位置坐标 */
export interface LocationPoint {
  x: number;
  y: number;
  z: number;
  distanceToNearestRescuer: number;  // 距最近救援员 (m)
  accuracy: number;                   // 定位精度 (m)
  azimuth: number;                    // 方位角 (度)
}

/** 环境样本 */
export interface EnvironmentSample {
  temperature: number;   // 环境温度 (℃)
  humidity: number;      // 湿度 (%)
  combustibleGas: number; // 可燃气体 (%LEL)
  gasUnit: string;        // 气体单位
  combustibleUnit?: string; // 后端字段别名
  co: number;             // 一氧化碳 (ppm)
  o2: number;             // 氧气 (%)
  oxygen?: number;         // 后端字段别名
  riskLevel: RiskLevel;   // 环境风险等级
}

/** 受灾人员 */
export interface Victim {
  id: string;
  name: string;
  age: number;
  gender: string;
  deviceId: string;
  level: RiskLevel;
  priorityScore: number;
  vitals: VitalSigns;
  location: LocationPoint;
  assignedRescuerId: string | null;
  taskId: string | null;
  status: string;
  reasons: string[];
  lastSeenAt: string;
}

/** 救援人员 */
export interface Rescuer {
  id: string;
  name: string;
  team: string;
  deviceId: string;
  status: DeviceStatus;
  location: LocationPoint;
  environment: EnvironmentSample;
  taskId: string | null;
  currentTaskId?: string | null;
  victimId: string | null;
  targetVictimId?: string | null;
  battery: number;
  lastSeenAt: string;
}

/** 设备 */
export interface Device {
  id: string;
  name: string;
  type: string;
  assignedTo: string;
  ownerId?: string;
  status: DeviceStatus;
  battery: number;
  version: string;
  firmwareVersion?: string;
  signalQuality: string;
  linkQuality?: string;
  lastSeenAt: string;
}

/** 告警事件 */
export interface AlertEvent {
  id: string;
  type: string;
  level: RiskLevel;
  severity?: RiskLevel;
  title: string;
  detail: string;
  targetId: string;
  personId?: string;
  deviceId: string;
  status: string;
  firstSeenAt: string;
  lastSeenAt: string;
}

/** 救援任务 */
export interface RescueTask {
  id: string;
  victimId: string;
  rescuerId: string | null;
  status: TaskStatus;
  priority: RiskLevel;
  priorityScore: number;
  reason: string;
  createdAt: string;
  assignedAt: string | null;
  completedAt: string | null;
}

/** 趋势数据点 */
export interface TrendPoint {
  time: string;
  spo2: number;
  heartRate: number;
  gas: number;
  alertCount: number;
  alerts?: number;
}

/** 指标卡片 */
export interface MetricCard {
  label: string;
  value: string;
  unit: string;
  hint: string;
  trend?: string;
  level: RiskLevel;
}

/** 态势总览 */
export interface SiteOverview {
  siteId: string;
  siteName: string;
  phaseLabel: string;
  phase?: string;
  snapshotTime: string;
  serverTime?: string;
  metrics: MetricCard[];
  distribution: Record<string, number>;
  riskDistribution?: Record<string, number>;
  trends: TrendPoint[];
  topVictims: Victim[];
  priorityQueue?: Victim[];
  latestAlerts: AlertEvent[];
  activeRescuers: Rescuer[];
}

/** AI响应 */
export interface AiResponse {
  type: string;
  result: string;
}

/** AI 伤病分析请求 */
export interface InjuryAnalysisRequest {
  name: string;
  age: number;
  gender: string;
  spo2: number;
  heartRate: number;
  systolic: number;
  diastolic: number;
  temperature: number;
  motionState: string;
  level: string;
  priorityScore: number;
  reasons: string[];
}

/** AI 路径优化请求 */
export interface RouteOptimizeRequest {
  victimX: number;
  victimY: number;
  rescuerX: number;
  rescuerY: number;
  distance: number;
  zone: string;
  envTemp: number;
  envHumidity: number;
  gasLevel: number;
  co: number;
  o2: number;
}

/** AI 体征趋势请求 */
export interface VitalsTrendRequest {
  name: string;
  duration: string;
  spo2Trend: string;
  hrTrend: string;
  currentSpo2: number;
  currentHr: number;
  minSpo2: number;
  maxHr: number;
}

/** AI 环境评估请求 */
export interface EnvironmentAssessmentRequest {
  location: string;
  temperature: number;
  humidity: number;
  combustibleGas: number;
  co: number;
  o2: number;
  riskLevel: string;
}

/** AI 综合报告请求 */
export interface ReportRequest {
  siteName: string;
  victims: number;
  rescuers: number;
  redAlerts: number;
  orangeAlerts: number;
  yellowAlerts: number;
}

/** AI 聊天请求 */
export interface ChatRequest {
  message: string;
  history?: ChatHistoryMessage[];
}

/** AI 上下文消息 */
export interface ChatHistoryMessage {
  role: 'user' | 'assistant' | 'ai';
  content: string;
}

/** 聊天消息 */
export interface ChatMessage {
  role: 'user' | 'ai' | 'system';
  content: string;
  time: string;
  type?: string;
}
