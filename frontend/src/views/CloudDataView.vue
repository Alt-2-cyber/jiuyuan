<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { AlertTriangle, CloudDownload, Database, HeartPulse, RefreshCw } from 'lucide-vue-next'
import PanelShell from '@/components/PanelShell.vue'
import { rescueApi } from '@/api/rescue'
import type { CloudSyncResult, CloudVictimSample, CloudVictimSummary } from '@/types/rescue'

const loading = ref(false)
const syncing = ref(false)
const summary = ref<CloudVictimSummary | null>(null)
const syncResult = ref<CloudSyncResult | null>(null)
const errorMessage = ref('')

const samples = computed<CloudVictimSample[]>(() => summary.value?.items ?? [])
const latest = computed(() => summary.value?.latest ?? null)
const latestValidVital = computed(() => samples.value.find((item) => hasAnyValidVital(item)) ?? null)
const abnormalCount = computed(() =>
  samples.value.filter((item) => item.sos || isLowSpo2(item.spo2) || isAbnormalHeartRate(item.heartRate)).length
)
const syncDiagnosis = computed(() => {
  const message = syncResult.value?.message || errorMessage.value
  if (!message) return []
  if (message.includes('IOTDA.000021') || message.includes('not subscribed to IoTDA') || message.includes('403')) {
    return [
      '当前失败来自华为云权限或实例归属校验，不是前端按钮问题。',
      '检查 AK/SK 对应的 IAM 用户是否拥有 IoTDA FullAccess 或 IoTDA 管理员权限。',
      '确认该用户和设备所在的 IoTDA 实例、区域 cn-north-4、实例 ff1fe1ecbb 属于同一账号/项目。',
      '同步失败不会清空 MySQL，页面仍会显示已入库的真实设备样本。'
    ]
  }
  if (message.includes('IOTDA.000001') || message.includes('Internal server error')) {
    return [
      '华为云返回内部错误，优先检查 instanceId、endpoint、projectId 是否匹配。',
      '当前专业版/标准版 IoTDA 实例需要带实例 ID，当前项目已配置 ff1fe1ecbb。',
      '如果持续失败，重新确认控制台设备详情里的应用侧接入地址和项目 ID。'
    ]
  }
  return []
})

async function loadCloudData() {
  loading.value = true
  errorMessage.value = ''
  syncResult.value = null
  try {
    summary.value = await rescueApi.cloudVictimSamples(120)
  } catch (error) {
    errorMessage.value = error instanceof Error ? error.message : '读取云端入库数据失败'
  } finally {
    loading.value = false
  }
}

async function syncCloudData() {
  syncing.value = true
  errorMessage.value = ''
  syncResult.value = null
  try {
    syncResult.value = await rescueApi.syncCloudVictimSamples()
    await loadCloudData()
  } catch (error) {
    errorMessage.value = error instanceof Error ? error.message : '同步华为云失败'
  } finally {
    syncing.value = false
  }
}

function hasValidSpo2(value: number | null | undefined) {
  return typeof value === 'number' && value >= 50 && value <= 100
}

function hasValidHeartRate(value: number | null | undefined) {
  return typeof value === 'number' && value >= 30 && value <= 220
}

function hasAnyValidVital(item: CloudVictimSample | null | undefined) {
  return Boolean(item && (hasValidSpo2(item.spo2) || hasValidHeartRate(item.heartRate)))
}

function isLowSpo2(value: number | null | undefined) {
  return hasValidSpo2(value) && Number(value) < 90
}

function isAbnormalHeartRate(value: number | null | undefined) {
  return hasValidHeartRate(value) && (Number(value) < 50 || Number(value) > 120)
}

function formatSpo2(value: number | null | undefined) {
  return hasValidSpo2(value) ? `${value}%` : '未采集'
}

function formatHeartRate(value: number | null | undefined) {
  return hasValidHeartRate(value) ? `${value} bpm` : '未采集'
}

function formatVitalPair(item: CloudVictimSample | null | undefined) {
  if (!item) return '--'
  if (!hasAnyValidVital(item)) return '未采集'
  return `${formatSpo2(item.spo2)} / ${formatHeartRate(item.heartRate)}`
}

function rawHint(value: number | null | undefined) {
  return typeof value === 'number' ? `原始值 ${value}` : '无上报'
}

function rawText(item: CloudVictimSample, ...keys: string[]) {
  for (const key of keys) {
    const value = item.raw?.[key]
    if (typeof value === 'string' && value.trim()) return value.trim()
    if (typeof value === 'number' && Number.isFinite(value)) return String(value)
  }
  return '--'
}

function rawNumber(item: CloudVictimSample, ...keys: string[]) {
  for (const key of keys) {
    const value = item.raw?.[key]
    if (typeof value === 'number' && Number.isFinite(value)) return value
    if (typeof value === 'string' && value.trim() && !Number.isNaN(Number(value))) return Number(value)
  }
  return null
}

function victimNo(item: CloudVictimSample) {
  return rawText(item, 'victim_id', 'victimId', 'target_id')
}

function cloudDeviceId(item: CloudVictimSample) {
  return rawText(item, '_cloud_device_id', 'cloud_device_id') !== '--'
    ? rawText(item, '_cloud_device_id', 'cloud_device_id')
    : item.deviceId
}

function formatDistance(item: CloudVictimSample) {
  const value = rawNumber(item, 'distance', 'distance_m', 'dist', 'range')
  return value === null ? '--' : `${value.toFixed(1)} m`
}

function riskText(item: CloudVictimSample) {
  if (item.sos) return 'SOS'
  if (isLowSpo2(item.spo2)) return '低血氧'
  if (isAbnormalHeartRate(item.heartRate)) return '心率异常'
  if (!hasAnyValidVital(item)) return '未采集'
  return '正常'
}

function riskClass(item: CloudVictimSample) {
  const text = riskText(item)
  if (text === 'SOS' || text === '低血氧') return 'risk-red'
  if (text === '心率异常') return 'risk-orange'
  if (text === '未采集') return 'risk-gray'
  return 'risk-green'
}

function vitalClass(valid: boolean) {
  return valid ? 'valid' : 'invalid'
}

onMounted(loadCloudData)
</script>

<template>
  <div class="page-stack cloud-data-page">
    <PanelShell
      title="华为云 IoTDA 数据入库"
      subtitle="从设备影子同步到 MySQL，并在前端展示最新生命体征数据"
    >
      <div class="cloud-toolbar">
        <div class="cloud-status">
          <span :class="{ active: summary?.cloudEnabled }"></span>
          <strong>{{ summary?.cloudEnabled ? '云同步已启用' : '云同步未启用' }}</strong>
          <small>service_id: {{ summary?.serviceId || 'environment' }}</small>
        </div>
        <div class="cloud-actions">
          <button class="ghost-button" :disabled="loading" @click="loadCloudData">
            <RefreshCw :size="16" />
            刷新
          </button>
          <button class="primary-button" :disabled="syncing" @click="syncCloudData">
            <CloudDownload :size="16" />
            {{ syncing ? '同步中' : '同步华为云' }}
          </button>
        </div>
      </div>

      <div v-if="syncResult" class="sync-message" :class="{ failed: !syncResult.success }">
        {{ syncResult.message }}
      </div>
      <div v-if="syncDiagnosis.length" class="sync-diagnosis">
        <strong>同步失败处理：</strong>
        <ul>
          <li v-for="item in syncDiagnosis" :key="item">{{ item }}</li>
        </ul>
      </div>
      <div v-if="errorMessage" class="sync-message failed">{{ errorMessage }}</div>
    </PanelShell>

    <div class="cloud-kpis">
      <article>
        <Database :size="22" />
        <span>MySQL 已入库</span>
        <strong>{{ summary?.total ?? 0 }}</strong>
      </article>
      <article>
        <HeartPulse :size="22" />
        <span>最新血氧 / 心率</span>
        <strong>{{ formatVitalPair(latest) }}</strong>
        <small v-if="latest && !hasAnyValidVital(latest)">设备已上报，但传感器暂无有效采样</small>
      </article>
      <article>
        <HeartPulse :size="22" />
        <span>最近有效采样</span>
        <strong>{{ formatVitalPair(latestValidVital) }}</strong>
        <small v-if="!latestValidVital">等待 MAX30102 返回有效心率血氧</small>
      </article>
      <article>
        <AlertTriangle :size="22" />
        <span>异常记录</span>
        <strong>{{ abnormalCount }}</strong>
      </article>
    </div>

    <PanelShell title="最新上报数据" subtitle="0 或空值按未采集处理，避免误判为真实心率血氧">
      <div class="table-wrap">
        <table class="data-table cloud-table">
          <thead>
            <tr>
              <th>时间</th>
              <th>设备</th>
              <th>受灾端编号</th>
              <th>距离</th>
              <th>序号</th>
              <th>血氧</th>
              <th>心率</th>
              <th>收缩压</th>
              <th>电量</th>
              <th>状态</th>
              <th>来源</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="item in samples" :key="item.id">
              <td>{{ item.eventTime }}</td>
              <td class="cloud-device">
                <strong>{{ cloudDeviceId(item) }}</strong>
                <span>{{ item.serviceId }}</span>
              </td>
              <td>{{ victimNo(item) }}</td>
              <td>{{ formatDistance(item) }}</td>
              <td>{{ item.seq ?? '--' }}</td>
              <td>
                <b class="vital-value" :class="vitalClass(hasValidSpo2(item.spo2))">
                  {{ formatSpo2(item.spo2) }}
                </b>
                <small v-if="!hasValidSpo2(item.spo2)" class="vital-subtext">{{ rawHint(item.spo2) }}</small>
              </td>
              <td>
                <b class="vital-value" :class="vitalClass(hasValidHeartRate(item.heartRate))">
                  {{ formatHeartRate(item.heartRate) }}
                </b>
                <small v-if="!hasValidHeartRate(item.heartRate)" class="vital-subtext">
                  {{ rawHint(item.heartRate) }}
                </small>
              </td>
              <td>{{ item.systolic ?? '--' }}<span v-if="item.systolic"> mmHg</span></td>
              <td>{{ item.battery ?? '--' }}<span v-if="item.battery">%</span></td>
              <td><span class="risk-badge compact" :class="riskClass(item)">{{ riskText(item) }}</span></td>
              <td>{{ item.source }}</td>
            </tr>
            <tr v-if="!samples.length">
              <td colspan="11" class="empty-cell">
                {{ loading ? '正在读取数据...' : '暂无入库数据，点击“同步华为云”读取设备影子' }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </PanelShell>

    <PanelShell title="心率血氧说明" subtitle="当前页面只展示已写入 cloud_victim_sample 的数据">
      <div class="cloud-help">
        <article>
          <strong>有效血氧</strong>
          <p>血氧值在 50% 到 100% 之间才按有效数据展示，低于 90% 标记为低血氧。</p>
        </article>
        <article>
          <strong>有效心率</strong>
          <p>心率值在 30 到 220 bpm 之间才按有效数据展示，低于 50 或高于 120 标记为异常。</p>
        </article>
        <article>
          <strong>未采集状态</strong>
          <p>华为云上报 0 或空值时显示为未采集，通常表示手指未放好、传感器未读到有效脉搏或设备端仍在初始化。</p>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
