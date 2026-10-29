import { http } from '@kit.NetworkKit';
import { util } from '@kit.ArkTS';
import { BusinessError } from '@kit.BasicServicesKit';
import { ApiConstants } from '../constants/ApiConstants';
import { AppDatabase } from '../database/AppDatabase';

/**
 * HTTP 网络请求工具类
 * 封装 HarmonyOS HTTP 请求，自动尝试多个后端地址
 */
export class HttpUtils {
  static async get<T>(path: string): Promise<T> {
    return HttpUtils.send<T>(http.RequestMethod.GET, path);
  }

  static async post<T>(path: string, body: object): Promise<T> {
    return HttpUtils.send<T>(http.RequestMethod.POST, path, body);
  }

  static async send<T>(method: http.RequestMethod, path: string, body?: object): Promise<T> {
    const bases = HttpUtils.baseCandidates();
    let lastError: Error = new Error('网络请求失败');
    for (let i = 0; i < bases.length; i++) {
      const url = HttpUtils.join(bases[i], path);
      try {
        const result = await HttpUtils.requestUrl<T>(method, url, body);
        HttpUtils.rememberBase(bases[i]);
        return result;
      } catch (err) {
        lastError = new Error(`${err}`);
        console.error(`[HttpUtils] ${method} ${url} 失败: ${lastError.message}`);
      }
    }
    throw lastError;
  }

  static buildUrl(path: string): string {
    return HttpUtils.join(ApiConstants.BASE_URL, path);
  }

  static normalizeBase(url: string): string {
    let value = url.trim();
    if (value.endsWith('/')) {
      value = value.substring(0, value.length - 1);
    }
    if (!value.startsWith('http://') && !value.startsWith('https://')) {
      value = 'http://' + value;
    }
    return value;
  }

  static async loadSavedBaseUrl(): Promise<void> {
    try {
      const saved = await AppDatabase.getConfig('server_url', ApiConstants.BASE_URL);
      if (saved && saved.length > 0) {
        ApiConstants.BASE_URL = HttpUtils.normalizeBase(saved);
      }
    } catch (err) {
      console.error(`[HttpUtils] 读取服务器地址失败: ${JSON.stringify(err)}`);
    }
  }

  static async saveBaseUrl(url: string): Promise<void> {
    ApiConstants.BASE_URL = HttpUtils.normalizeBase(url);
    await AppDatabase.saveConfig('server_url', ApiConstants.BASE_URL);
  }

  private static rememberBase(base: string): void {
    if (base === ApiConstants.BASE_URL) {
      return;
    }
    ApiConstants.BASE_URL = base;
    AppDatabase.saveConfig('server_url', base).catch((err: Error) => {
      console.error(`[HttpUtils] 保存服务器地址失败: ${JSON.stringify(err)}`);
    });
  }

  private static baseCandidates(): string[] {
    const result: string[] = [];
    HttpUtils.pushUnique(result, ApiConstants.BASE_URL);
    for (let i = 0; i < ApiConstants.FALLBACK_URLS.length; i++) {
      HttpUtils.pushUnique(result, ApiConstants.FALLBACK_URLS[i]);
    }
    return result;
  }

  private static pushUnique(list: string[], url: string): void {
    const normalized = HttpUtils.normalizeBase(url);
    if (normalized.length === 0) {
      return;
    }
    if (list.indexOf(normalized) < 0) {
      list.push(normalized);
    }
  }

  private static join(base: string, path: string): string {
    if (path.startsWith('http://') || path.startsWith('https://')) {
      return path;
    }
    const normalized = HttpUtils.normalizeBase(base);
    if (normalized.endsWith('/') && path.startsWith('/')) {
      return normalized + path.substring(1);
    }
    if (!normalized.endsWith('/') && !path.startsWith('/')) {
      return normalized + '/' + path;
    }
    return normalized + path;
  }

  private static requestUrl<T>(method: http.RequestMethod, url: string, body?: object): Promise<T> {
    console.info(`[HttpUtils] ${method} ${url}`);
    return new Promise<T>((resolve: Function, reject: Function) => {
      const httpRequest = http.createHttp();
      const header: Record<string, string> = {};
      header['Content-Type'] = 'application/json';
      header['Accept'] = 'application/json';
      const options: http.HttpRequestOptions = {
        method: method,
        header: header,
        extraData: body ? JSON.stringify(body) : '',
        expectDataType: http.HttpDataType.STRING,
        connectTimeout: 4000,
        readTimeout: 12000
      };

      httpRequest.request(url, options, (err: BusinessError, data: http.HttpResponse) => {
        httpRequest.destroy();
        if (err) {
          reject(new Error(`连接失败 ${url} (${err.code ?? 'network'})`));
          return;
        }
        if (data.responseCode < 200 || data.responseCode >= 300) {
          reject(new Error(`HTTP ${data.responseCode}`));
          return;
        }
        try {
          const bodyStr = HttpUtils.toText(data.result);
          const result: T = JSON.parse(bodyStr) as T;
          console.info(`[HttpUtils] ${method} ${url} 成功`);
          resolve(result);
        } catch (parseErr) {
          reject(new Error('JSON parse failed'));
        }
      });
    });
  }

  private static toText(result: string | Object | ArrayBuffer): string {
    if (typeof result === 'string') {
      return result;
    }
    try {
      const buffer = result as ArrayBuffer;
      const decoder = util.TextDecoder.create('utf-8');
      const decoded = decoder.decodeWithStream(new Uint8Array(buffer), { stream: false }) ?? '';
      if (decoded && decoded.length > 0) {
        return decoded;
      }
    } catch (err) {
      console.error(`[HttpUtils] 二进制响应解码失败: ${JSON.stringify(err)}`);
    }
    return JSON.stringify(result);
  }
}
