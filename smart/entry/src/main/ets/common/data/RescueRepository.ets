import { ApiConstants } from '../constants/ApiConstants';
import {
  AlertEvent,
  Device,
  EnvironmentSample,
  RescueTask,
  Rescuer,
  RiskLevel,
  SiteOverview,
  Victim
} from '../models/DataModels';
import { HttpUtils } from '../utils/HttpUtils';
import { LocalDemoData } from './LocalDemoData';

export class RescueRepository {
  static offline: boolean = false;

  static async overview(): Promise<SiteOverview> {
    return RescueRepository.fetch(ApiConstants.OVERVIEW, (): SiteOverview => LocalDemoData.overview());
  }

  static async victims(): Promise<Victim[]> {
    return RescueRepository.fetch(ApiConstants.VICTIMS, (): Victim[] => LocalDemoData.victims());
  }

  static async victim(id: string): Promise<Victim | null> {
    try {
      const data = await HttpUtils.get<Victim>(`${ApiConstants.VICTIM_DETAIL}${id}`);
      RescueRepository.offline = false;
      return RescueRepository.normalizeVictim(data);
    } catch (err) {
      console.error(`RescueRepository victim ${id} 失败: ${JSON.stringify(err)}`);
      const all = await RescueRepository.victims();
      for (let i = 0; i < all.length; i++) {
        if (all[i].id === id) {
          return all[i];
        }
      }
      return LocalDemoData.victimById(id);
    }
  }

  static async rescuers(): Promise<Rescuer[]> {
    const list = await RescueRepository.fetch(ApiConstants.RESCUERS, (): Rescuer[] => LocalDemoData.rescuers());
    const result: Rescuer[] = [];
    for (let i = 0; i < list.length; i++) {
      result.push(RescueRepository.normalizeRescuer(list[i]));
    }
    return result;
  }

  static async tasks(): Promise<RescueTask[]> {
    return RescueRepository.fetch(ApiConstants.TASKS, (): RescueTask[] => LocalDemoData.tasks());
  }

  static async alerts(): Promise<AlertEvent[]> {
    const list = await RescueRepository.fetch(ApiConstants.ALERTS, (): AlertEvent[] => LocalDemoData.alerts());
    const result: AlertEvent[] = [];
    for (let i = 0; i < list.length; i++) {
      result.push(RescueRepository.normalizeAlert(list[i]));
    }
    return result;
  }

  static async devices(): Promise<Device[]> {
    const list = await RescueRepository.fetch(ApiConstants.DEVICES, (): Device[] => LocalDemoData.devices());
    const result: Device[] = [];
    for (let i = 0; i < list.length; i++) {
      result.push(RescueRepository.normalizeDevice(list[i]));
    }
    return result;
  }

  private static async fetch<T>(path: string, fallback: () => T): Promise<T> {
    try {
      const data = await HttpUtils.get<T>(path);
      RescueRepository.offline = false;
      return data;
    } catch (err) {
      console.error(`RescueRepository ${path} 失败，使用本地演示数据: ${JSON.stringify(err)}`);
      RescueRepository.offline = true;
      return fallback();
    }
  }

  private static normalizeVictim(item: Victim): Victim {
    return item;
  }

  private static normalizeRescuer(item: Rescuer): Rescuer {
    const env: EnvironmentSample = item.environment ?? {
      temperature: 0,
      humidity: 0,
      combustibleGas: 0,
      gasUnit: '%LEL',
      co: 0,
      o2: 0,
      riskLevel: RiskLevel.GRAY
    };
    return {
      id: item.id,
      name: item.name,
      team: item.team,
      deviceId: item.deviceId,
      status: item.status,
      location: item.location,
      environment: {
        temperature: env.temperature,
        humidity: env.humidity,
        combustibleGas: env.combustibleGas,
        gasUnit: env.gasUnit ?? env.combustibleUnit ?? '%LEL',
        combustibleUnit: env.combustibleUnit ?? env.gasUnit,
        co: env.co,
        o2: env.o2 ?? env.oxygen ?? 0,
        oxygen: env.oxygen ?? env.o2,
        riskLevel: env.riskLevel
      },
      taskId: item.taskId ?? item.currentTaskId ?? null,
      currentTaskId: item.currentTaskId ?? item.taskId ?? null,
      victimId: item.victimId ?? item.targetVictimId ?? null,
      targetVictimId: item.targetVictimId ?? item.victimId ?? null,
      battery: item.battery,
      lastSeenAt: RescueRepository.asText(item.lastSeenAt)
    };
  }

  private static normalizeAlert(item: AlertEvent): AlertEvent {
    const level = item.level ?? item.severity ?? RiskLevel.GRAY;
    const targetId = item.targetId ?? item.personId ?? '--';
    return {
      id: item.id ?? '',
      type: item.type ?? '',
      level: level,
      severity: item.severity ?? level,
      title: item.title ?? '未命名告警',
      detail: item.detail ?? '',
      targetId: targetId,
      personId: item.personId ?? targetId,
      deviceId: item.deviceId ?? '--',
      status: item.status ?? '',
      firstSeenAt: RescueRepository.asText(item.firstSeenAt),
      lastSeenAt: RescueRepository.asText(item.lastSeenAt)
    };
  }

  private static normalizeDevice(item: Device): Device {
    return {
      id: item.id,
      name: item.name,
      type: item.type,
      assignedTo: item.assignedTo ?? item.ownerId ?? '--',
      ownerId: item.ownerId ?? item.assignedTo,
      status: item.status,
      battery: item.battery,
      version: item.version ?? item.firmwareVersion ?? '--',
      firmwareVersion: item.firmwareVersion ?? item.version,
      signalQuality: item.signalQuality ?? item.linkQuality ?? '--',
      linkQuality: item.linkQuality ?? item.signalQuality,
      lastSeenAt: RescueRepository.asText(item.lastSeenAt)
    };
  }

  private static asText(value: string | Object | undefined | null): string {
    if (value === undefined || value === null) {
      return '';
    }
    if (typeof value === 'string') {
      return value;
    }
    return `${value}`;
  }
}
