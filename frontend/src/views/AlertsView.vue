<script setup lang="ts">
import PanelShell from '@/components/PanelShell.vue'
import RiskBadge from '@/components/RiskBadge.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()
</script>

<template>
  <div class="page-stack">
    <PanelShell title="告警中心" subtitle="生命体征、环境、设备、定位异常统一处置">
      <div class="alert-timeline">
        <article v-for="alert in store.alerts" :key="alert.id" class="timeline-item">
          <div class="timeline-dot" :class="`dot-${alert.severity.toLowerCase()}`" />
          <div class="timeline-body">
            <header>
              <div>
                <strong>{{ alert.title }}</strong>
                <span>{{ alert.id }} / {{ alert.type }}</span>
              </div>
              <RiskBadge :level="alert.severity" compact />
            </header>
            <p>{{ alert.detail }}</p>
            <footer>
              <span>人员 {{ alert.personId }}</span>
              <span>设备 {{ alert.deviceId }}</span>
              <span>状态 {{ alert.status }}</span>
              <span>最近 {{ alert.lastSeenAt }}</span>
            </footer>
          </div>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
