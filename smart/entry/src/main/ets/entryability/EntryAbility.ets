import { AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { AppDatabase } from '../common/database/AppDatabase';
import { HttpUtils } from '../common/utils/HttpUtils';

const DOMAIN = 0x0001;

/**
 * 星闪救援 - 应用入口
 */
export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    try {
      this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_DARK);
    } catch (err) {
      hilog.error(DOMAIN, 'starflash', 'Failed to set colorMode. Cause: %{public}s', JSON.stringify(err));
    }
    AppDatabase.init(this.context).then(() => {
      return HttpUtils.loadSavedBaseUrl();
    }).catch((err: Error) => {
      hilog.error(DOMAIN, 'starflash', '初始化本地数据库失败: %{public}s', JSON.stringify(err));
    });
    hilog.info(DOMAIN, 'starflash', '%{public}s', '星闪救援 - Ability onCreate');
  }

  onDestroy(): void {
    hilog.info(DOMAIN, 'starflash', '%{public}s', '星闪救援 - Ability onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    hilog.info(DOMAIN, 'starflash', '%{public}s', '星闪救援 - onWindowStageCreate');

    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        hilog.error(DOMAIN, 'starflash', '加载首页失败: %{public}s', JSON.stringify(err));
        return;
      }
      hilog.info(DOMAIN, 'starflash', '星闪救援首页加载成功');
    });
  }

  onWindowStageDestroy(): void {
    hilog.info(DOMAIN, 'starflash', '%{public}s', '星闪救援 - onWindowStageDestroy');
  }

  onForeground(): void {
    hilog.info(DOMAIN, 'starflash', '%{public}s', '星闪救援 - onForeground');
  }

  onBackground(): void {
    hilog.info(DOMAIN, 'starflash', '%{public}s', '星闪救援 - onBackground');
  }
}
