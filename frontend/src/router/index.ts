import { createRouter, createWebHistory } from 'vue-router'
import DashboardView from '@/views/DashboardView.vue'
import VictimsView from '@/views/VictimsView.vue'
import RescuersView from '@/views/RescuersView.vue'
import TasksView from '@/views/TasksView.vue'
import AlertsView from '@/views/AlertsView.vue'
import DevicesView from '@/views/DevicesView.vue'
import IngestView from '@/views/IngestView.vue'
import CloudDataView from '@/views/CloudDataView.vue'
import AnalyticsView from '@/views/AnalyticsView.vue'
import SettingsView from '@/views/SettingsView.vue'

export const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', redirect: '/dashboard' },
    { path: '/cloud-data', component: CloudDataView, meta: { title: '\u534e\u4e3a\u4e91\u6570\u636e' } },
    { path: '/dashboard', component: DashboardView, meta: { title: '态势总览' } },
    { path: '/victims', component: VictimsView, meta: { title: '受灾人员' } },
    { path: '/rescuers', component: RescuersView, meta: { title: '救援人员' } },
    { path: '/tasks', component: TasksView, meta: { title: '任务调度' } },
    { path: '/alerts', component: AlertsView, meta: { title: '告警中心' } },
    { path: '/devices', component: DevicesView, meta: { title: '设备管理' } },
    { path: '/ingest', component: IngestView, meta: { title: '数据接入' } },
    { path: '/analytics', component: AnalyticsView, meta: { title: '数据分析' } },
    { path: '/settings', component: SettingsView, meta: { title: '系统配置' } }
  ]
})
