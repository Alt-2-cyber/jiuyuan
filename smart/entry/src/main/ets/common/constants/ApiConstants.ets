/**
 * API 端点常量
 * 后端 Spring Boot 服务地址
 */
export class ApiConstants {
  // 模拟器/真机访问开发电脑时，填电脑当前局域网 IP
  static BASE_URL: string = 'http://192.168.1.18:8088';
  static readonly FALLBACK_URLS: string[] = [
    'http://192.168.1.18:8088',
    'http://127.0.0.1:8088',
    'http://10.0.2.2:8088'
  ];

  // ========== 救援业务接口 ==========
  /** 态势总览 */
  static readonly OVERVIEW: string = '/api/overview';
  /** 地图数据 */
  static readonly MAP: string = '/api/map';
  /** 受灾人员列表 */
  static readonly VICTIMS: string = '/api/victims';
  /** 单个受灾人员详情 */
  static readonly VICTIM_DETAIL: string = '/api/victims/';
  /** 救援人员列表 */
  static readonly RESCUERS: string = '/api/rescuers';
  /** 设备列表 */
  static readonly DEVICES: string = '/api/devices';
  /** 告警列表 */
  static readonly ALERTS: string = '/api/alerts';
  /** 任务列表 */
  static readonly TASKS: string = '/api/tasks';
  /** 任务派发 */
  static readonly TASK_ASSIGN: string = '/api/tasks/';

  // ========== AI 智能接口 ==========
  /** 伤病智能分析 */
  static readonly AI_INJURY_ANALYSIS: string = '/api/ai/injury-analysis';
  /** 救援路径优化 */
  static readonly AI_ROUTE_OPTIMIZE: string = '/api/ai/route-optimize';
  /** 生命体征解读 */
  static readonly AI_VITALS_INTERPRET: string = '/api/ai/vitals-interpret';
  /** 环境风险评估 */
  static readonly AI_ENV_ASSESSMENT: string = '/api/ai/environment-assessment';
  /** 综合灾情报告 */
  static readonly AI_GENERATE_REPORT: string = '/api/ai/generate-report';
  /** 通用AI对话 */
  static readonly AI_CHAT: string = '/api/ai/chat';
  /** 通用AI流式对话 */
  static readonly AI_CHAT_STREAM: string = '/api/ai/chat/stream';
}
