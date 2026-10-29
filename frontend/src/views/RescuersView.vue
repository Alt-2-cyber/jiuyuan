<script setup lang="ts">
import PanelShell from '@/components/PanelShell.vue'
import RiskBadge from '@/components/RiskBadge.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()
</script>

<template>
  <div class="page-grid rescuer-grid">
    <PanelShell title="救援人员终端" subtitle="导航终端、电量、任务和在线状态" class="wide-panel">
      <div class="rescue-team-grid">
        <article v-for="rescuer in store.rescuers" :key="rescuer.id" class="operator-card">
          <header>
            <div>
              <strong>{{ rescuer.name }}</strong>
              <span>{{ rescuer.id }} / {{ rescuer.team }}</span>
            </div>
            <span class="status-pill" :class="`device-${rescuer.status.toLowerCase()}`">{{ rescuer.status }}</span>
          </header>
          <div class="operator-radar">
            <div class="radar-arrow" :style="{ transform: `rotate(${rescuer.location.azimuth}deg)` }" />
            <span>{{ rescuer.location.azimuth.toFixed(0) }}°</span>
          </div>
          <dl>
            <div>
              <dt>设备</dt>
              <dd>{{ rescuer.deviceId }}</dd>
            </div>
            <div>
              <dt>电量</dt>
              <dd>{{ rescuer.battery }}%</dd>
            </div>
            <div>
              <dt>任务</dt>
              <dd>{{ rescuer.currentTaskId || '待命' }}</dd>
            </div>
            <div>
              <dt>目标</dt>
              <dd>{{ rescuer.targetVictimId || '--' }}</dd>
            </div>
          </dl>
        </article>
      </div>
    </PanelShell>

    <PanelShell title="环境安全矩阵" subtitle="温湿度、可燃气体、CO、氧含量">
      <div class="env-list">
        <article v-for="rescuer in store.rescuers" :key="rescuer.id" class="env-card">
          <div>
            <strong>{{ rescuer.name }}</strong>
            <RiskBadge :level="rescuer.environment.riskLevel" compact />
          </div>
          <div class="env-meter">
            <span>温度</span>
            <meter min="0" max="70" :value="rescuer.environment.temperature" />
            <b>{{ rescuer.environment.temperature }}℃</b>
          </div>
          <div class="env-meter">
            <span>可燃气</span>
            <meter min="0" max="30" :value="rescuer.environment.combustibleGas" />
            <b>{{ rescuer.environment.combustibleGas }}{{ rescuer.environment.combustibleUnit }}</b>
          </div>
          <div class="env-meter">
            <span>CO</span>
            <meter min="0" max="100" :value="rescuer.environment.co" />
            <b>{{ rescuer.environment.co }} ppm</b>
          </div>
          <div class="env-meter">
            <span>O2</span>
            <meter min="16" max="22" :value="rescuer.environment.oxygen" />
            <b>{{ rescuer.environment.oxygen }}%</b>
          </div>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
