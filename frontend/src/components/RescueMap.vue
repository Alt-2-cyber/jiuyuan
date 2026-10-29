<script setup lang="ts">
import { computed } from 'vue'
import type { RescueMapPayload, RiskLevel, RoutePlan } from '@/types/rescue'

const props = defineProps<{
  payload: RescueMapPayload | null
  routePlan?: RoutePlan | null
}>()

const zoneClass = (risk: RiskLevel) => `map-zone map-zone-${risk.toLowerCase()}`

const routePoints = computed(() => {
  if (!props.payload || !props.routePlan?.waypoints?.length) return ''
  return props.routePlan.waypoints
    .map((point) => `${(point.x / props.payload!.width) * 100},${(point.y / props.payload!.height) * 100}`)
    .join(' ')
})
</script>

<template>
  <div class="rescue-map">
    <div v-if="payload" class="map-canvas">
      <div
        v-for="zone in payload.zones"
        :key="zone.name"
        :class="zoneClass(zone.risk)"
        :style="{
          left: `${(zone.x / payload.width) * 100}%`,
          top: `${(zone.y / payload.height) * 100}%`,
          width: `${(zone.w / payload.width) * 100}%`,
          height: `${(zone.h / payload.height) * 100}%`
        }"
      >
        <span>{{ zone.name }}</span>
      </div>

      <button
        v-for="victim in payload.victims"
        :key="victim.id"
        class="map-node victim-node"
        :class="`node-${victim.level.toLowerCase()}`"
        :style="{
          left: `${(victim.location.x / payload.width) * 100}%`,
          top: `${(victim.location.y / payload.height) * 100}%`
        }"
        :title="`${victim.name} ${victim.priorityScore}`"
      >
        {{ victim.id.replace('V-', '') }}
      </button>

      <button
        v-for="rescuer in payload.rescuers"
        :key="rescuer.id"
        class="map-node rescuer-node"
        :class="`device-${rescuer.status.toLowerCase()}`"
        :style="{
          left: `${(rescuer.location.x / payload.width) * 100}%`,
          top: `${(rescuer.location.y / payload.height) * 100}%`
        }"
        :title="`${rescuer.name} ${rescuer.team}`"
      >
        R
      </button>

      <svg v-if="routePoints" class="route-overlay" viewBox="0 0 100 100" preserveAspectRatio="none">
        <polyline class="route-shadow" :points="routePoints" />
        <polyline class="route-line" :points="routePoints" />
      </svg>

      <div
        v-for="point in routePlan?.waypoints || []"
        :key="`${point.type}-${point.x}-${point.y}`"
        class="route-point"
        :class="`route-${point.type}`"
        :style="{
          left: `${(point.x / payload.width) * 100}%`,
          top: `${(point.y / payload.height) * 100}%`
        }"
        :title="point.label"
      />

      <div class="map-grid" />
    </div>
    <div v-else class="map-empty">地图数据加载中</div>
  </div>
</template>
