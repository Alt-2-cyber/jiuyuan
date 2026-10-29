import { router } from '@kit.ArkUI';
import { RescueRepository } from '../common/data/RescueRepository';
import { Device, DeviceStatus } from '../common/models/DataModels';

/**
 * 设备管理
 */
@Entry
@Component
struct DevicesPage {
  @State devices: Device[] = [];
  @State loading: boolean = true;
  @State errorMsg: string = '';
  @State offline: boolean = false;

  aboutToAppear(): void { this.loadData(); }

  async loadData(): Promise<void> {
    this.loading = true;
    this.errorMsg = '';
    try {
      this.devices = await RescueRepository.devices();
      this.offline = RescueRepository.offline;
    } catch (err) {
      console.error('加载设备失败:', JSON.stringify(err));
      this.devices = [];
      this.errorMsg = '设备数据加载失败，请检查后端服务和网络连接';
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
      } else if (this.devices.length === 0) {
        this.StateView('暂无设备', '后端当前没有返回设备数据');
      } else {
        List() {
          ForEach(this.devices, (device: Device) => {
            ListItem() { this.DeviceCard(device); }
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
      Text('设备管理').fontSize(18).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').layoutWeight(1)
      Text('刷新').fontSize(13).fontColor('#4FC3F7').onClick(() => { this.loadData(); })
    }
    .width('100%').padding({ top: 44, bottom: 14, left: 14, right: 14 }).backgroundColor('#0D2137')
  }

  @Builder
  DeviceCard(device: Device) {
    Row() {
      Text(this.getTypeIcon(device.type)).fontSize(24).width(46).height(46).textAlign(TextAlign.Center).borderRadius(23).backgroundColor('#132D45')

      Column() {
        Row() {
          Text(device.name).fontSize(14).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
          Blank()
          Text(this.getStatusText(device.status))
            .fontSize(10).fontColor(this.getStatusColor(device.status))
            .padding({ left: 6, right: 6, top: 2, bottom: 2 })
            .borderRadius(4).backgroundColor(this.getStatusColor(device.status) + '22')
        }
        .width('100%')

        Row() {
          Text(`${device.type} | ${device.id}`).fontSize(11).fontColor('#667788')
          Blank()
          Text(`B:${device.battery}%`).fontSize(11).fontColor(device.battery > 50 ? '#66BB6A' : '#FFB74D')
          Text(` S:${device.signalQuality}`).fontSize(11).fontColor('#4FC3F7').margin({ left: 8 })
        }
        .width('100%').margin({ top: 4 })
      }
      .layoutWeight(1).alignItems(HorizontalAlign.Start).margin({ left: 10 })
    }
    .width('100%').padding(12)
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

  getTypeIcon(type: string): string {
    switch (type) {
      case 'VICTIM_TAG': return '\u231A';
      case 'RESCUER_TERMINAL': return '\u{1F4DF}';
      case 'GATEWAY': return '\u{1F4E1}';
      case 'ANCHOR': return '\u{1F4CD}';
      default: return '\u{1F4F1}';
    }
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
