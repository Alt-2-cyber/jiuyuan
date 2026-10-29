<script setup lang="ts">
import type { RiskLevel } from '@/types/rescue'

defineProps<{
  level: RiskLevel
  compact?: boolean
}>()

const labels: Record<RiskLevel, string> = {
  RED: '红色',
  ORANGE: '橙色',
  YELLOW: '黄色',
  GREEN: '绿色',
  GRAY: '待确认'
}
</script>

<template>
  <span class="risk-badge" :class="[`risk-${level.toLowerCase()}`, { compact }]">
    {{ labels[level] }}
  </span>
</template>
