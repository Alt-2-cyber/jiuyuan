import { router } from '@kit.ArkUI';
import { RescueRepository } from '../common/data/RescueRepository';
import { SiteOverview, MetricCard, RiskLevel, Victim, AlertEvent, TrendPoint } from '../common/models/DataModels';

/**
 * 态势总览 - 救援现场全景仪表盘
 */
@Entry
@Component
struct DashboardPage {
  @State overview: SiteOverview | null = null;
  @State loading: boolean = true;
  @State errorMsg: string = '';
  @State refreshTime: string = '';
  @State offline: boolean = false;

  aboutToAppear(): void {
    this.loadData();
  }

  async loadData(): Promise<void> {
    this.loading = true;
    this.errorMsg = '';
    try {
      const data = await RescueRepository.overview();
      this.overview = this.normalizeOverview(data);
      this.offline = RescueRepository.offline;
      const now = new Date();
      this.refreshTime = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`;
    } catch (err) {
      console.error('加载态势失败:', JSON.stringify(err));
      this.errorMsg = '态势数据加载失败，请检查后端服务和网络连接。';
      this.offline = true;
    } finally {
      this.loading = false;
    }
  }

  normalizeOverview(raw: SiteOverview): SiteOverview {
    const metrics = raw.metrics ?? [];
    const trends = raw.trends ?? [];
    const alerts = raw.latestAlerts ?? [];
    const distribution = raw.distribution ?? raw.riskDistribution ?? {};

    return {
      siteId: raw.siteId ?? '',
      siteName: raw.siteName ?? '星闪救援现场',
      phaseLabel: raw.phaseLabel ?? raw.phase ?? '现场处置中',
      phase: raw.phase,
      snapshotTime: raw.snapshotTime ?? raw.serverTime ?? '',
      serverTime: raw.serverTime,
      metrics: metrics.map((item: MetricCard): MetricCard => {
        return {
          label: item.label ?? '--',
          value: item.value ?? '--',
          unit: item.unit ?? '',
          hint: item.hint ?? item.trend ?? '',
          trend: item.trend,
          level: item.level ?? RiskLevel.GRAY
        };
      }),
      distribution: distribution,
      riskDistribution: raw.riskDistribution,
      trends: trends.map((point: TrendPoint): TrendPoint => {
        return {
          time: point.time ?? '',
          spo2: this.toNumber(point.spo2, 0),
          heartRate: this.toNumber(point.heartRate, 0),
          gas: this.toNumber(point.gas, 0),
          alertCount: this.toNumber(point.alertCount ?? point.alerts, 0),
          alerts: point.alerts
        };
      }),
      topVictims: raw.topVictims ?? raw.priorityQueue ?? [],
      priorityQueue: raw.priorityQueue,
      latestAlerts: alerts.map((item: AlertEvent): AlertEvent => {
        return {
          id: item.id ?? '',
          type: item.type ?? '',
          level: item.level ?? item.severity ?? RiskLevel.GRAY,
          severity: item.severity,
          title: item.title ?? '未命名告警',
          detail: item.detail ?? '',
          targetId: item.targetId ?? item.personId ?? '--',
          personId: item.personId,
          deviceId: item.deviceId ?? '--',
          status: item.status ?? '',
          firstSeenAt: item.firstSeenAt ?? '',
          lastSeenAt: item.lastSeenAt ?? ''
        };
      }),
      activeRescuers: raw.activeRescuers ?? []
    };
  }

  toNumber(value: number | undefined | null, fallback: number): number {
    return value === undefined || value === null || Number.isNaN(value) ? fallback : value;
  }

  metrics(): MetricCard[] {
    return this.overview?.metrics ?? [];
  }

  trends(): TrendPoint[] {
    return this.overview?.trends ?? [];
  }

  topVictims(): Victim[] {
    return this.overview?.topVictims ?? [];
  }

  latestAlerts(): AlertEvent[] {
    return this.overview?.latestAlerts ?? [];
  }

  distribution(): Record<string, number> {
    return this.overview?.distribution ?? {};
  }

  build() {
    Column() {
      this.TopBar();
      if (this.offline) {
        this.OfflineBanner();
      }

      if (this.loading) {
        this.LoadingView();
      } else if (this.overview !== null) {
        Scroll() {
          Column() {
            this.HeroPanel();
            this.MetricsGrid();
            this.DistributionChart();
            this.TrendSection();
            this.TopVictimsList();
            this.LatestAlertsList();
          }
          .padding({ left: 14, right: 14, bottom: 28 })
        }
        .layoutWeight(1)
        .scrollBar(BarState.Off)
      } else {
        this.ErrorView();
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#020617')
  }

  @Builder
  TopBar() {
    Row() {
      Text('‹')
        .fontSize(30)
        .fontColor('#E2E8F0')
        .width(40)
        .height(40)
        .textAlign(TextAlign.Center)
        .onClick(() => { router.back(); })

      Column() {
        Text('态势总览')
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .fontColor('#F8FAFC')
        Text('救援现场实时指挥面板')
          .fontSize(11)
          .fontColor('#94A3B8')
          .margin({ top: 2 })
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start)

      Text(this.offline ? '离线' : (this.refreshTime || '刷新'))
        .fontSize(12)
        .fontColor(this.offline ? '#FBBF24' : '#38BDF8')
        .padding({ left: 10, right: 10, top: 7, bottom: 7 })
        .borderRadius(6)
        .backgroundColor('#0F172A')
        .border({ width: 1, color: '#1E3A5F' })
        .onClick(() => { this.loadData(); })
    }
    .width('100%')
    .padding({ top: 42, bottom: 12, left: 10, right: 14 })
    .backgroundColor('#020617')
    .border({ width: { bottom: 1 }, color: '#1E293B' })
  }

  @Builder
  OfflineBanner() {
    Text('后端未连接，当前显示本地演示数据。请到系统设置填写电脑 IP，例如 http://192.168.1.18:8088')
      .fontSize(11)
      .fontColor('#FBBF24')
      .width('100%')
      .padding({ left: 14, right: 14, top: 8, bottom: 8 })
      .backgroundColor('#422006')
  }

  @Builder
  HeroPanel() {
    Column() {
      Row() {
        Column() {
          Text(this.overview?.siteName ?? '星闪救援现场')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F8FAFC')
            .maxLines(1)
          Text(this.overview?.phaseLabel ?? '现场处置中')
            .fontSize(12)
            .fontColor('#94A3B8')
            .margin({ top: 4 })
            .maxLines(1)
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Start)

        Column() {
          Text('LIVE')
            .fontSize(10)
            .fontWeight(FontWeight.Bold)
            .fontColor('#052E16')
            .padding({ left: 8, right: 8, top: 4, bottom: 4 })
            .borderRadius(4)
            .backgroundColor('#22C55E')
          Text(this.formatTime(this.overview?.snapshotTime ?? ''))
            .fontSize(10)
            .fontColor('#94A3B8')
            .margin({ top: 6 })
        }
        .alignItems(HorizontalAlign.End)
      }
      .width('100%')

      Row() {
        this.SummaryCell('高危人员', this.countRisk(RiskLevel.RED).toString(), '#EF4444')
        this.SummaryCell('活跃告警', this.latestAlerts().filter((item: AlertEvent) => item.status === 'ACTIVE').length.toString(), '#F59E0B')
        this.SummaryCell('救援队伍', this.overview?.activeRescuers.length.toString() ?? '0', '#38BDF8')
      }
      .width('100%')
      .margin({ top: 16 })
      .justifyContent(FlexAlign.SpaceBetween)
    }
    .width('100%')
    .padding(16)
    .margin({ top: 14 })
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
  }

  @Builder
  SummaryCell(label: string, value: string, color: string) {
    Column() {
      Text(value)
        .fontSize(22)
        .fontWeight(FontWeight.Bold)
        .fontColor(color)
        .fontFamily('monospace')
      Text(label)
        .fontSize(10)
        .fontColor('#94A3B8')
        .margin({ top: 2 })
    }
    .width('31%')
    .height(58)
    .justifyContent(FlexAlign.Center)
    .borderRadius(6)
    .backgroundColor('#111827')
  }

  @Builder
  SectionTitle(title: string, subtitle: string) {
    Row() {
      Text(title)
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor('#E2E8F0')
        .layoutWeight(1)
      Text(subtitle)
        .fontSize(10)
        .fontColor('#64748B')
    }
    .width('100%')
    .margin({ top: 18, bottom: 8 })
  }

  @Builder
  MetricsGrid() {
    this.SectionTitle('关键指标', '实时采样')

    Grid() {
      ForEach(this.metrics(), (metric: MetricCard) => {
        GridItem() { this.MetricCardItem(metric); }
      });
    }
    .columnsTemplate('1fr 1fr')
    .columnsGap(10)
    .rowsGap(10)
    .width('100%')
    .height(190)
  }

  @Builder
  MetricCardItem(metric: MetricCard) {
    Column() {
      Row() {
        Text(metric.label)
          .fontSize(11)
          .fontColor('#94A3B8')
          .layoutWeight(1)
          .maxLines(1)
        Column()
          .width(8)
          .height(8)
          .borderRadius(4)
          .backgroundColor(this.getRiskColor(metric.level))
      }
      .width('100%')

      Row() {
        Text(metric.value)
          .fontSize(30)
          .fontWeight(FontWeight.Bold)
          .fontColor('#F8FAFC')
          .fontFamily('monospace')
        Text(metric.unit)
          .fontSize(12)
          .fontColor('#64748B')
          .margin({ left: 4, top: 12 })
      }
      .width('100%')
      .margin({ top: 8 })
      .alignItems(VerticalAlign.Bottom)

      Text(metric.hint)
        .fontSize(10)
        .fontColor(this.getRiskColor(metric.level))
        .margin({ top: 6 })
        .maxLines(1)
    }
    .width('100%')
    .height('100%')
    .padding(12)
    .alignItems(HorizontalAlign.Start)
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
  }

  @Builder
  DistributionChart() {
    this.SectionTitle('风险分布', '按等级')

    Row() {
      this.DistBar('红', '#EF4444', this.distribution()['RED'] ?? 0)
      this.DistBar('橙', '#F59E0B', this.distribution()['ORANGE'] ?? 0)
      this.DistBar('黄', '#EAB308', this.distribution()['YELLOW'] ?? 0)
      this.DistBar('绿', '#22C55E', this.distribution()['GREEN'] ?? 0)
      this.DistBar('灰', '#94A3B8', this.distribution()['GRAY'] ?? 0)
    }
    .width('100%')
    .padding(12)
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
    .justifyContent(FlexAlign.SpaceBetween)
  }

  @Builder
  DistBar(label: string, color: string, count: number) {
    Column() {
      Text(count.toString())
        .fontSize(22)
        .fontWeight(FontWeight.Bold)
        .fontColor(color)
        .fontFamily('monospace')
      Text(label)
        .fontSize(10)
        .fontColor('#94A3B8')
        .margin({ top: 4 })
    }
    .width('18%')
    .height(58)
    .justifyContent(FlexAlign.Center)
    .borderRadius(6)
    .backgroundColor('#111827')
  }

  @Builder
  TrendSection() {
    this.SectionTitle('趋势监控', '最近 12 点')

    Column() {
      this.TrendRow('最低血氧', this.minTrend('spo2').toString(), '%', '#EF4444', this.percent(100 - this.minTrend('spo2'), 35))
      this.TrendRow('最高心率', this.maxTrend('heartRate').toString(), 'bpm', '#F59E0B', this.percent(this.maxTrend('heartRate'), 170))
      this.TrendRow('燃气峰值', this.maxTrend('gas').toFixed(1), '%LEL', '#EC4899', this.percent(this.maxTrend('gas'), 30))
    }
    .width('100%')
    .padding(14)
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
  }

  @Builder
  TrendRow(label: string, value: string, unit: string, color: string, widthValue: string) {
    Column() {
      Row() {
        Text(label)
          .fontSize(11)
          .fontColor('#94A3B8')
          .layoutWeight(1)
        Text(`${value} ${unit}`)
          .fontSize(12)
          .fontWeight(FontWeight.Bold)
          .fontColor(color)
          .fontFamily('monospace')
      }
      .width('100%')

      Stack({ alignContent: Alignment.Start }) {
        Row().width('100%').height(6).borderRadius(3).backgroundColor('#1E293B')
        Row().width(widthValue).height(6).borderRadius(3).backgroundColor(color)
      }
      .width('100%')
      .height(6)
      .margin({ top: 7 })
    }
    .width('100%')
    .margin({ bottom: 12 })
  }

  @Builder
  TopVictimsList() {
    this.SectionTitle('高危受灾人员', `${this.topVictims().length} 人`)

    ForEach(this.topVictims().slice(0, 5), (victim: Victim) => {
      Row() {
        Column()
          .width(4)
          .height(54)
          .borderRadius(2)
          .backgroundColor(this.getRiskColor(victim.level))

        Column() {
          Row() {
            Text(`${victim.name} · ${victim.age}岁`)
              .fontSize(14)
              .fontWeight(FontWeight.Bold)
              .fontColor('#F8FAFC')
              .layoutWeight(1)
              .maxLines(1)
            Text(victim.status)
              .fontSize(10)
              .fontColor(this.getRiskColor(victim.level))
              .padding({ left: 7, right: 7, top: 3, bottom: 3 })
              .borderRadius(4)
              .backgroundColor(this.getRiskColor(victim.level) + '22')
          }
          .width('100%')

          Text(`SpO2 ${victim.vitals?.spo2 ?? 0}% · HR ${victim.vitals?.heartRate ?? 0} · 评分 ${victim.priorityScore}`)
            .fontSize(11)
            .fontColor('#94A3B8')
            .margin({ top: 5 })
            .maxLines(1)
        }
        .layoutWeight(1)
        .margin({ left: 10 })
        .alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      .padding(12)
      .margin({ bottom: 8 })
      .borderRadius(8)
      .backgroundColor('#0F172A')
      .border({ width: 1, color: '#1E293B' })
      .onClick(() => {
        router.pushUrl({ url: 'pages/VictimDetailPage', params: { victimId: victim.id } });
      })
    });
  }

  @Builder
  LatestAlertsList() {
    this.SectionTitle('最新告警', '按时间')

    ForEach(this.latestAlerts().slice(0, 5), (alert: AlertEvent) => {
      Row() {
        Column()
          .width(4)
          .height(58)
          .borderRadius(2)
          .backgroundColor(this.getRiskColor(alert.level))

        Column() {
          Row() {
            Text(alert.title)
              .fontSize(13)
              .fontWeight(FontWeight.Bold)
              .fontColor('#F8FAFC')
              .maxLines(1)
              .layoutWeight(1)
            Text(alert.status === 'ACTIVE' ? '活跃' : '已确认')
              .fontSize(10)
              .fontColor(alert.status === 'ACTIVE' ? '#EF4444' : '#22C55E')
          }
          .width('100%')

          Text(alert.detail)
            .fontSize(11)
            .fontColor('#94A3B8')
            .maxLines(2)
            .margin({ top: 5 })
        }
        .layoutWeight(1)
        .margin({ left: 10 })
        .alignItems(HorizontalAlign.Start)
      }
      .width('100%')
      .padding(12)
      .margin({ bottom: 8 })
      .borderRadius(8)
      .backgroundColor('#0F172A')
      .border({ width: 1, color: '#1E293B' })
    });
  }

  @Builder
  LoadingView() {
    Column() {
      LoadingProgress()
        .width(42)
        .height(42)
        .color('#38BDF8')
      Text('正在同步现场态势')
        .fontSize(14)
        .fontColor('#94A3B8')
        .margin({ top: 14 })
    }
    .width('100%')
    .layoutWeight(1)
    .justifyContent(FlexAlign.Center)
  }

  @Builder
  ErrorView() {
    Column() {
      Text('数据连接异常')
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
        .fontColor('#F8FAFC')
      Text(this.errorMsg)
        .fontSize(13)
        .fontColor('#94A3B8')
        .margin({ top: 8 })
        .textAlign(TextAlign.Center)
      Button('重试')
        .height(40)
        .margin({ top: 18 })
        .backgroundColor('#38BDF8')
        .fontColor('#020617')
        .onClick(() => { this.loadData(); })
    }
    .width('100%')
    .layoutWeight(1)
    .padding({ left: 24, right: 24 })
    .justifyContent(FlexAlign.Center)
  }

  countRisk(level: RiskLevel): number {
    return this.distribution()[level] ?? 0;
  }

  maxTrend(field: string): number {
    let result = 0;
    for (const point of this.trends()) {
      if (field === 'heartRate') {
        result = Math.max(result, point.heartRate);
      } else if (field === 'gas') {
        result = Math.max(result, point.gas);
      } else {
        result = Math.max(result, point.spo2);
      }
    }
    return result;
  }

  minTrend(field: string): number {
    const points = this.trends();
    if (points.length === 0) {
      return 0;
    }
    let result = field === 'spo2' ? points[0].spo2 : 0;
    for (const point of points) {
      if (field === 'spo2') {
        result = Math.min(result, point.spo2);
      }
    }
    return result;
  }

  percent(value: number, max: number): string {
    if (max <= 0) {
      return '0%';
    }
    const result = Math.max(6, Math.min(100, Math.round(value / max * 100)));
    return `${result}%`;
  }

  formatTime(value: string): string {
    if (!value) {
      return '--:--:--';
    }
    if (value.length >= 19 && value.indexOf('T') >= 0) {
      return value.substring(11, 19);
    }
    return value;
  }

  getRiskColor(level: RiskLevel): string {
    switch (level) {
      case RiskLevel.RED: return '#EF4444';
      case RiskLevel.ORANGE: return '#F59E0B';
      case RiskLevel.YELLOW: return '#EAB308';
      case RiskLevel.GREEN: return '#22C55E';
      case RiskLevel.GRAY: return '#94A3B8';
      default: return '#94A3B8';
    }
  }
}
