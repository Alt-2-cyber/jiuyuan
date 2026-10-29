<script setup lang="ts">
import type { MetricCard } from '@/types/rescue'
import RiskBadge from './RiskBadge.vue'

defineProps<{
  metric: MetricCard
}>()
</script>

<template>
  <article class="metric-tile">
    <div class="metric-topline">
      <span>{{ metric.label }}</span>
      <RiskBadge :level="metric.level" compact />
    </div>
    <div class="metric-value">
      {{ metric.value }}<small>{{ metric.unit }}</small>
    </div>
    <p>{{ metric.trend }}</p>
  </article>
</template>
