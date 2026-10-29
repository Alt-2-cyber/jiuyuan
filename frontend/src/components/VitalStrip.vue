<script setup lang="ts">
import type { Victim } from '@/types/rescue'
import RiskBadge from './RiskBadge.vue'

defineProps<{
  victim: Victim
}>()
</script>

<template>
  <article class="vital-strip">
    <div class="strip-id">
      <strong>{{ victim.id }}</strong>
      <span>{{ victim.name }}</span>
    </div>
    <RiskBadge :level="victim.level" compact />
    <div class="strip-vitals">
      <span>SpO2 <b>{{ victim.vitals.spo2 || '--' }}</b></span>
      <span>HR <b>{{ victim.vitals.heartRate || '--' }}</b></span>
      <span>BP <b>{{ victim.vitals.systolic || '--' }}/{{ victim.vitals.diastolic || '--' }}</b></span>
    </div>
    <div class="strip-score">
      <small>优先级</small>
      <b>{{ victim.priorityScore.toFixed(1) }}</b>
    </div>
  </article>
</template>
