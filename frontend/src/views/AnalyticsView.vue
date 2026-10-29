<script setup lang="ts">
import { computed } from 'vue'
import PanelShell from '@/components/PanelShell.vue'
import TrendChart from '@/components/TrendChart.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()

const stats = computed(() => {
  const victims = store.victims
  const avgSpo2 =
    victims.filter((item) => item.vitals.spo2 > 0).reduce((sum, item) => sum + item.vitals.spo2, 0) /
    Math.max(1, victims.filter((item) => item.vitals.spo2 > 0).length)
  const avgDistance =
    victims.reduce((sum, item) => sum + item.location.distanceToNearestRescuer, 0) / Math.max(1, victims.length)
  return [
    { label: '平均血氧', value: `${avgSpo2.toFixed(1)}%` },
    { label: '平均救援距离', value: `${avgDistance.toFixed(1)}m` },
    { label: '待派发任务', value: `${store.pendingTasks.length}` },
    { label: '高危环境点', value: `${store.rescuers.filter((item) => item.environment.riskLevel === 'RED').length}` }
  ]
})
</script>

<template>
  <div class="page-grid analytics-grid">
    <PanelShell title="态势分析" subtitle="救援优先级和现场环境趋势" class="wide-panel">
      <div class="analysis-kpis">
        <article v-for="item in stats" :key="item.label">
          <span>{{ item.label }}</span>
          <strong>{{ item.value }}</strong>
        </article>
      </div>
      <TrendChart :trends="store.overview?.trends || []" />
    </PanelShell>

    <PanelShell title="优先级解释" subtitle="当前评分模型拆解">
      <div class="model-list">
        <article>
          <strong>生命风险 45%</strong>
          <p>血氧、心率、血压趋势、体温异常。</p>
        </article>
        <article>
          <strong>失联与运动 20%</strong>
          <p>SOS、跌倒、长时间静止、生命体征过期。</p>
        </article>
        <article>
          <strong>位置可达 15%</strong>
          <p>最近救援员距离、定位质量、误差圈。</p>
        </article>
        <article>
          <strong>环境风险 10%</strong>
          <p>可燃气体、高温、CO、缺氧。</p>
        </article>
        <article>
          <strong>SOS 加权 10%</strong>
          <p>主动求救和重复触发提高排序。</p>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
