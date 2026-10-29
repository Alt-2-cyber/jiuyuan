<script setup lang="ts">
import PanelShell from '@/components/PanelShell.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()
</script>

<template>
  <div class="page-stack">
    <PanelShell title="星闪设备管理" subtitle="腕带、救援终端、网关、定位锚点">
      <div class="device-grid">
        <article v-for="device in store.devices" :key="device.id" class="device-card">
          <header>
            <div>
              <strong>{{ device.name }}</strong>
              <span>{{ device.id }} / {{ device.type }}</span>
            </div>
            <span class="status-pill" :class="`device-${device.status.toLowerCase()}`">{{ device.status }}</span>
          </header>
          <div class="battery-ring" :style="{ '--battery': `${device.battery}%` }">
            <strong>{{ device.battery }}%</strong>
            <span>BAT</span>
          </div>
          <dl>
            <div>
              <dt>绑定对象</dt>
              <dd>{{ device.ownerId }}</dd>
            </div>
            <div>
              <dt>固件</dt>
              <dd>{{ device.firmwareVersion }}</dd>
            </div>
            <div>
              <dt>链路</dt>
              <dd>{{ device.linkQuality }}</dd>
            </div>
            <div>
              <dt>最近在线</dt>
              <dd>{{ device.lastSeenAt }}</dd>
            </div>
          </dl>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
