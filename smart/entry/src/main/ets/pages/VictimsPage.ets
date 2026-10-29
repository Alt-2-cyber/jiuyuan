import { router } from '@kit.ArkUI';
import { RescueRepository } from '../common/data/RescueRepository';
import { Victim, RiskLevel } from '../common/models/DataModels';

/**
 * 受灾人员列表
 */
@Entry
@Component
struct VictimsPage {
  @State victims: Victim[] = [];
  @State loading: boolean = true;
  @State errorMsg: string = '';
  @State offline: boolean = false;

  aboutToAppear(): void {
    this.loadData();
  }

  async loadData(): Promise<void> {
    this.loading = true;
    this.errorMsg = '';
    try {
      this.victims = await RescueRepository.victims();
      this.offline = RescueRepository.offline;
    } catch (err) {
      console.error('加载受灾人员失败:', JSON.stringify(err));
      this.victims = [];
      this.errorMsg = '受灾人员数据加载失败，请检查后端服务和网络连接';
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
      } else if (this.victims.length === 0) {
        this.StateView('暂无受灾人员', '后端当前没有返回人员数据');
      } else {
        List() {
          ForEach(this.victims, (victim: Victim) => {
            ListItem() { this.VictimCard(victim); }
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
      Text('\u2190')
        .fontSize(22)
        .fontColor('#FFFFFF')
        .padding({ right: 12 })
        .onClick(() => { router.back(); })
      Text('受灾人员')
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FFFFFF')
        .layoutWeight(1)
      Text('刷新')
        .fontSize(13)
        .fontColor('#4FC3F7')
        .onClick(() => { this.loadData(); })
    }
    .width('100%')
    .padding({ top: 44, bottom: 14, left: 14, right: 14 })
    .backgroundColor('#0D2137')
  }

  @Builder
  VictimCard(victim: Victim) {
    Row() {
      // 风险色块
      Column()
        .width(4)
        .height(60)
        .backgroundColor(this.getRiskColor(victim.level))
        .borderRadius(2)

      Column() {
        Row() {
          Text(`${victim.name}`)
            .fontSize(15)
            .fontWeight(FontWeight.Bold)
            .fontColor('#FFFFFF')
          Text(` ${victim.age}岁/${victim.gender}`)
            .fontSize(12)
            .fontColor('#8899AA')
          Blank()
          Text(victim.status)
            .fontSize(11)
            .fontColor(this.getRiskColor(victim.level))
            .padding({ left: 8, right: 8, top: 2, bottom: 2 })
            .borderRadius(4)
            .backgroundColor(this.getRiskColor(victim.level) + '22')
        }
        .width('100%')

        Row() {
          Text(`SpO2:${victim.vitals.spo2}%`)
            .fontSize(11)
            .fontColor(victim.vitals.spo2 < 90 ? '#FF5252' : '#8899AA')
          Text(` HR:${victim.vitals.heartRate}`)
            .fontSize(11)
            .fontColor('#8899AA')
            .margin({ left: 10 })
          Text(` BP:${victim.vitals.systolic}/${victim.vitals.diastolic}`)
            .fontSize(11)
            .fontColor('#8899AA')
            .margin({ left: 10 })
          Blank()
          Text(`评分:${victim.priorityScore}`)
            .fontSize(12)
            .fontWeight(FontWeight.Bold)
            .fontColor('#FFB74D')
        }
        .width('100%')
        .margin({ top: 4 })

        Row() {
          Text(`距救援员: ${victim.location.distanceToNearestRescuer}m`)
            .fontSize(10)
            .fontColor('#556677')
          Text(` 精度: ${victim.location.accuracy}m`)
            .fontSize(10)
            .fontColor('#556677')
            .margin({ left: 8 })
          Blank()
          Text(`🔋${victim.vitals.battery}%`)
            .fontSize(10)
            .fontColor('#667788')
        }
        .width('100%')
        .margin({ top: 2 })
      }
      .layoutWeight(1)
      .margin({ left: 12 })
      .alignItems(HorizontalAlign.Start)
    }
    .width('100%')
    .padding({ top: 12, bottom: 12, right: 12 })
    .onClick(() => {
      router.pushUrl({ url: 'pages/VictimDetailPage', params: { victimId: victim.id } });
    })
  }

  @Builder
  LoadingView() {
    Column() {
      LoadingProgress()
        .width(36)
        .height(36)
        .color('#4FC3F7')
    }
    .width('100%')
    .layoutWeight(1)
    .justifyContent(FlexAlign.Center)
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

  getRiskColor(level: RiskLevel): string {
    switch (level) {
      case RiskLevel.RED: return '#FF5252';
      case RiskLevel.ORANGE: return '#FFB74D';
      case RiskLevel.YELLOW: return '#FFD54F';
      case RiskLevel.GREEN: return '#66BB6A';
      case RiskLevel.GRAY: return '#9E9E9E';
      default: return '#8899AA';
    }
  }
}
