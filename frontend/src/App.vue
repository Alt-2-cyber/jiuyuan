<script setup lang="ts">
import {
  Activity,
  BellRing,
  ClipboardList,
  Cloud,
  Cpu,
  Database,
  Gauge,
  Home,
  Radar,
  Settings,
  ShieldAlert
} from 'lucide-vue-next'
import { computed, onMounted } from 'vue'
import { RouterLink, RouterView, useRoute } from 'vue-router'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()
const route = useRoute()

const navItems = [
  { path: '/dashboard', label: '态势总览', icon: Home },
  { path: '/victims', label: '受灾人员', icon: Activity },
  { path: '/rescuers', label: '救援人员', icon: ShieldAlert },
  { path: '/tasks', label: '任务调度', icon: ClipboardList },
  { path: '/alerts', label: '告警中心', icon: BellRing },
  { path: '/devices', label: '设备管理', icon: Cpu },
  { path: '/ingest', label: '数据接入', icon: Cloud },
  { path: '/cloud-data', label: '华为云数据', icon: Database },
  { path: '/analytics', label: '数据分析', icon: Gauge },
  { path: '/settings', label: '系统配置', icon: Settings }
]

const pageTitle = computed(() => String(route.meta.title || '态势总览'))
const redCount = computed(() => store.redVictims.length)
const alertCount = computed(() => store.activeAlerts.length)

onMounted(() => {
  store.hydrate()
  window.setInterval(() => {
    store.hydrate()
  }, 30000)
})
</script>

<template>
  <div class="app-frame">
    <aside class="sidebar">
      <div class="brand-lockup">
        <div class="brand-mark">
          <Radar :size="24" />
        </div>
        <div>
          <strong>星闪救援</strong>
          <span>NearLink Rescue</span>
        </div>
      </div>

      <nav class="main-nav">
        <RouterLink v-for="item in navItems" :key="item.path" :to="item.path" class="nav-link">
          <component :is="item.icon" :size="18" />
          <span>{{ item.label }}</span>
        </RouterLink>
      </nav>

      <div class="sidebar-brief">
        <span>现场阶段</span>
        <strong>{{ store.overview?.phase || '数据同步中' }}</strong>
        <p>网关 GW-001 在线，指挥链路稳定。</p>
      </div>
    </aside>

    <main class="main-shell">
      <header class="topbar">
        <div>
          <span class="eyebrow">STARFLASH EMERGENCY COMMAND</span>
          <h1>{{ pageTitle }}</h1>
        </div>
        <div class="topbar-status">
          <div>
            <span>红色高危</span>
            <strong>{{ redCount }}</strong>
          </div>
          <div>
            <span>活跃告警</span>
            <strong>{{ alertCount }}</strong>
          </div>
          <button class="refresh-button" @click="store.hydrate">刷新态势</button>
        </div>
      </header>

      <RouterView />
    </main>
  </div>
</template>

