import { router } from '@kit.ArkUI';
import { RescueRepository } from '../common/data/RescueRepository';
import { Rescuer, DeviceStatus } from '../common/models/DataModels';

/**
 * 救援人员列表
 */
@Entry
@Component
struct RescuersPage {
  @State rescuers: Rescuer[] = [];
  @State loading: boolean = true;
  @State errorMsg: string = '';
  @State offline: boolean = false;

  aboutToAppear(): void { this.loadData(); }

  async loadData(): Promise<void> {
    this.loading = true;
    this.errorMsg = '';
    try {
      this.rescuers = await RescueRepository.rescuers();
      this.offline = RescueRepository.offline;
    } catch (err) {
      console.error('加载救援人员失败:', JSON.stringify(err));
      this.rescuers = [];
      this.errorMsg = '救援人员数据加载失败，请检查后端服务和网络连接';
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
      } else if (this.rescuers.length === 0) {
        this.StateView('暂无救援人员', '后端当前没有返回救援人员数据');
      } else {
        List() {
          ForEach(this.rescuers, (rescuer: Rescuer) => {
            ListItem() { this.RescuerCard(rescuer); }
          });
        }
        .layoutWeight(1)
        .divider({ strokeWidth: 1, color: '#1A3345' })
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#0A1628')
  }

  @Builder
  TopBar() {
    Row() {
      Text('\u2190').fontSize(22).fontColor('#FFFFFF').padding({ right: 12 }).onClick(() => { router.back(); })
      Text('救援人员').fontSize(18).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').layoutWeight(1)
      Text('刷新').fontSize(13).fontColor('#4FC3F7').onClick(() => { this.loadData(); })
    }
    .width('100%')
    .padding({ top: 44, bottom: 14, left: 14, right: 14 })
    .backgroundColor('#0D2137')
  }

  @Builder
  RescuerCard(rescuer: Rescuer) {
    Row() {
      Column().width(4).height(70).backgroundColor(this.getStatusColor(rescuer.status)).borderRadius(2)

      Column() {
        Row() {
          Text(rescuer.name)
            .fontSize(15).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
          Text(` ${rescuer.team}`)
            .fontSize(11).fontColor('#8899AA')
          Blank()
          Text(this.getStatusText(rescuer.status))
            .fontSize(11).fontColor(this.getStatusColor(rescuer.status))
            .padding({ left: 6, right: 6, top: 2, bottom: 2 })
            .borderRadius(4)
            .backgroundColor(this.getStatusColor(rescuer.status) + '22')
        }
        .width('100%')

        Row() {
          Text(`环境: ${rescuer.environment.temperature}\u2103 / ${rescuer.environment.humidity}% / 燃气${rescuer.environment.combustibleGas}%LEL`)
            .fontSize(11).fontColor('#667788').maxLines(1)
        }
        .width('100%').margin({ top: 4 })

        Row() {
          Text(`任务: ${rescuer.taskId || rescuer.currentTaskId || '空闲'}`)
            .fontSize(11).fontColor('#8899AA')
          Text(` 目标: ${rescuer.victimId || rescuer.targetVictimId || '-'}`)
            .fontSize(11).fontColor('#8899AA').margin({ left: 10 })
          Blank()
          Text(`🔋${rescuer.battery}%`)
            .fontSize(11).fontColor(rescuer.battery > 60 ? '#66BB6A' : '#FFB74D')
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

  getStatusText(status: DeviceStatus): string {
    switch (status) {
      case DeviceStatus.ONLINE: return '在线';
      case DeviceStatus.WARNING: return '告警';
      case DeviceStatus.OFFLINE: return '离线';
      default: return '未知';
    }
  }

  getStatusColor(status: DeviceStatus): string {
    switch (status) {
      case DeviceStatus.ONLINE: return '#66BB6A';
      case DeviceStatus.WARNING: return '#FFB74D';
      case DeviceStatus.OFFLINE: return '#FF5252';
      default: return '#8899AA';
    }
  }
}
