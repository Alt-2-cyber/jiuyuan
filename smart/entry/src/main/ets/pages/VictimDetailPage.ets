import { router } from '@kit.ArkUI';
import { promptAction } from '@kit.ArkUI';
import { HttpUtils } from '../common/utils/HttpUtils';
import { RescueRepository } from '../common/data/RescueRepository';
import { Victim, RiskLevel, InjuryAnalysisRequest, AiResponse } from '../common/models/DataModels';
import { ApiConstants } from '../common/constants/ApiConstants';

/**
 * 受灾人员详情 + AI伤病分析
 */
@Entry
@Component
struct VictimDetailPage {
  @State victim: Victim | null = null;
  @State loading: boolean = true;
  @State aiResult: string = '';
  @State aiLoading: boolean = false;
  private victimId: string = '';

  aboutToAppear(): void {
    const params = router.getParams() as Record<string, Object>;
    if (params && params['victimId']) {
      this.victimId = params['victimId'] as string;
    }
    this.loadData();
  }

  async loadData(): Promise<void> {
    this.loading = true;
    try {
      const found = await RescueRepository.victim(this.victimId);
      if (found) {
        this.victim = found;
      }
    } catch (err) {
      console.error('加载受灾详情失败:', JSON.stringify(err));
    } finally {
      this.loading = false;
    }
  }

  async analyzeInjury(): Promise<void> {
    if (!this.victim) return;
    this.aiLoading = true;
    this.aiResult = '';
    try {
      const body: InjuryAnalysisRequest = {
        name: this.victim.name,
        age: this.victim.age,
        gender: this.victim.gender,
        spo2: this.victim.vitals.spo2,
        heartRate: this.victim.vitals.heartRate,
        systolic: this.victim.vitals.systolic,
        diastolic: this.victim.vitals.diastolic,
        temperature: this.victim.vitals.temperature,
        motionState: this.victim.vitals.motionState,
        level: this.victim.level,
        priorityScore: this.victim.priorityScore,
        reasons: this.victim.reasons
      };
      const response: AiResponse = await HttpUtils.post<AiResponse>(ApiConstants.AI_INJURY_ANALYSIS, body);
      this.aiResult = response.result;
    } catch (err) {
      promptAction.showToast({ message: 'AI分析失败', duration: 2000 });
    } finally {
      this.aiLoading = false;
    }
  }

  build() {
    Column() {
      this.TopBar();
      if (this.loading) {
        this.LoadingView();
      } else if (this.victim) {
        Scroll() {
          Column() {
            this.BasicInfo();
            this.VitalSignsCard();
            this.LocationCard();
            this.ReasonsCard();
            this.AiSection();
          }
          .padding(12)
        }
        .layoutWeight(1)
      } else {
        Column() {
          Text('未找到该受灾人员')
            .fontSize(16).fontColor('#F8FAFC')
          Text('请返回列表后重试，或检查后端连接')
            .fontSize(12).fontColor('#94A3B8').margin({ top: 8 })
        }
        .width('100%').layoutWeight(1).justifyContent(FlexAlign.Center)
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
      Text('受灾详情')
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FFFFFF')
        .layoutWeight(1)
    }
    .width('100%')
    .padding({ top: 44, bottom: 14, left: 14, right: 14 })
    .backgroundColor('#0D2137')
  }

  @Builder
  BasicInfo() {
    Row() {
      Column()
        .width(55)
        .height(55)
        .borderRadius(28)
        .backgroundColor(this.getRiskColor(this.victim!.level) + '22')
        .justifyContent(FlexAlign.Center) {
          Text(this.victim!.name.substring(0, 2))
            .fontSize(16)
            .fontColor(this.getRiskColor(this.victim!.level))
        }

      Column() {
        Row() {
          Text(this.victim!.name)
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#FFFFFF')
          Text(`  ${this.victim!.age}岁/${this.victim!.gender}`)
            .fontSize(14)
            .fontColor('#8899AA')
        }
        Text(`设备: ${this.victim!.deviceId} | 评分: ${this.victim!.priorityScore}`)
          .fontSize(11)
          .fontColor('#667788')
          .margin({ top: 4 })
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start)
      .margin({ left: 12 })
    }
    .width('100%')
    .padding(14)
    .borderRadius(10)
    .backgroundColor('#132D45')
  }

  @Builder
  VitalSignsCard() {
    Text('生命体征')
      .fontSize(14)
      .fontWeight(FontWeight.Bold)
      .fontColor('#8899AA')
      .margin({ top: 12, bottom: 8 })
      .width('100%')

    Grid() {
      GridItem() { this.VitalItem('血氧 SpO2', `${this.victim!.vitals.spo2}%`, this.victim!.vitals.spo2 < 90 ? '#FF5252' : '#66BB6A') }
      GridItem() { this.VitalItem('心率', `${this.victim!.vitals.heartRate} bpm`, this.victim!.vitals.heartRate > 120 ? '#FF5252' : '#FFB74D') }
      GridItem() { this.VitalItem('血压', `${this.victim!.vitals.systolic}/${this.victim!.vitals.diastolic}`, '#4FC3F7') }
      GridItem() { this.VitalItem('体温', `${this.victim!.vitals.temperature}\u2103`, this.victim!.vitals.temperature > 38 ? '#FF5252' : '#66BB6A') }
      GridItem() { this.VitalItem('活动状态', this.victim!.vitals.motionState, '#8899AA') }
      GridItem() { this.VitalItem('信号质量', `${(this.victim!.vitals.signalQuality * 100).toFixed(0)}%`, '#66BB6A') }
    }
    .columnsTemplate('1fr 1fr 1fr')
    .columnsGap(6)
    .rowsGap(6)
    .width('100%')
    .height(140)
  }

  @Builder
  VitalItem(label: string, value: string, color: string) {
    Column() {
      Text(label)
        .fontSize(10)
        .fontColor('#667788')
      Text(value)
        .fontSize(15)
        .fontWeight(FontWeight.Bold)
        .fontColor(color)
        .margin({ top: 4 })
    }
    .width('100%')
    .height('100%')
    .justifyContent(FlexAlign.Center)
    .borderRadius(8)
    .backgroundColor('#132D45')
  }

  @Builder
  LocationCard() {
    Text('位置信息')
      .fontSize(14)
      .fontWeight(FontWeight.Bold)
      .fontColor('#8899AA')
      .margin({ top: 12, bottom: 8 })
      .width('100%')

    Column() {
      this.LocItem('坐标', `x:${this.victim!.location.x}  y:${this.victim!.location.y}  z:${this.victim!.location.z}`)
      this.LocItem('距最近救援员', `${this.victim!.location.distanceToNearestRescuer}m`)
      this.LocItem('定位精度', `${this.victim!.location.accuracy}m`)
      this.LocItem('方位角', `${this.victim!.location.azimuth}\u00B0`)
      this.LocItem('最后上报', this.victim!.lastSeenAt)
    }
    .width('100%')
    .padding(12)
    .borderRadius(10)
    .backgroundColor('#132D45')
  }

  @Builder
  LocItem(label: string, value: string) {
    Row() {
      Text(label)
        .fontSize(12)
        .fontColor('#8899AA')
        .width(100)
      Text(value)
        .fontSize(12)
        .fontColor('#FFFFFF')
    }
    .width('100%')
    .padding({ top: 4, bottom: 4 })
  }

  @Builder
  ReasonsCard() {
    Text('风险因素')
      .fontSize(14)
      .fontWeight(FontWeight.Bold)
      .fontColor('#8899AA')
      .margin({ top: 12, bottom: 8 })
      .width('100%')

    Column() {
      ForEach(this.victim!.reasons, (reason: string) => {
        Text('\u2022 ' + reason)
          .fontSize(12)
          .fontColor('#FFB74D')
          .padding({ top: 3, bottom: 3 })
          .width('100%')
      });
    }
    .width('100%')
    .padding(12)
    .borderRadius(10)
    .backgroundColor('#132D45')
  }

  @Builder
  AiSection() {
    Text('AI 伤病分析')
      .fontSize(14)
      .fontWeight(FontWeight.Bold)
      .fontColor('#8899AA')
      .margin({ top: 12, bottom: 8 })
      .width('100%')

    Column() {
      if (!this.aiResult && !this.aiLoading) {
        Button('启动 AI 伤病分析')
          .fontSize(14)
          .backgroundColor('#00838F')
          .fontColor('#FFFFFF')
          .borderRadius(8)
          .padding({ left: 20, right: 20, top: 10, bottom: 10 })
          .margin({ top: 8, bottom: 8 })
          .onClick(() => { this.analyzeInjury(); })
      } else if (this.aiLoading) {
        Row() {
          LoadingProgress()
            .width(20)
            .height(20)
            .color('#00BCD4')
          Text(' AI正在分析伤病情况...')
            .fontSize(13)
            .fontColor('#8899AA')
            .margin({ left: 10 })
        }
        .padding({ top: 12, bottom: 12 })
      } else {
        Text(this.aiResult)
          .fontSize(13)
          .fontColor('#E0E0E0')
          .lineHeight(22)
          .padding(12)
          .width('100%')

        Button('重新分析')
          .fontSize(12)
          .backgroundColor('#00838F')
          .fontColor('#FFFFFF')
          .borderRadius(6)
          .margin({ top: 8, bottom: 8 })
          .onClick(() => { this.analyzeInjury(); })
      }
    }
    .width('100%')
    .padding(12)
    .borderRadius(10)
    .backgroundColor('#132D45')
    .border({ width: 1, color: '#00BCD444' })
    .margin({ bottom: 20 })
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
