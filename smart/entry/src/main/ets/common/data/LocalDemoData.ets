import {
  AlertEvent,
  Device,
  DeviceStatus,
  EnvironmentSample,
  LocationPoint,
  MetricCard,
  RescueTask,
  Rescuer,
  RiskLevel,
  SiteOverview,
  TaskStatus,
  TrendPoint,
  Victim,
  VitalSigns
} from '../models/DataModels';

export class LocalDemoData {
  static overview(): SiteOverview {
    const victims = LocalDemoData.victims();
    const alerts = LocalDemoData.alerts();
    const rescuers = LocalDemoData.rescuers();
    return {
      siteId: 'SITE-ALPHA',
      siteName: '星闪灾后救援联合演练现场',
      phaseLabel: '72 小时黄金救援 / 第 18 小时',
      phase: '72 小时黄金救援 / 第 18 小时',
      snapshotTime: LocalDemoData.nowText(),
      serverTime: LocalDemoData.nowText(),
      metrics: LocalDemoData.metrics(victims, alerts),
      distribution: LocalDemoData.distribution(victims),
      riskDistribution: LocalDemoData.distribution(victims),
      trends: LocalDemoData.trends(),
      topVictims: victims,
      priorityQueue: victims,
      latestAlerts: alerts,
      activeRescuers: rescuers
    };
  }

  static victims(): Victim[] {
    return [
      LocalDemoData.victim('V-001', '受灾者 01', 62, '男', 'VD-001', RiskLevel.RED, 94.5,
        LocalDemoData.vitals(84, 146, 88, 54, 36.2, '长时间静止', 0.78, 71),
        LocalDemoData.location(70, 20, 18.4, 3.2, 38), 'R-002', 'T-0002', '已派发',
        ['SpO2 84% 严重偏低', '心率 146 bpm', '收缩压 88 mmHg', '距离 R-002 18.4m']),
      LocalDemoData.victim('V-002', '受灾者 02', 34, '女', 'VD-002', RiskLevel.ORANGE, 78.2,
        LocalDemoData.vitals(89, 122, 96, 61, 37.1, '微弱活动', 0.74, 63),
        LocalDemoData.location(48, 18, 28.2, 2.8, 19), null, 'T-0003', '待派发',
        ['SpO2 89% 偏低', '疑似受困但仍有微弱活动', '定位质量中等']),
      LocalDemoData.victim('V-003', '受灾者 03', 27, '男', 'VD-003', RiskLevel.RED, 91.0,
        LocalDemoData.vitals(86, 138, 92, 58, 38.5, 'SOS', 0.86, 82),
        LocalDemoData.location(34, 46, 11.7, 1.9, 344), 'R-001', 'T-0001', '救援中',
        ['主动 SOS', 'SpO2 86% 偏低', '距离 R-001 11.7m']),
      LocalDemoData.victim('V-004', '受灾者 04', 45, '女', 'VD-004', RiskLevel.YELLOW, 54.4,
        LocalDemoData.vitals(93, 112, 108, 68, 36.9, '静止', 0.69, 55),
        LocalDemoData.location(18, 56, 22.6, 3.8, 82), null, 'T-0005', '待确认',
        ['生命体征轻度异常', '信号质量偏低，需要复测']),
      LocalDemoData.victim('V-005', '受灾者 05', 19, '男', 'VD-005', RiskLevel.GREEN, 28.1,
        LocalDemoData.vitals(97, 88, 116, 72, 36.6, '可移动', 0.91, 88),
        LocalDemoData.location(16, 16, 14.2, 1.6, 231), null, null, '等待转移',
        ['生命体征稳定', '位置靠近安全通道']),
      LocalDemoData.victim('V-006', '受灾者 06', 51, '男', 'VD-006', RiskLevel.ORANGE, 82.6,
        LocalDemoData.vitals(91, 132, 102, 64, 37.9, '静止', 0.81, 47),
        LocalDemoData.location(96, 48, 8.5, 2.2, 301), 'R-004', 'T-0004', '接近中',
        ['附近可燃气体 22.8% LEL', '心率 132 bpm', '最近救援员 8.5m']),
      LocalDemoData.victim('V-007', '受灾者 07', 38, '女', 'VD-007', RiskLevel.GRAY, 48.9,
        LocalDemoData.vitals(0, 0, 0, 0, 0, '信号中断', 0.12, 29),
        LocalDemoData.location(84, 18, 41.5, 8.5, 0), null, 'T-0006', '需复核',
        ['生命体征数据中断超过 120 秒', '设备低电量', '定位误差较大']),
      LocalDemoData.victim('V-008', '受灾者 08', 56, '女', 'VD-008', RiskLevel.YELLOW, 58.7,
        LocalDemoData.vitals(92, 118, 98, 59, 37.4, '微弱活动', 0.72, 64),
        LocalDemoData.location(104, 58, 18.9, 3.1, 284), null, null, '待派发',
        ['血氧临界', '处于管廊风险区边缘'])
    ];
  }

  static rescuers(): Rescuer[] {
    return [
      LocalDemoData.rescuer('R-001', '陈序', '突入一组', 'RD-001', DeviceStatus.ONLINE,
        LocalDemoData.location(24, 48, 0, 1.8, 42),
        LocalDemoData.env(39.5, 76, 6.2, 18, 20.5, RiskLevel.GREEN), 'T-0001', 'V-003', 84),
      LocalDemoData.rescuer('R-002', '林骁', '突入一组', 'RD-002', DeviceStatus.WARNING,
        LocalDemoData.location(64, 24, 0, 2.3, 274),
        LocalDemoData.env(47.8, 88, 18.4, 42, 20.1, RiskLevel.ORANGE), 'T-0002', 'V-001', 67),
      LocalDemoData.rescuer('R-003', '周宁', '医疗转运组', 'RD-003', DeviceStatus.ONLINE,
        LocalDemoData.location(18, 18, 0, 1.5, 126),
        LocalDemoData.env(36.2, 72, 4.1, 12, 20.8, RiskLevel.GREEN), null, null, 91),
      LocalDemoData.rescuer('R-004', '邵远', '侦检组', 'RD-004', DeviceStatus.ONLINE,
        LocalDemoData.location(92, 50, 0, 2.7, 315),
        LocalDemoData.env(55.1, 90, 22.8, 64, 19.7, RiskLevel.RED), 'T-0004', 'V-006', 58)
    ];
  }

  static alerts(): AlertEvent[] {
    return [
      LocalDemoData.alert('AL-001', 'VITAL_CRITICAL', RiskLevel.RED, '受灾者 01 低血氧高心率',
        'SpO2 84%，心率 146 bpm，建议立即救援并准备氧疗。', 'V-001', 'VD-001', 'ACTIVE'),
      LocalDemoData.alert('AL-002', 'SOS', RiskLevel.RED, '受灾者 03 主动 SOS',
        '腕带连续触发 SOS，距离救援员 R-001 约 11.7m。', 'V-003', 'VD-003', 'ACKED'),
      LocalDemoData.alert('AL-003', 'ENV_DANGER', RiskLevel.RED, 'C2 管廊可燃气体危险',
        'R-004 检测到 22.8% LEL，可燃气体超过撤离阈值。', 'R-004', 'RD-004', 'ACTIVE'),
      LocalDemoData.alert('AL-004', 'DEVICE_LOW_BATTERY', RiskLevel.YELLOW, '受灾者 07 设备低电量',
        'VD-007 电量 29%，生命体征数据已中断超过 120 秒。', 'V-007', 'VD-007', 'ACTIVE'),
      LocalDemoData.alert('AL-005', 'ENV_WARNING', RiskLevel.ORANGE, 'A2 商铺走廊气体升高',
        'R-002 检测到 18.4% LEL，建议侦检组确认。', 'R-002', 'RD-002', 'ACKED'),
      LocalDemoData.alert('AL-006', 'LOCATION_LOW_QUALITY', RiskLevel.YELLOW, '受灾者 04 定位质量偏低',
        '定位误差 3.8m，建议救援终端移动扫描复测。', 'V-004', 'VD-004', 'ACTIVE')
    ];
  }

  static tasks(): RescueTask[] {
    return [
      { id: 'T-0001', victimId: 'V-003', rescuerId: 'R-001', status: TaskStatus.IN_PROGRESS, priority: RiskLevel.RED, priorityScore: 91.0, reason: 'SOS + 低血氧，距离最近', createdAt: LocalDemoData.nowText(), assignedAt: LocalDemoData.nowText(), completedAt: null },
      { id: 'T-0002', victimId: 'V-001', rescuerId: 'R-002', status: TaskStatus.ASSIGNED, priority: RiskLevel.RED, priorityScore: 94.5, reason: 'SpO2 84%，心率 146，血压偏低', createdAt: LocalDemoData.nowText(), assignedAt: LocalDemoData.nowText(), completedAt: null },
      { id: 'T-0003', victimId: 'V-002', rescuerId: null, status: TaskStatus.PENDING, priority: RiskLevel.ORANGE, priorityScore: 78.2, reason: '血氧 89%，仍有微弱活动', createdAt: LocalDemoData.nowText(), assignedAt: null, completedAt: null },
      { id: 'T-0004', victimId: 'V-006', rescuerId: 'R-004', status: TaskStatus.PAUSED, priority: RiskLevel.ORANGE, priorityScore: 82.6, reason: '附近可燃气体超 20% LEL，等待侦检确认', createdAt: LocalDemoData.nowText(), assignedAt: LocalDemoData.nowText(), completedAt: null },
      { id: 'T-0005', victimId: 'V-004', rescuerId: null, status: TaskStatus.PENDING, priority: RiskLevel.YELLOW, priorityScore: 54.4, reason: '生命体征轻度异常，定位质量偏低', createdAt: LocalDemoData.nowText(), assignedAt: null, completedAt: null },
      { id: 'T-0006', victimId: 'V-007', rescuerId: null, status: TaskStatus.PENDING, priority: RiskLevel.GRAY, priorityScore: 48.9, reason: '设备失联，需复核', createdAt: LocalDemoData.nowText(), assignedAt: null, completedAt: null }
    ];
  }

  static devices(): Device[] {
    const result: Device[] = [];
    const victims = LocalDemoData.victims();
    const rescuers = LocalDemoData.rescuers();
    for (let i = 0; i < victims.length; i++) {
      const victim = victims[i];
      result.push({
        id: victim.deviceId,
        name: `受灾人员星闪生命腕带 ${victim.id.substring(2)}`,
        type: 'VICTIM_TAG',
        assignedTo: victim.id,
        status: victim.level === RiskLevel.GRAY ? DeviceStatus.WARNING : DeviceStatus.ONLINE,
        battery: victim.vitals.battery,
        version: 'v0.8.2',
        signalQuality: victim.vitals.signalQuality > 0.75 ? '强' : '中',
        lastSeenAt: victim.lastSeenAt
      });
    }
    for (let i = 0; i < rescuers.length; i++) {
      const rescuer = rescuers[i];
      result.push({
        id: rescuer.deviceId,
        name: `救援人员星闪导航终端 ${rescuer.id.substring(2)}`,
        type: 'RESCUER_TERMINAL',
        assignedTo: rescuer.id,
        status: rescuer.status,
        battery: rescuer.battery,
        version: 'v0.9.4',
        signalQuality: rescuer.environment.riskLevel === RiskLevel.RED ? '受干扰' : '强',
        lastSeenAt: rescuer.lastSeenAt
      });
    }
    result.push({ id: 'GW-001', name: '便携星闪网关 A', type: 'GATEWAY', assignedTo: '现场指挥组', status: DeviceStatus.ONLINE, battery: 96, version: 'v0.7.9', signalQuality: '强', lastSeenAt: LocalDemoData.nowText() });
    result.push({ id: 'A-001', name: '星闪定位锚点 1', type: 'ANCHOR', assignedTo: 'A1 区', status: DeviceStatus.ONLINE, battery: 78, version: 'v0.6.1', signalQuality: '强', lastSeenAt: LocalDemoData.nowText() });
    result.push({ id: 'A-002', name: '星闪定位锚点 2', type: 'ANCHOR', assignedTo: 'A2 区', status: DeviceStatus.ONLINE, battery: 73, version: 'v0.6.1', signalQuality: '中', lastSeenAt: LocalDemoData.nowText() });
    result.push({ id: 'A-003', name: '星闪定位锚点 3', type: 'ANCHOR', assignedTo: 'C2 区', status: DeviceStatus.WARNING, battery: 51, version: 'v0.6.1', signalQuality: '中', lastSeenAt: LocalDemoData.nowText() });
    return result;
  }

  static victimById(id: string): Victim | null {
    const all = LocalDemoData.victims();
    for (let i = 0; i < all.length; i++) {
      if (all[i].id === id) {
        return all[i];
      }
    }
    return all.length > 0 ? all[0] : null;
  }

  private static metrics(victims: Victim[], alerts: AlertEvent[]): MetricCard[] {
    let red = 0;
    let orange = 0;
    for (let i = 0; i < victims.length; i++) {
      if (victims[i].level === RiskLevel.RED) {
        red++;
      } else if (victims[i].level === RiskLevel.ORANGE) {
        orange++;
      }
    }
    let activeAlerts = 0;
    for (let i = 0; i < alerts.length; i++) {
      if (alerts[i].status !== 'RESOLVED') {
        activeAlerts++;
      }
    }
    return [
      { label: '受灾人员', value: `${victims.length}`, unit: '人', hint: '本地演示数据', trend: '本地演示数据', level: red > 0 ? RiskLevel.RED : RiskLevel.GREEN },
      { label: '高危待救', value: `${red + orange}`, unit: '人', hint: `红色 ${red} / 橙色 ${orange}`, trend: `红色 ${red} / 橙色 ${orange}`, level: red > 0 ? RiskLevel.RED : RiskLevel.ORANGE },
      { label: '在线设备', value: '16', unit: '台', hint: '链路稳定率 94%', trend: '链路稳定率 94%', level: RiskLevel.GREEN },
      { label: '活跃告警', value: `${activeAlerts}`, unit: '条', hint: '2 条环境风险', trend: '2 条环境风险', level: activeAlerts > 4 ? RiskLevel.RED : RiskLevel.YELLOW }
    ];
  }

  private static distribution(victims: Victim[]): Record<string, number> {
    const result = {} as Record<string, number>;
    result['RED'] = 0;
    result['ORANGE'] = 0;
    result['YELLOW'] = 0;
    result['GREEN'] = 0;
    result['GRAY'] = 0;
    for (let i = 0; i < victims.length; i++) {
      const key: string = victims[i].level;
      result[key] = (result[key] ?? 0) + 1;
    }
    return result;
  }

  private static trends(): TrendPoint[] {
    const spo2: number[] = [96, 95, 94, 93, 91, 90, 88, 87, 86, 85, 86, 84];
    const hr: number[] = [88, 92, 96, 102, 110, 118, 126, 132, 136, 142, 138, 146];
    const gas: number[] = [3.2, 3.8, 4.4, 5.2, 7.8, 9.6, 11.2, 13.8, 15.6, 18.4, 21.2, 22.8];
    const alert: number[] = [1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 6];
    const result: TrendPoint[] = [];
    for (let i = 0; i < spo2.length; i++) {
      const hour = (8 + Math.floor(i / 2)).toString().padStart(2, '0');
      const minute = ((i % 2) * 30).toString().padStart(2, '0');
      result.push({
        time: `${hour}:${minute}`,
        spo2: spo2[i],
        heartRate: hr[i],
        gas: gas[i],
        alertCount: alert[i],
        alerts: alert[i]
      });
    }
    return result;
  }

  private static victim(id: string, name: string, age: number, gender: string, deviceId: string, level: RiskLevel,
    priorityScore: number, vitals: VitalSigns, location: LocationPoint, assignedRescuerId: string | null,
    taskId: string | null, status: string, reasons: string[]): Victim {
    return { id, name, age, gender, deviceId, level, priorityScore, vitals, location, assignedRescuerId, taskId, status, reasons, lastSeenAt: LocalDemoData.nowText() };
  }

  private static rescuer(id: string, name: string, team: string, deviceId: string, status: DeviceStatus,
    location: LocationPoint, environment: EnvironmentSample, taskId: string | null, victimId: string | null, battery: number): Rescuer {
    return { id, name, team, deviceId, status, location, environment, taskId, currentTaskId: taskId, victimId, targetVictimId: victimId, battery, lastSeenAt: LocalDemoData.nowText() };
  }

  private static alert(id: string, type: string, level: RiskLevel, title: string, detail: string, targetId: string, deviceId: string, status: string): AlertEvent {
    return { id, type, level, severity: level, title, detail, targetId, personId: targetId, deviceId, status, firstSeenAt: LocalDemoData.nowText(), lastSeenAt: LocalDemoData.nowText() };
  }

  private static vitals(spo2: number, heartRate: number, systolic: number, diastolic: number, temperature: number,
    motionState: string, signalQuality: number, battery: number): VitalSigns {
    return { spo2, heartRate, systolic, diastolic, temperature, motionState, signalQuality, battery };
  }

  private static location(x: number, y: number, distanceToNearestRescuer: number, accuracy: number, azimuth: number): LocationPoint {
    return { x, y, z: 0, distanceToNearestRescuer, accuracy, azimuth };
  }

  private static env(temperature: number, humidity: number, combustibleGas: number, co: number, o2: number, riskLevel: RiskLevel): EnvironmentSample {
    return { temperature, humidity, combustibleGas, gasUnit: '%LEL', combustibleUnit: '%LEL', co, o2, oxygen: o2, riskLevel };
  }

  private static nowText(): string {
    const d = new Date();
    const p = (n: number): string => n.toString().padStart(2, '0');
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
  }
}
