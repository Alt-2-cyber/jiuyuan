<script setup lang="ts">
import { computed, ref } from 'vue'
import PanelShell from '@/components/PanelShell.vue'
import RiskBadge from '@/components/RiskBadge.vue'
import { useRescueStore } from '@/stores/rescueStore'
import type { RiskLevel } from '@/types/rescue'

const store = useRescueStore()
const filter = ref<RiskLevel | 'ALL'>('ALL')

const realVictims = computed(() => store.realVictims)
const demoVictims = computed(() => store.victims.filter((item) => !store.realVictims.some((real) => real.id === item.id)))

const victims = computed(() => {
  const source = filter.value === 'ALL' ? realVictims.value : realVictims.value.filter((item) => item.level === filter.value)
  return source
})

const filters: Array<{ label: string; value: RiskLevel | 'ALL' }> = [
  { label: '全部', value: 'ALL' },
  { label: '红色', value: 'RED' },
  { label: '橙色', value: 'ORANGE' },
  { label: '黄色', value: 'YELLOW' },
  { label: '绿色', value: 'GREEN' },
  { label: '待确认', value: 'GRAY' }
]
</script>

<template>
  <div class="page-stack">
    <div class="filter-bar">
      <button
        v-for="item in filters"
        :key="item.value"
        :class="{ active: filter === item.value }"
        @click="filter = item.value"
      >
        {{ item.label }}
      </button>
    </div>

    <PanelShell title="真实受灾端样本" subtitle="优先展示华为云同步进入 MySQL 的记录">
      <div class="table-wrap">
        <table class="data-table">
          <thead>
            <tr>
              <th>人员</th>
              <th>等级</th>
              <th>血氧</th>
              <th>心率</th>
              <th>血压</th>
              <th>距离</th>
              <th>精度</th>
              <th>任务</th>
              <th>原因</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="victim in victims" :key="victim.id">
              <td>
                <strong>{{ victim.id }}</strong>
                <span>{{ victim.name }} / {{ victim.deviceId }}</span>
              </td>
              <td><RiskBadge :level="victim.level" compact /></td>
              <td>{{ victim.vitals.spo2 || '--' }}%</td>
              <td>{{ victim.vitals.heartRate || '--' }} bpm</td>
              <td>{{ victim.vitals.systolic || '--' }}/{{ victim.vitals.diastolic || '--' }}</td>
              <td>{{ victim.location.distanceToNearestRescuer.toFixed(1) }} m</td>
              <td>{{ victim.location.accuracy.toFixed(1) }} m</td>
              <td>{{ victim.taskId || '未建任务' }} / {{ victim.status }}</td>
              <td>
                <div class="reason-list">
                  <span v-for="reason in victim.reasons" :key="reason">{{ reason }}</span>
                </div>
              </td>
            </tr>
            <tr v-if="!victims.length">
              <td colspan="9" class="empty-cell">暂无真实样本，请先同步云端数据。</td>
            </tr>
          </tbody>
        </table>
      </div>
    </PanelShell>

    <PanelShell title="演示背景数据" subtitle="仅用于界面占位，不作为实时态势依据">
      <div class="demo-note">
        <p>当前演示数据条数：{{ demoVictims.length }}</p>
        <p>这些数据保留用于无云端场景下的页面完整性展示，但不参与真实调度优先级。</p>
      </div>
    </PanelShell>
  </div>
</template>
