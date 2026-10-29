import { router } from '@kit.ArkUI';

/**
 * 星闪救援 - 指挥中心首页
 */
@Entry
@Component
struct Index {
  @State currentTime: string = '';

  aboutToAppear(): void {
    this.updateTime();
    setInterval(() => { this.updateTime(); }, 60000);
  }

  updateTime(): void {
    const now = new Date();
    this.currentTime = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`;
  }

  build() {
    Column() {
      this.HeaderSection();
      this.AlertStrip();
      this.StatsRow();
      this.MenuGrid();
      Blank()
      this.BottomInfo();
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#020617')
  }

  @Builder
  HeaderSection() {
    Column() {
      Row() {
        Column() {
          Text('星闪救援指挥中心')
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F8FAFC')
          Text('NearLink 灾后协同救援系统')
            .fontSize(12)
            .fontColor('#94A3B8')
            .margin({ top: 4 })
        }
        .layoutWeight(1)
        .alignItems(HorizontalAlign.Start)

        Column() {
          Text(this.currentTime)
            .fontSize(28)
            .fontWeight(FontWeight.Bold)
            .fontColor('#38BDF8')
            .fontFamily('monospace')
          Text('在线')
            .fontSize(10)
            .fontColor('#22C55E')
            .margin({ top: 2 })
        }
        .alignItems(HorizontalAlign.End)
      }
      .width('100%')

      Row() {
        this.StatusPill('指挥链路', '稳定', '#22C55E')
        this.StatusPill('数据同步', '实时', '#38BDF8')
        this.StatusPill('告警等级', '高', '#EF4444')
      }
      .width('100%')
      .margin({ top: 16 })
      .justifyContent(FlexAlign.SpaceBetween)
    }
    .width('100%')
    .padding({ top: 46, bottom: 16, left: 16, right: 16 })
    .backgroundColor('#020617')
    .border({ width: { bottom: 1 }, color: '#1E293B' })
  }

  @Builder
  StatusPill(label: string, value: string, color: string) {
    Row() {
      Column()
        .width(7)
        .height(7)
        .borderRadius(4)
        .backgroundColor(color)
      Text(`${label} ${value}`)
        .fontSize(10)
        .fontColor('#CBD5E1')
        .margin({ left: 6 })
    }
    .height(30)
    .padding({ left: 10, right: 10 })
    .borderRadius(6)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
  }

  @Builder
  AlertStrip() {
    Row() {
      Text('RED')
        .fontSize(10)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FEE2E2')
        .padding({ left: 8, right: 8, top: 4, bottom: 4 })
        .borderRadius(4)
        .backgroundColor('#7F1D1D')
      Text('2 名高危人员等待持续跟进，6 条告警保持监控')
        .fontSize(12)
        .fontColor('#E2E8F0')
        .margin({ left: 10 })
        .layoutWeight(1)
        .maxLines(1)
    }
    .width('100%')
    .height(44)
    .padding({ left: 14, right: 14 })
    .backgroundColor('#111827')
    .border({ width: { bottom: 1 }, color: '#1E293B' })
  }

  @Builder
  StatsRow() {
    Row() {
      this.StatCard('高危', '2', '#EF4444')
      this.StatCard('警告', '2', '#F59E0B')
      this.StatCard('安全', '4', '#22C55E')
      this.StatCard('设备', '16', '#38BDF8')
    }
    .width('100%')
    .padding({ left: 14, right: 14, top: 14, bottom: 10 })
    .justifyContent(FlexAlign.SpaceBetween)
  }

  @Builder
  StatCard(label: string, value: string, accentColor: string) {
    Column() {
      Text(value)
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
        .fontColor(accentColor)
        .fontFamily('monospace')
      Text(label)
        .fontSize(10)
        .fontColor('#94A3B8')
        .margin({ top: 2 })
    }
    .width('23%')
    .height(68)
    .justifyContent(FlexAlign.Center)
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
  }

  @Builder
  MenuGrid() {
    Grid() {
      GridItem() { this.MenuCard('态势总览', '全局态势实时监控', '01', '#38BDF8', 'pages/DashboardPage') }
      GridItem() { this.MenuCard('受灾人员', '8 人受灾待救援', '02', '#EF4444', 'pages/VictimsPage') }
      GridItem() { this.MenuCard('救援人员', '4 人在岗救援', '03', '#22C55E', 'pages/RescuersPage') }
      GridItem() { this.MenuCard('任务调度', '6 个任务进行中', '04', '#F59E0B', 'pages/TasksPage') }
      GridItem() { this.MenuCard('告警中心', '6 条活跃告警', '05', '#EC4899', 'pages/AlertsPage') }
      GridItem() { this.MenuCard('设备管理', '16 台在线设备', '06', '#A78BFA', 'pages/DevicesPage') }
      GridItem() { this.MenuCard('AI 助手', '智能分析决策', 'AI', '#2DD4BF', 'pages/AIChatPage') }
      GridItem() { this.MenuCard('系统设置', '配置与连接管理', 'SET', '#94A3B8', 'pages/SettingsPage') }
    }
    .columnsTemplate('1fr 1fr')
    .columnsGap(10)
    .rowsGap(10)
    .width('100%')
    .layoutWeight(1)
    .padding({ left: 14, right: 14, top: 4, bottom: 8 })
  }

  @Builder
  MenuCard(title: string, desc: string, mark: string, accentColor: string, targetPage: string) {
    Row() {
      Column() {
        Text(mark)
          .fontSize(mark.length > 2 ? 10 : 13)
          .fontWeight(FontWeight.Bold)
          .fontColor(accentColor)
          .fontFamily('monospace')
      }
      .width(38)
      .height(38)
      .borderRadius(7)
      .backgroundColor('#111827')
      .border({ width: 1, color: accentColor + '66' })
      .justifyContent(FlexAlign.Center)

      Column() {
        Text(title)
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .fontColor('#F8FAFC')
          .maxLines(1)
        Text(desc)
          .fontSize(10)
          .fontColor('#94A3B8')
          .margin({ top: 3 })
          .maxLines(1)
      }
      .alignItems(HorizontalAlign.Start)
      .margin({ left: 10 })
      .layoutWeight(1)
    }
    .width('100%')
    .height(78)
    .padding({ left: 10, right: 10 })
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
    .onClick(() => {
      router.pushUrl({ url: targetPage });
    })
  }

  @Builder
  BottomInfo() {
    Row() {
      Text('星闪 NearLink · 救援通信专网 · v1.0.0')
        .fontSize(10)
        .fontColor('#64748B')
    }
    .width('100%')
    .height(34)
    .justifyContent(FlexAlign.Center)
  }
}
