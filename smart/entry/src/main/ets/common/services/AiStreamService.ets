import { http } from '@kit.NetworkKit';
import { util } from '@kit.ArkTS';
import { BusinessError } from '@kit.BasicServicesKit';
import { ApiConstants } from '../constants/ApiConstants';
import { ChatHistoryMessage, ChatRequest } from '../models/DataModels';
import { HttpUtils } from '../utils/HttpUtils';

type ChunkCallback = (chunk: string) => void;

export class AiStreamService {
  static async chatStream(
    message: string,
    history: ChatHistoryMessage[],
    onChunk: ChunkCallback
  ): Promise<void> {
    const url = HttpUtils.buildUrl(ApiConstants.AI_CHAT_STREAM);
    const requestBody: ChatRequest = { message, history };
    const httpRequest = http.createHttp();
    const decoder = util.TextDecoder.create('utf-8');
    let sseBuffer = '';
    let finished = false;

    const onDataReceive = (data: ArrayBuffer): void => {
      try {
        const decoded = decoder.decodeWithStream(new Uint8Array(data), { stream: true }) ?? '';
        if (decoded.length === 0) {
          return;
        }
        sseBuffer += decoded;
        sseBuffer = AiStreamService.consumeSseBuffer(sseBuffer, onChunk);
      } catch (err) {
        console.error(`AI流式数据解析失败: ${JSON.stringify(err)}`);
        finished = true;
        httpRequest.destroy();
      }
    };

    const onDataEnd = (): void => {
      try {
        const tail = decoder.decodeWithStream(new Uint8Array(), { stream: false }) ?? '';
        if (tail.length > 0) {
          sseBuffer += tail;
        }
        if (sseBuffer.length > 0) {
          AiStreamService.consumeSseBuffer(sseBuffer + '\n\n', onChunk);
        }
      } catch (err) {
        console.error(`AI流式结束解析失败: ${JSON.stringify(err)}`);
      } finally {
        finished = true;
        httpRequest.destroy();
      }
    };

    httpRequest.on('dataReceive', onDataReceive);
    httpRequest.on('dataEnd', onDataEnd);

    await new Promise<void>((resolve: Function, reject: Function) => {
      httpRequest.requestInStream(url, {
        method: http.RequestMethod.POST,
        header: {
          'Content-Type': 'application/json',
          'Accept': 'text/event-stream'
        },
        extraData: JSON.stringify(requestBody),
        connectTimeout: 10000,
        readTimeout: 120000
      }, (err: BusinessError, statusCode: number) => {
        if (err) {
          httpRequest.destroy();
          reject(new Error('网络请求失败，请检查后端服务。'));
          return;
        }
        if (statusCode < 200 || statusCode >= 300) {
          httpRequest.destroy();
          reject(new Error(`AI服务返回异常：${statusCode}`));
          return;
        }

        const timer = setInterval(() => {
          if (finished) {
            clearInterval(timer);
            resolve();
          }
        }, 80);
      });
    });
  }

  private static consumeSseBuffer(buffer: string, onChunk: ChunkCallback): string {
    let remain = buffer.replace(/\r\n/g, '\n');
    let index = remain.indexOf('\n\n');
    while (index >= 0) {
      const eventBlock = remain.substring(0, index);
      remain = remain.substring(index + 2);
      AiStreamService.consumeSseEvent(eventBlock, onChunk);
      index = remain.indexOf('\n\n');
    }
    return remain;
  }

  private static consumeSseEvent(eventBlock: string, onChunk: ChunkCallback): void {
    const lines = eventBlock.split('\n');
    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line.startsWith('data:')) {
        continue;
      }
      const data = line.substring(5).trim();
      if (!data || data === '[DONE]') {
        continue;
      }
      onChunk(data);
    }
  }
}
