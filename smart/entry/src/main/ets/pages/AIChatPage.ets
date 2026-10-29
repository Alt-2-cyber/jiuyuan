import { router } from '@kit.ArkUI';
import { ChatMessage, ChatHistoryMessage,
  InjuryAnalysisRequest, RouteOptimizeRequest, VitalsTrendRequest,
  EnvironmentAssessmentRequest, ReportRequest } from '../common/models/DataModels';
import { AiStreamService } from '../common/services/AiStreamService';
import { AppDatabase } from '../common/database/AppDatabase';

/**
 * AI 智能助手 - 对话与快捷分析
 */
@Entry
@Component
struct AIChatPage {
  @State messages: ChatMessage[] = [];
  @State inputText: string = '';
  @State loading: boolean = false;
  @State showQuickActions: boolean = true;
  private streamBuffer: string = '';
  private streamFlushTimer: number = -1;
  private scroller: Scroller = new Scroller();
  private readonly welcomeMessage: ChatMessage = {
    role: 'system',
    content: '星闪救援 AI 已接入后端智能分析服务。可直接询问现场处置、风险判断、路径调度，也可以选择下方快捷指令。',
    time: this.now()
  };

  aboutToAppear(): void {
    this.loadMessages();
  }

  now(): string {
    const d = new Date();
    return `${d.getHours().toString().padStart(2, '0')}:${d.getMinutes().toString().padStart(2, '0')}`;
  }

  async sendMessage(userText: string): Promise<void> {
    const text = userText.trim();
    if (!text || this.loading) return;
    this.showQuickActions = false;

    const history = this.buildHistory();
    const userMessage: ChatMessage = { role: 'user', content: text, time: this.now() };
    this.messages = [...this.messages, userMessage];
    AppDatabase.addChatMessage(userMessage).catch((err: Error) => {
      console.error(`保存用户消息失败: ${JSON.stringify(err)}`);
    });

    const aiMessage: ChatMessage = { role: 'ai', content: '', time: this.now(), type: 'zhipu-stream' };
    this.messages = [...this.messages, aiMessage];
    const aiIndex = this.messages.length - 1;
    this.inputText = '';
    this.loading = true;
    this.streamBuffer = '';
    this.scrollToBottom();

    try {
      await AiStreamService.chatStream(text, history, (chunk: string) => {
        this.streamBuffer += chunk;
        this.scheduleStreamFlush(aiIndex);
      });
      this.flushStreamBuffer(aiIndex);
      if (!this.messages[aiIndex].content.trim()) {
        this.updateMessageContent(aiIndex, 'AI服务没有返回内容，请稍后重试。', false);
      }
      AppDatabase.addChatMessage(this.messages[aiIndex]).catch((err: Error) => {
        console.error(`保存AI消息失败: ${JSON.stringify(err)}`);
      });
    } catch (err) {
      const message = err instanceof Error && err.message ? err.message : 'AI服务暂时不可用，请检查后端服务和网络连接。';
      this.updateMessageContent(aiIndex, message, false);
    } finally {
      this.flushStreamBuffer(aiIndex);
      this.loading = false;
      this.scrollToBottom();
    }
  }

  scheduleStreamFlush(index: number): void {
    if (this.streamFlushTimer >= 0) {
      return;
    }
    this.streamFlushTimer = setTimeout(() => {
      this.streamFlushTimer = -1;
      this.flushStreamBuffer(index);
    }, 90);
  }

  flushStreamBuffer(index: number): void {
    if (!this.streamBuffer) {
      return;
    }
    const chunk = this.streamBuffer;
    this.streamBuffer = '';
    this.updateMessageContent(index, chunk, true);
  }

  updateMessageContent(index: number, content: string, append: boolean): void {
    if (index < 0 || index >= this.messages.length) {
      return;
    }
    const updated: ChatMessage = {
      role: this.messages[index].role,
      content: append ? `${this.messages[index].content}${content}` : content,
      time: this.messages[index].time,
      type: this.messages[index].type
    };
    const next = this.messages.slice();
    next[index] = updated;
    this.messages = next;
    this.scrollToBottom();
  }

  async quickAction(action: string, prompt: string): Promise<void> {
    this.showQuickActions = false;
    const actionPrompt = this.buildQuickActionPrompt(action, prompt);
    await this.sendMessage(`[快捷] ${action}\n${actionPrompt}`);
  }

  buildQuickActionPrompt(action: string, prompt: string): string {
    switch (action) {
      case '伤病分析': {
        const injuryReq: InjuryAnalysisRequest = {
          name: '受灾者01', age: 62, gender: '男', spo2: 84, heartRate: 146,
          systolic: 88, diastolic: 54, temperature: 36.2, motionState: '长时间静止',
          level: 'RED', priorityScore: 94.5, reasons: ['SpO2 84% 严重偏低', '心率 146 bpm']
        };
        return `请进行伤病智能分析，并给出现场处置优先级、风险点和救援建议。数据：${JSON.stringify(injuryReq)}`;
      }
      case '路径优化': {
        const routeReq: RouteOptimizeRequest = {
          victimX: 70, victimY: 20, rescuerX: 64, rescuerY: 24, distance: 18.4,
          zone: 'A2商铺走廊', envTemp: 47.8, envHumidity: 88,
          gasLevel: 18.4, co: 42, o2: 20.1
        };
        return `请进行救援路径优化，输出推荐路线、避险要点和抵近注意事项。数据：${JSON.stringify(routeReq)}`;
      }
      case '体征解读': {
        const vitalsReq: VitalsTrendRequest = {
          name: '受灾者01', duration: '60',
          spo2Trend: '96→84 (持续下降)', hrTrend: '88→146 (持续上升)',
          currentSpo2: 84, currentHr: 146, minSpo2: 84, maxHr: 146
        };
        return `请解读生命体征趋势，判断主要风险并给出现场监测建议。数据：${JSON.stringify(vitalsReq)}`;
      }
      case '环境评估': {
        const envReq: EnvironmentAssessmentRequest = {
          location: 'A2商铺走廊', temperature: 47.8, humidity: 88,
          combustibleGas: 18.4, co: 42, o2: 20.1, riskLevel: 'ORANGE'
        };
        return `请评估现场环境风险，说明进入条件、防护建议和撤离触发条件。数据：${JSON.stringify(envReq)}`;
      }
      case '综合报告': {
        const reportReq: ReportRequest = {
          siteName: '地下商场A区负二层', victims: 8, rescuers: 4,
          redAlerts: 3, orangeAlerts: 1, yellowAlerts: 2
        };
        return `请生成一份简明综合灾情报告，包含态势摘要、处置优先级和下一步行动。数据：${JSON.stringify(reportReq)}`;
      }
      default:
        return prompt;
    }
  }

  async loadMessages(): Promise<void> {
    try {
      const storedMessages = await AppDatabase.loadChatMessages();
      if (storedMessages.length > 0) {
        this.messages = storedMessages;
        this.showQuickActions = false;
      } else {
        this.messages = [this.welcomeMessage];
      }
    } catch (err) {
      console.error(`加载聊天记录失败: ${JSON.stringify(err)}`);
      this.messages = [this.welcomeMessage];
    }
    this.scrollToBottom();
  }

  buildHistory(): ChatHistoryMessage[] {
    return this.messages
      .filter((msg: ChatMessage) => msg.role === 'user' || msg.role === 'ai')
      .slice(-8)
      .map((msg: ChatMessage): ChatHistoryMessage => {
        return {
          role: msg.role === 'ai' ? 'assistant' : 'user',
          content: msg.content
        };
      });
  }

  scrollToBottom(): void {
    setTimeout(() => {
      this.scroller.scrollEdge(Edge.Bottom);
    }, 120);
  }

  build() {
    Column() {
      this.TopBar();

      List({ scroller: this.scroller }) {
        ForEach(this.messages, (msg: ChatMessage) => {
          ListItem() {
            if (msg.role === 'system') {
              this.SystemBubble(msg);
            } else if (msg.role === 'ai') {
              this.AiBubble(msg);
            } else {
              this.UserBubble(msg);
            }
          }
        });

        if (this.loading) {
          ListItem() {
            this.ThinkingRow();
          }
        }
      }
      .layoutWeight(1)
      .padding({ left: 14, right: 14, top: 10 })
      .scrollBar(BarState.Off)

      if (this.showQuickActions) {
        this.QuickActionsBar();
      }

      this.InputBar();
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
        Text('AI 智能助手')
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .fontColor('#F8FAFC')
        Text('流式问答 · 后端代理')
          .fontSize(11)
          .fontColor('#94A3B8')
          .margin({ top: 2 })
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start)

      Text('清除')
        .fontSize(12)
        .fontColor('#94A3B8')
        .padding({ left: 10, right: 10, top: 7, bottom: 7 })
        .borderRadius(6)
        .backgroundColor('#0F172A')
        .border({ width: 1, color: '#1E293B' })
        .onClick(() => {
          this.messages = [this.welcomeMessage];
          this.showQuickActions = true;
          AppDatabase.clearChatMessages().catch((err: Error) => {
            console.error(`清除聊天记录失败: ${JSON.stringify(err)}`);
          });
        })
    }
    .width('100%')
    .padding({ top: 42, bottom: 12, left: 10, right: 14 })
    .backgroundColor('#020617')
    .border({ width: { bottom: 1 }, color: '#1E293B' })
  }

  @Builder
  SystemBubble(msg: ChatMessage) {
    Column() {
      Text('系统提示')
        .fontSize(11)
        .fontColor('#38BDF8')
        .fontWeight(FontWeight.Bold)
      Text(msg.content)
        .fontSize(13)
        .fontColor('#CBD5E1')
        .lineHeight(21)
        .margin({ top: 6 })
    }
    .width('100%')
    .padding(14)
    .margin({ top: 4, bottom: 6 })
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
    .alignItems(HorizontalAlign.Start)
  }

  @Builder
  AiBubble(msg: ChatMessage) {
    Row() {
      this.Avatar('AI', '#2DD4BF')
      Column() {
        Text(msg.content || '正在生成...')
          .fontSize(14)
          .fontColor('#E2E8F0')
          .lineHeight(22)
        Text(msg.time)
          .fontSize(10)
          .fontColor('#64748B')
          .margin({ top: 8 })
      }
      .layoutWeight(1)
      .padding(12)
      .margin({ left: 8 })
      .borderRadius(8)
      .backgroundColor('#0F172A')
      .border({ width: 1, color: '#1E293B' })
      .alignItems(HorizontalAlign.Start)
    }
    .width('100%')
    .margin({ top: 10 })
    .alignItems(VerticalAlign.Top)
  }

  @Builder
  UserBubble(msg: ChatMessage) {
    Row() {
      Blank()
      Column() {
        Text(msg.content)
          .fontSize(14)
          .fontColor('#F8FAFC')
          .lineHeight(22)
        Text(msg.time)
          .fontSize(10)
          .fontColor('#BAE6FD')
          .margin({ top: 8 })
      }
      .padding(12)
      .margin({ right: 8 })
      .borderRadius(8)
      .backgroundColor('#075985')
      .alignItems(HorizontalAlign.Start)
      .constraintSize({ maxWidth: '78%' })
      this.Avatar('ME', '#38BDF8')
    }
    .width('100%')
    .margin({ top: 10 })
    .alignItems(VerticalAlign.Top)
  }

  @Builder
  Avatar(text: string, color: string) {
    Column() {
      Text(text)
        .fontSize(10)
        .fontWeight(FontWeight.Bold)
        .fontColor(color)
        .fontFamily('monospace')
    }
    .width(34)
    .height(34)
    .borderRadius(7)
    .backgroundColor('#111827')
    .border({ width: 1, color: color + '66' })
    .justifyContent(FlexAlign.Center)
  }

  @Builder
  ThinkingRow() {
    Row() {
      LoadingProgress()
        .width(16)
        .height(16)
        .color('#2DD4BF')
      Text('AI 正在分析现场数据')
        .fontSize(12)
        .fontColor('#94A3B8')
        .margin({ left: 8 })
    }
    .padding({ left: 12, right: 12, top: 10, bottom: 10 })
    .margin({ top: 8, bottom: 2, left: 42 })
    .borderRadius(8)
    .backgroundColor('#0F172A')
    .border({ width: 1, color: '#1E293B' })
  }

  @Builder
  QuickActionsBar() {
    Column() {
      Row() {
        Text('快捷指令')
          .fontSize(12)
          .fontWeight(FontWeight.Bold)
          .fontColor('#CBD5E1')
        Blank()
        Text('选择后直接发起分析')
          .fontSize(10)
          .fontColor('#64748B')
      }
      .width('100%')
      .margin({ bottom: 8 })

      Row() {
        this.QuickBtn('伤病分析', '#EF4444')
        this.QuickBtn('路径优化', '#F59E0B')
        this.QuickBtn('体征解读', '#22C55E')
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)

      Row() {
        this.QuickBtn('环境评估', '#EC4899')
        this.QuickBtn('综合报告', '#38BDF8')
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)
      .margin({ top: 8 })
    }
    .width('100%')
    .padding({ left: 14, right: 14, top: 10, bottom: 10 })
    .backgroundColor('#020617')
    .border({ width: { top: 1 }, color: '#1E293B' })
  }

  @Builder
  QuickBtn(label: string, color: string) {
    Text(label)
      .fontSize(12)
      .fontWeight(FontWeight.Bold)
      .fontColor(color)
      .textAlign(TextAlign.Center)
      .height(34)
      .padding({ left: 12, right: 12 })
      .borderRadius(6)
      .backgroundColor('#0F172A')
      .border({ width: 1, color: color + '66' })
      .onClick(() => {
        if (!this.loading) {
          this.quickAction(label, '');
        }
      })
  }

  @Builder
  InputBar() {
    Row() {
      TextInput({ placeholder: '输入救援指令或现场问题', text: this.inputText })
        .placeholderColor('#64748B')
        .fontColor('#F8FAFC')
        .fontSize(14)
        .backgroundColor('#0F172A')
        .border({ width: 1, color: '#1E293B' })
        .borderRadius(8)
        .height(44)
        .layoutWeight(1)
        .padding({ left: 14, right: 14 })
        .enabled(!this.loading)
        .onChange((value: string) => { this.inputText = value; })
        .onSubmit(() => { this.sendMessage(this.inputText); })

      Button('发送')
        .width(64)
        .height(44)
        .borderRadius(8)
        .backgroundColor(this.inputText.trim() && !this.loading ? '#38BDF8' : '#1E293B')
        .fontColor(this.inputText.trim() && !this.loading ? '#020617' : '#64748B')
        .fontSize(13)
        .fontWeight(FontWeight.Bold)
        .margin({ left: 8 })
        .enabled(!this.loading)
        .onClick(() => {
          if (this.inputText.trim()) {
            this.sendMessage(this.inputText);
          }
        })
    }
    .width('100%')
    .padding({ left: 14, right: 14, top: 10, bottom: 18 })
    .backgroundColor('#020617')
    .border({ width: { top: 1 }, color: '#1E293B' })
  }
}
