import { router } from '@kit.ArkUI';
import { promptAction } from '@kit.ArkUI';
import { HttpUtils } from '../common/utils/HttpUtils';
import { SiteOverview, AiResponse, ChatRequest } from '../common/models/DataModels';
import { ApiConstants } from '../common/constants/ApiConstants';

/**
 * 系统设置
 */
@Entry
@Component
struct SettingsPage {
  @State serverUrl: string = ApiConstants.BASE_URL;
  @State refreshInterval: number = 30;
  @State enableOfflineCache: boolean = true;
  @State notificationEnabled: boolean = true;
  @State darkMode: boolean = true;
  @State testing: boolean = false;

  aboutToAppear(): void {
    this.serverUrl = ApiConstants.BASE_URL;
    HttpUtils.loadSavedBaseUrl().then(() => {
      this.serverUrl = ApiConstants.BASE_URL;
    });
  }

  build() {
    Column() {
      this.TopBar();
      Scroll() {
        Column() {
          this.ServerSection();
          this.GeneralSection();
          this.AiSection();
          this.AboutSection();
        }
        .padding(12)
      }
      .layoutWeight(1)
    }
    .width('100%').height('100%').backgroundColor('#0A1628')
  }

  @Builder
  TopBar() {
    Row() {
      Text('\u2190').fontSize(22).fontColor('#FFFFFF').padding({ right: 12 }).onClick(() => { router.back(); })
      Text('系统设置').fontSize(18).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').layoutWeight(1)
    }
    .width('100%').padding({ top: 44, bottom: 14, left: 14, right: 14 }).backgroundColor('#0D2137')
  }

  @Builder
  ServerSection() {
    Column() {
      Text('服务器配置').fontSize(14).fontWeight(FontWeight.Bold).fontColor('#8899AA').width('100%').margin({ bottom: 12 })
      Text('服务器地址').fontSize(13).fontColor('#FFFFFF').width('100%')
      TextInput({ text: this.serverUrl, placeholder: 'http://192.168.1.18:8088' })
        .fontSize(13)
        .fontColor('#E2E8F0')
        .placeholderColor('#667788')
        .backgroundColor('#0D2137')
        .borderRadius(6)
        .padding(10)
        .margin({ top: 6, bottom: 8 })
        .onChange((value: string) => { this.serverUrl = value; })
      Text('鸿蒙模拟器/真机请填电脑当前局域网 IP，端口 8088')
        .fontSize(11).fontColor('#556677').width('100%').margin({ bottom: 10 })
      Row() {
        Button('保存地址')
          .fontSize(13).backgroundColor('#1E3A5F').fontColor('#FFFFFF').borderRadius(6)
          .padding({ left: 14, right: 14, top: 8, bottom: 8 })
          .onClick(() => { this.saveServerUrl(); })
        Button(this.testing ? '测试中...' : '测试数据接口')
          .fontSize(13).backgroundColor('#00838F').fontColor('#FFFFFF').borderRadius(6)
          .margin({ left: 10 })
          .padding({ left: 14, right: 14, top: 8, bottom: 8 })
          .enabled(!this.testing)
          .onClick(() => { this.testBackend(); })
      }
      .width('100%')
      this.SettingItem('刷新间隔', `${this.refreshInterval} 秒`, '数据自动刷新频率')
    }
    .width('100%').padding(14).margin({ top: 10 }).borderRadius(10).backgroundColor('#132D45')
  }

  @Builder
  GeneralSection() {
    Column() {
      Text('通用设置').fontSize(14).fontWeight(FontWeight.Bold).fontColor('#8899AA').width('100%').margin({ bottom: 12 })
      this.ToggleItem('离线缓存', '启用本地数据库缓存', this.enableOfflineCache, (val) => { this.enableOfflineCache = val; })
      this.ToggleItem('告警通知', '接收高优先级告警', this.notificationEnabled, (val) => { this.notificationEnabled = val; })
    }
    .width('100%').padding(14).margin({ top: 10 }).borderRadius(10).backgroundColor('#132D45')
  }

  @Builder
  AiSection() {
    Column() {
      Text('AI 配置').fontSize(14).fontWeight(FontWeight.Bold).fontColor('#8899AA').width('100%').margin({ bottom: 12 })

      Text('AI 服务由后端统一代理').fontSize(12).fontColor('#66BB6A').width('100%')
      Text('模型: glm-4.7-flash (智谱AI，可在后端配置)').fontSize(12).fontColor('#8899AA').width('100%').margin({ top: 4 })
      Text('APP 端点: 后端 /api/ai/chat 与 /api/ai/chat/stream').fontSize(12).fontColor('#8899AA').width('100%').margin({ top: 2 })

      Button('测试 AI 连接')
        .fontSize(13).backgroundColor('#00838F').fontColor('#FFFFFF').borderRadius(6)
        .margin({ top: 10 }).padding({ left: 16, right: 16, top: 8, bottom: 8 })
        .onClick(() => { this.testAiConnection(); })
    }
    .width('100%').padding(14).margin({ top: 10 }).borderRadius(10)
    .backgroundColor('#132D45').border({ width: 1, color: '#00BCD433' })
  }

  @Builder
  AboutSection() {
    Column() {
      Text('关于').fontSize(14).fontWeight(FontWeight.Bold).fontColor('#8899AA').width('100%').margin({ bottom: 12 })

      Row() {
        Text('\u{1F3E5}').fontSize(32)
        Column() {
          Text('星闪救援指挥系统').fontSize(16).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
          Text('基于华为星闪 NearLink 技术').fontSize(12).fontColor('#8899AA').margin({ top: 4 })
        }
        .layoutWeight(1).alignItems(HorizontalAlign.Start).margin({ left: 12 })
      }
      .width('100%')

      Divider().color('#1A3345').margin({ top: 12, bottom: 12 })

      this.AboutItem('版本', 'v1.0.0')
      this.AboutItem('协议', '星闪 NearLink SLE')
      this.AboutItem('后端', 'Spring Boot 3.3.5')
      this.AboutItem('数据库', 'MySQL 8.0')
      this.AboutItem('AI引擎', '智谱 GLM-4.7-Flash')
      this.AboutItem('鸿蒙SDK', 'API 6.0.2')
    }
    .width('100%').padding(14).margin({ top: 10, bottom: 20 }).borderRadius(10).backgroundColor('#132D45')
  }

  @Builder
  SettingItem(label: string, value: string, hint: string) {
    Row() {
      Column() {
        Text(label).fontSize(14).fontColor('#FFFFFF')
        Text(hint).fontSize(11).fontColor('#556677').margin({ top: 2 })
      }
      .layoutWeight(1).alignItems(HorizontalAlign.Start)
      Text(value).fontSize(13).fontColor('#4FC3F7')
    }
    .width('100%').padding({ top: 8, bottom: 8 })
  }

  @Builder
  ToggleItem(label: string, desc: string, value: boolean, onChange: (val: boolean) => void) {
    Row() {
      Column() {
        Text(label).fontSize(14).fontColor('#FFFFFF')
        Text(desc).fontSize(11).fontColor('#556677').margin({ top: 2 })
      }
      .layoutWeight(1).alignItems(HorizontalAlign.Start)
      Toggle({ type: ToggleType.Switch, isOn: value })
        .selectedColor('#00BCD4')
        .onChange((isOn: boolean) => { onChange(isOn); })
    }
    .width('100%').padding({ top: 8, bottom: 8 })
  }

  @Builder
  AboutItem(label: string, value: string) {
    Row() {
      Text(label).fontSize(13).fontColor('#8899AA').width(80)
      Text(value).fontSize(13).fontColor('#FFFFFF')
    }
    .width('100%').padding({ top: 4, bottom: 4 })
  }

  async saveServerUrl(): Promise<void> {
    try {
      await HttpUtils.saveBaseUrl(this.serverUrl);
      this.serverUrl = ApiConstants.BASE_URL;
      promptAction.showToast({ message: `已保存 ${this.serverUrl}`, duration: 2000 });
    } catch (err) {
      promptAction.showToast({ message: '保存失败', duration: 2000 });
    }
  }

  async testBackend(): Promise<void> {
    this.testing = true;
    try {
      await HttpUtils.saveBaseUrl(this.serverUrl);
      const data = await HttpUtils.get<SiteOverview>(ApiConstants.OVERVIEW);
      promptAction.showDialog({
        title: '数据接口测试',
        message: `连接成功\n当前地址: ${ApiConstants.BASE_URL}\n现场: ${data.siteName ?? data.phase ?? '已返回态势数据'}`,
        buttons: [{ text: '确定', color: '#00BCD4' }]
      });
    } catch (err) {
      promptAction.showDialog({
        title: '数据接口测试',
        message: `连接失败\n请确认后端已启动，并填写电脑 IP，例如 http://192.168.1.18:8088\n${JSON.stringify(err)}`,
        buttons: [{ text: '确定', color: '#FF5252' }]
      });
    } finally {
      this.testing = false;
    }
  }

  async testAiConnection(): Promise<void> {
    this.testing = true;
    try {
      await HttpUtils.saveBaseUrl(this.serverUrl);
      const chatReq: ChatRequest = { message: '你好，请做简短自我介绍。' };
      const response: AiResponse = await HttpUtils.post<AiResponse>(ApiConstants.AI_CHAT, chatReq);
      promptAction.showDialog({
        title: 'AI 连接测试',
        message: 'AI服务连接成功！\n\n' + response.result.substring(0, 120) + '...',
        buttons: [{ text: '确定', color: '#00BCD4' }]
      });
    } catch (err) {
      promptAction.showDialog({
        title: 'AI 连接测试',
        message: 'AI服务连接失败，请检查服务器地址。',
        buttons: [{ text: '确定', color: '#FF5252' }]
      });
    } finally {
      this.testing = false;
    }
  }
}
