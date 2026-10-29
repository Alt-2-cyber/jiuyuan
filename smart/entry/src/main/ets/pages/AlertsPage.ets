import { router } from '@kit.ArkUI';
import { RescueRepository } from '../common/data/RescueRepository';
import { AlertEvent, RiskLevel } from '../common/models/DataModels';

/**
 * 告警中心
 */
@Entry
@Component
struct AlertsPage {
  @State alerts: AlertEvent[] = [];
  @State loading: boolean = true;
  @State errorMsg: string = '';
  @State offline: boolean = false;

  aboutToAppear(): void { this.loadData(); }

  async loadData(): Promise<void> {
    this.loading = true;
    this.errorMsg = '';
    try {
      this.alerts = await RescueRepository.alerts();
      this.offline = RescueRepository.offline;
    } catch (err) {
      console.error('加载告警失败:', JSON.stringify(err));
      this.alerts = [];
      this.errorMsg = '告警数据加载失败，请检查后端服务和网络连接';
      this.offline = true;
    } finally {
      this.loading = false;
    }
  }

  build() {
    Column() {
      this.TopBar();
      if (this.offline) {
        Text('后端未连接，当前显示本地演示数据')
          .fontSize(11).fontColor('#FBBF24').width('100%')
          .padding({ left: 14, right: 14, top: 8, bottom: 8 }).backgroundColor('#422006')
      }
      if (this.loading) {
        this.LoadingView();
      } else if (this.errorMsg.length > 0) {
        this.StateView('连接异常', this.errorMsg);
      } else if (this.alerts.length === 0) {
        this.StateView('暂无告警', '后端当前没有返回告警数据');
      } else {
        List() {
          ForEach(this.alerts, (alert: AlertEvent) => {
            ListItem() { this.AlertCard(alert); }
          });
        }
        .layoutWeight(1).divider({ strokeWidth: 1, color: '#1A3345' })
      }
    }
    .width('100%').height('100%').backgroundColor('#0A1628')
  }

  @Builder
  TopBar() {
    Row() {
      Text('\u2190').fontSize(22).fontColor('#FFFFFF').padding({ right: 12 }).onClick(() => { router.back(); })
      Text('告警中心').fontSize(18).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').layoutWeight(1)
      Text('刷新').fontSize(13).fontColor('#4FC3F7').onClick(() => { this.loadData(); })
    }
    .width('100%').padding({ top: 44, bottom: 14, left: 14, right: 14 }).backgroundColor('#0D2137')
  }

  @Builder
  AlertCard(alert: AlertEvent) {
    Row() {
      Column().width(4).height(80).backgroundColor(this.getLevelColor(alert.level)).borderRadius(2)

      Column() {
        Row() {
          Text(this.getTypeText(alert.type))
            .fontSize(10).fontColor('#FFFFFF')
            .padding({ left: 5, right: 5, top: 2, bottom: 2 })
            .borderRadius(3).backgroundColor(this.getLevelColor(alert.level) + '33')
          Text(` ${alert.title}`)
            .fontSize(14).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').maxLines(1).layoutWeight(1)
          Text(alert.status === 'ACTIVE' ? '活跃' : '已确认')
            .fontSize(10).fontColor(alert.status === 'ACTIVE' ? '#FF5252' : '#66BB6A')
        }
        .width('100%')

        Text(alert.detail).fontSize(12).fontColor('#8899AA').maxLines(2).margin({ top: 4 }).width('100%')

        Row() {
          Text(`目标: ${alert.targetId}`).fontSize(11).fontColor('#667788')
          Text(` 设备: ${alert.deviceId}`).fontSize(11).fontColor('#667788').margin({ left: 10 })
          Blank()
          Text(this.formatClock(alert.lastSeenAt))
            .fontSize(10).fontColor('#556677')
        }
        .width('100%').margin({ top: 2 })
      }
      .layoutWeight(1).margin({ left: 12 }).alignItems(HorizontalAlign.Start)
    }
    .width('100%').padding({ top: 12, bottom: 12, right: 12 })
  }

  @Builder
  LoadingView() {
    Column() {
      LoadingProgress().width(36).height(36).color('#4FC3F7')
    }
    .width('100%').layoutWeight(1).justifyContent(FlexAlign.Center)
  }

  @Builder
  StateView(title: string, detail: string) {
    Column() {
      Text(title)
        .fontSize(17)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FFFFFF')
      Text(detail)
        .fontSize(13)
        .fontColor('#8899AA')
        .textAlign(TextAlign.Center)
        .margin({ top: 8 })
      Button('重新加载')
        .height(38)
        .fontSize(13)
        .fontColor('#0A1628')
        .backgroundColor('#4FC3F7')
        .borderRadius(6)
        .margin({ top: 18 })
        .onClick(() => { this.loadData(); })
    }
    .width('100%')
    .layoutWeight(1)
    .padding({ left: 24, right: 24 })
    .justifyContent(FlexAlign.Center)
  }

  getTypeText(type: string): string {
    switch (type) {
      case 'VITAL_CRITICAL': return '生命危急';
      case 'SOS': return 'SOS';
      case 'ENV_DANGER': return '环境危险';
      case 'ENV_WARNING': return '环境警告';
      case 'DEVICE_LOW_BATTERY': return '低电量';
      case 'LOCATION_LOW_QUALITY': return '定位偏差';
      default: return type;
    }
  }

  getLevelColor(level: RiskLevel): string {
    switch (level) {
      case RiskLevel.RED: return '#FF5252';
      case RiskLevel.ORANGE: return '#FFB74D';
      case RiskLevel.YELLOW: return '#FFD54F';
      case RiskLevel.GREEN: return '#66BB6A';
      case RiskLevel.GRAY: return '#9E9E9E';
      default: return '#8899AA';
    }
  }

  formatClock(value: string): string {
    if (!value || value.length < 16) {
      return '--:--';
    }
    return value.substring(11, 16);
  }
}
