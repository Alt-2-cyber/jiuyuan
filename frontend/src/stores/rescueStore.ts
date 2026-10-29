import { defineStore } from 'pinia'
import { rescueApi } from '@/api/rescue'
import type {
  AlertEvent,
  Device,
  RescueMapPayload,
  RoutePlan,
  RescueTask,
  Rescuer,
  SiteOverview,
  Victim
} from '@/types/rescue'

interface RescueState {
  loading: boolean
  overview: SiteOverview | null
  map: RescueMapPayload | null
  victims: Victim[]
  rescuers: Rescuer[]
  devices: Device[]
  alerts: AlertEvent[]
  tasks: RescueTask[]
  routePlan: RoutePlan | null
}

export const useRescueStore = defineStore('rescue', {
  state: (): RescueState => ({
    loading: false,
    overview: null,
    map: null,
    victims: [],
    rescuers: [],
    devices: [],
    alerts: [],
    tasks: [],
    routePlan: null
  }),
  getters: {
    redVictims: (state) => state.victims.filter((item) => item.level === 'RED'),
    activeAlerts: (state) => state.alerts.filter((item) => item.status !== 'RESOLVED'),
    pendingTasks: (state) => state.tasks.filter((item) => item.status === 'PENDING'),
    realVictims: (state) => state.victims.filter((item) => item.status.includes('真实') || item.id.includes('REAL'))
  },
  actions: {
    async hydrate() {
      this.loading = true
      try {
        const [overview, map, victims, rescuers, devices, alerts, tasks, routePlan] = await Promise.all([
          rescueApi.overview(),
          rescueApi.map(),
          rescueApi.victims(),
          rescueApi.rescuers(),
          rescueApi.devices(),
          rescueApi.alerts(),
          rescueApi.tasks(),
          rescueApi.routePlan()
        ])
        this.overview = overview
        this.map = map
        this.victims = victims
        this.rescuers = rescuers
        this.devices = devices
        this.alerts = alerts
        this.tasks = tasks
        this.routePlan = routePlan
      } finally {
        this.loading = false
      }
    },
    async assignTask(taskId: string, rescuerId: string) {
      await rescueApi.assignTask(taskId, rescuerId)
      await this.hydrate()
    },
    async planRoute(victimId?: string | null, rescuerId?: string | null) {
      this.routePlan = await rescueApi.routePlan(victimId, rescuerId)
    }
  }
})
