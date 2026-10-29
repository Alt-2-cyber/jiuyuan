<script setup lang="ts">
import * as echarts from 'echarts'
import { onBeforeUnmount, onMounted, ref, watch } from 'vue'
import type { TrendPoint } from '@/types/rescue'

const props = defineProps<{
  trends: TrendPoint[]
}>()

const chartEl = ref<HTMLDivElement | null>(null)
let chart: echarts.ECharts | null = null

const render = () => {
  if (!chartEl.value) return
  if (!chart) chart = echarts.init(chartEl.value)

  chart.setOption({
    backgroundColor: 'transparent',
    grid: { left: 34, right: 16, top: 28, bottom: 30 },
    tooltip: { trigger: 'axis', backgroundColor: '#101820', borderColor: '#2a3d4c', textStyle: { color: '#dbe9ef' } },
    legend: { top: 0, right: 10, textStyle: { color: '#9ab0bd' } },
    xAxis: {
      type: 'category',
      data: props.trends.map((item) => item.time),
      axisLine: { lineStyle: { color: '#2b4350' } },
      axisLabel: { color: '#8297a4' }
    },
    yAxis: [
      {
        type: 'value',
        min: 70,
        max: 160,
        splitLine: { lineStyle: { color: 'rgba(98, 129, 144, .16)' } },
        axisLabel: { color: '#8297a4' }
      },
      {
        type: 'value',
        min: 0,
        max: 30,
        splitLine: { show: false },
        axisLabel: { color: '#8297a4' }
      }
    ],
    series: [
      {
        name: '最低血氧',
        type: 'line',
        smooth: true,
        data: props.trends.map((item) => item.spo2),
        symbol: 'circle',
        symbolSize: 6,
        lineStyle: { color: '#61d394', width: 3 },
        itemStyle: { color: '#61d394' },
        areaStyle: { color: 'rgba(97, 211, 148, .12)' }
      },
      {
        name: '最高心率',
        type: 'line',
        smooth: true,
        data: props.trends.map((item) => item.heartRate),
        symbol: 'circle',
        symbolSize: 6,
        lineStyle: { color: '#ffb84d', width: 3 },
        itemStyle: { color: '#ffb84d' }
      },
      {
        name: '可燃气体',
        type: 'bar',
        yAxisIndex: 1,
        data: props.trends.map((item) => item.gas),
        barWidth: 10,
        itemStyle: { color: '#ff5a5f', borderRadius: [3, 3, 0, 0] }
      }
    ]
  })
}

const resize = () => chart?.resize()

onMounted(() => {
  render()
  window.addEventListener('resize', resize)
})

watch(() => props.trends, render, { deep: true })

onBeforeUnmount(() => {
  window.removeEventListener('resize', resize)
  chart?.dispose()
})
</script>

<template>
  <div ref="chartEl" class="trend-chart" />
</template>
