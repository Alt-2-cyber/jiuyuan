import { router } from '@kit.ArkUI';
import { promptAction } from '@kit.ArkUI';
import { RescueRepository } from '../common/data/RescueRepository';
import { RescueTask, TaskStatus, RiskLevel } from '../common/models/DataModels';

/**
 * 任务调度
 */
@Entry
@Component
struct TasksPage {
  @State tasks: RescueTask[] = [];
  @State loading: boolean = true;
  @State errorMsg: string = '';
  @State offline: boolean = false;

  aboutToAppear(): void { this.loadData(); }

  async loadData(): Promise<void> {
    this.loading = true;
    this.errorMsg = '';
    try {
      this.tasks = await RescueRepository.tasks();
      this.offline = RescueRepository.offline;
    } catch (err) {
      console.error('加载任务失败:', JSON.stringify(err));
      this.tasks = [];
      this.errorMsg = '任务数据加载失败，请检查后端服务和网络连接';
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
      } else if (this.tasks.length === 0) {
        this.StateView('暂无任务', '后端当前没有返回任务数据');
      } else {
        List() {
          ForEach(this.tasks, (task: RescueTask) => {
            ListItem() { this.TaskCard(task); }
          });
        }
        .layoutWeight(1)
        .divider({ strokeWidth: 1, color: '#1A3345' })
      }
    }
    .width('100%').height('100%').backgroundColor('#0A1628')
  }

  @Builder
  TopBar() {
    Row() {
      Text('\u2190').fontSize(22).fontColor('#FFFFFF').padding({ right: 12 }).onClick(() => { router.back(); })
      Text('任务调度').fontSize(18).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').layoutWeight(1)
      Text('刷新').fontSize(13).fontColor('#4FC3F7').onClick(() => { this.loadData(); })
    }
    .width('100%').padding({ top: 44, bottom: 14, left: 14, right: 14 }).backgroundColor('#0D2137')
  }

  @Builder
  TaskCard(task: RescueTask) {
    Row() {
      Column().width(4).height(80).backgroundColor(this.getPriorityColor(task.priority)).borderRadius(2)

      Column() {
        Row() {
          Text(task.id).fontSize(12).fontColor('#4FC3F7')
          Text(` 受灾: ${task.victimId}`).fontSize(13).fontColor('#FFFFFF')
          Blank()
          Text(this.getStatusText(task.status))
            .fontSize(11).fontColor(this.getStatusColor(task.status))
            .padding({ left: 6, right: 6, top: 2, bottom: 2 })
            .borderRadius(4).backgroundColor(this.getStatusColor(task.status) + '22')
        }
        .width('100%')

        Text(task.reason).fontSize(12).fontColor('#8899AA').maxLines(2).margin({ top: 4 }).width('100%')

        Row() {
          Text(`救援员: ${task.rescuerId || '未指派'}`)
            .fontSize(11).fontColor(task.rescuerId ? '#66BB6A' : '#FF5252')
          Text(` 评分: ${task.priorityScore}`)
            .fontSize(11).fontColor('#FFB74D').margin({ left: 10 })
          Blank()
          Text(task.status === TaskStatus.PENDING ? '指派' : '查看')
            .fontSize(11).fontColor('#4FC3F7')
            .padding({ left: 8, right: 8, top: 4, bottom: 4 })
            .borderRadius(4).backgroundColor('#4FC3F722')
            .onClick(() => {
              promptAction.showToast({ message: `任务 ${task.id} 操作`, duration: 1500 });
            })
        }
        .width('100%').margin({ top: 4 })
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

  getStatusText(status: TaskStatus): string {
    switch (status) {
      case TaskStatus.PENDING: return '待派发';
      case TaskStatus.ASSIGNED: return '已派发';
      case TaskStatus.IN_PROGRESS: return '救援中';
      case TaskStatus.PAUSED: return '已暂停';
      case TaskStatus.COMPLETED: return '已完成';
      case TaskStatus.CANCELLED: return '已取消';
      default: return '未知';
    }
  }

  getStatusColor(status: TaskStatus): string {
    switch (status) {
      case TaskStatus.PENDING: return '#FFB74D';
      case TaskStatus.ASSIGNED: return '#4FC3F7';
      case TaskStatus.IN_PROGRESS: return '#FF5252';
      case TaskStatus.PAUSED: return '#9E9E9E';
      case TaskStatus.COMPLETED: return '#66BB6A';
      case TaskStatus.CANCELLED: return '#9E9E9E';
      default: return '#8899AA';
    }
  }

  getPriorityColor(priority: RiskLevel): string {
    switch (priority) {
      case RiskLevel.RED: return '#FF5252';
      case RiskLevel.ORANGE: return '#FFB74D';
      case RiskLevel.YELLOW: return '#FFD54F';
      case RiskLevel.GREEN: return '#66BB6A';
      case RiskLevel.GRAY: return '#9E9E9E';
      default: return '#8899AA';
    }
  }
}
