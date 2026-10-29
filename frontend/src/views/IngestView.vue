<script setup lang="ts">
import { computed } from 'vue'
import {
  Cable,
  CheckCircle2,
  Cloud,
  Database,
  Laptop,
  Radio,
  Server,
  Usb,
  Wifi
} from 'lucide-vue-next'
import PanelShell from '@/components/PanelShell.vue'
import RiskBadge from '@/components/RiskBadge.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()

const latestVictim = computed(() => store.victims[0])

const localSteps = [
  { label: '受灾端 WS63', detail: 'SLE Server / VD-001', icon: Radio, state: 'online' },
  { label: '救援端 WS63', detail: 'SLE Client / RSSI 趋势', icon: Cable, state: 'online' },
  { label: 'USB 串口', detail: '115200 / JSON Lines', icon: Usb, state: 'ready' },
  { label: 'Spring Boot', detail: 'POST /api/device-ingest', icon: Server, state: 'ready' },
  { label: 'MySQL', detail: 'vital_sample / location_sample', icon: Database, state: 'ready' },
  { label: 'Vue3 前端', detail: 'WebSocket / 轮询刷新', icon: Laptop, state: 'online' }
]

const cloudSteps = [
  { label: 'WS63 网关', detail: '星闪汇聚 + Wi-Fi', icon: Wifi },
  { label: '华为云 IoTDA', detail: 'MQTT 设备接入', icon: Cloud },
  { label: '规则转发', detail: 'HTTP/MQTT 到后端', icon: Server },
  { label: 'MySQL 入库', detail: '历史数据与复盘', icon: Database }
]

const samplePayload = computed(() => {
  const victim = latestVictim.value
  return {
    msgType: 'RESCUE_TERMINAL_REPORT',
    victimDeviceId: victim?.deviceId || 'VD-001',
    rescuerDeviceId: 'RD-001',
    spo2: victim?.vitals.spo2 || 88,
    heartRate: victim?.vitals.heartRate || 124,
    systolic: victim?.vitals.systolic || 96,
    battery: victim?.vitals.battery || 82,
    rssi: -61,
    distance: 10.8,
    trend: 'CLOSING',
    sos: true
  }
})
</script>

<template>
  <div class="page-grid ingest-grid">
    <PanelShell title="当前推荐链路" subtitle="两块 WS63 先走本地串口回传，最快跑通 MySQL 和 Web 显示" class="wide-panel">
      <div class="ingest-flow">
        <article v-for="step in localSteps" :key="step.label" class="flow-node">
          <div class="flow-icon">
            <component :is="step.icon" :size="22" />
          </div>
          <strong>{{ step.label }}</strong>
          <span>{{ step.detail }}</span>
          <i :class="`flow-state flow-${step.state}`">{{ step.state }}</i>
        </article>
      </div>
    </PanelShell>

    <PanelShell title="基础版接入状态" subtitle="先不强依赖 Wi-Fi 和云，救援端作为数据汇聚点">
      <div class="ingest-status">
        <article>
          <CheckCircle2 :size="20" />
          <div>
            <strong>WS63 星闪链路</strong>
            <span>受灾端到救援端已按 SLE UART 基础版设计</span>
          </div>
          <RiskBadge level="GREEN" compact />
        </article>
        <article>
          <CheckCircle2 :size="20" />
          <div>
            <strong>串口回传协议</strong>
            <span>JSON Lines，适合 Java 串口采集程序读取</span>
          </div>
          <RiskBadge level="YELLOW" compact />
        </article>
        <article>
          <CheckCircle2 :size="20" />
          <div>
            <strong>后端入库</strong>
            <span>建议新增 /api/device-ingest/rescue-terminal</span>
          </div>
          <RiskBadge level="YELLOW" compact />
        </article>
      </div>
    </PanelShell>

    <PanelShell title="串口数据样例" subtitle="救援端读取后直接 POST 到后端">
      <pre class="payload-box">{{ JSON.stringify(samplePayload, null, 2) }}</pre>
    </PanelShell>

    <PanelShell title="华为云升级链路" subtitle="等本地链路跑通后，再把串口替换成 Wi-Fi/MQTT/IoTDA" class="wide-panel">
      <div class="cloud-path">
        <article v-for="step in cloudSteps" :key="step.label">
          <component :is="step.icon" :size="24" />
          <div>
            <strong>{{ step.label }}</strong>
            <span>{{ step.detail }}</span>
          </div>
        </article>
      </div>
    </PanelShell>

    <PanelShell title="MySQL 入库规划" subtitle="前端展示依赖后端聚合后的实时状态">
      <div class="table-wrap">
        <table class="data-table compact-table">
          <thead>
            <tr>
              <th>数据</th>
              <th>表</th>
              <th>字段</th>
              <th>刷新</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>生命体征</td>
              <td>vital_sample</td>
              <td>spo2 / heart_rate / systolic / battery</td>
              <td>1s</td>
            </tr>
            <tr>
              <td>定位趋势</td>
              <td>location_sample</td>
              <td>rssi / distance / trend / quality</td>
              <td>1s</td>
            </tr>
            <tr>
              <td>告警事件</td>
              <td>alert_event</td>
              <td>sos / low_spo2 / offline</td>
              <td>事件触发</td>
            </tr>
          </tbody>
        </table>
      </div>
    </PanelShell>

    <PanelShell title="接入模式选择" subtitle="项目阶段建议">
      <div class="mode-list">
        <article class="mode-card active">
          <strong>阶段 1：串口本地接入</strong>
          <p>最适合两块 WS63 基础版，调试快、链路短、容易定位问题。</p>
        </article>
        <article class="mode-card">
          <strong>阶段 2：救援端 Wi-Fi 直连后端</strong>
          <p>救援端或旁路网关联网，直接 HTTP/MQTT 上报 Spring Boot。</p>
        </article>
        <article class="mode-card">
          <strong>阶段 3：华为云 IoTDA</strong>
          <p>用于远程部署、设备管理、规则转发和多现场汇聚。</p>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
