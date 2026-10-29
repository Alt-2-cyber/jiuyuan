import relationalStore from '@ohos.data.relationalStore';
import type common from '@ohos.app.ability.common';
import { ChatMessage } from '../models/DataModels';

const DB_NAME = 'starflash_rescue.db';
const DB_VERSION = 1;

const CREATE_APP_CONFIG_SQL = `
CREATE TABLE IF NOT EXISTS app_config (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL,
  updated_at INTEGER NOT NULL
)`;

const CREATE_CHAT_MESSAGES_SQL = `
CREATE TABLE IF NOT EXISTS chat_messages (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  role TEXT NOT NULL,
  content TEXT NOT NULL,
  time TEXT NOT NULL,
  type TEXT,
  created_at INTEGER NOT NULL
)`;

export class AppDatabase {
  private static store: relationalStore.RdbStore | null = null;
  private static initTask: Promise<void> | null = null;

  static async init(context: common.UIAbilityContext): Promise<void> {
    if (AppDatabase.store !== null) {
      return;
    }
    if (AppDatabase.initTask !== null) {
      await AppDatabase.initTask;
      return;
    }

    AppDatabase.initTask = AppDatabase.openStore(context);
    await AppDatabase.initTask;
  }

  private static async openStore(context: common.UIAbilityContext): Promise<void> {
    const config: relationalStore.StoreConfig = {
      name: DB_NAME,
      securityLevel: relationalStore.SecurityLevel.S1
    };

    const store = await relationalStore.getRdbStore(context, config);
    await store.executeSql(CREATE_APP_CONFIG_SQL);
    await store.executeSql(CREATE_CHAT_MESSAGES_SQL);
    store.version = DB_VERSION;
    AppDatabase.store = store;
  }

  static async saveConfig(key: string, value: string): Promise<void> {
    const store = await AppDatabase.requireStore();
    await store.executeSql(
      'INSERT OR REPLACE INTO app_config (key, value, updated_at) VALUES (?, ?, ?)',
      [key, value, Date.now()]
    );
  }

  static async getConfig(key: string, fallback: string): Promise<string> {
    const store = await AppDatabase.requireStore();
    const resultSet = await store.querySql('SELECT value FROM app_config WHERE key = ?', [key]);
    try {
      if (resultSet.goToNextRow()) {
        return resultSet.getString(0);
      }
      return fallback;
    } finally {
      resultSet.close();
    }
  }

  static async addChatMessage(message: ChatMessage): Promise<void> {
    const store = await AppDatabase.requireStore();
    await store.executeSql(
      'INSERT INTO chat_messages (role, content, time, type, created_at) VALUES (?, ?, ?, ?, ?)',
      [message.role, message.content, message.time, message.type ?? '', Date.now()]
    );
  }

  static async loadChatMessages(limit: number = 80): Promise<ChatMessage[]> {
    const store = await AppDatabase.requireStore();
    const resultSet = await store.querySql(
      'SELECT role, content, time, type FROM chat_messages ORDER BY id DESC LIMIT ?',
      [limit]
    );
    const messages: ChatMessage[] = [];
    try {
      while (resultSet.goToNextRow()) {
        const role = resultSet.getString(0) as 'user' | 'ai' | 'system';
        const content = resultSet.getString(1);
        const time = resultSet.getString(2);
        const type = resultSet.getString(3);
        messages.unshift({ role, content, time, type });
      }
      return messages;
    } finally {
      resultSet.close();
    }
  }

  static async clearChatMessages(): Promise<void> {
    const store = await AppDatabase.requireStore();
    await store.executeSql('DELETE FROM chat_messages');
  }

  private static async requireStore(): Promise<relationalStore.RdbStore> {
    if (AppDatabase.initTask !== null) {
      await AppDatabase.initTask;
    }
    if (AppDatabase.store === null) {
      throw new Error('AppDatabase is not initialized');
    }
    return AppDatabase.store;
  }
}
