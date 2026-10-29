import axios from 'axios'
import type {
  AlertEvent,
  CloudSyncResult,
  CloudVictimSummary,
  Device,
  RescueMapPayload,
  RoutePlan,
  RescueTask,
  Rescuer,
  SiteOverview,
  Victim
} from '@/types/rescue'

const http = axios.create({
  baseURL: '/api',
  timeout: 8000
})

export const rescueApi = {
  overview: async () => (await http.get<SiteOverview>('/overview')).data,
  map: async () => (await http.get<RescueMapPayload>('/map')).data,
  victims: async () => (await http.get<Victim[]>('/victims')).data,
  victim: async (id: string) => (await http.get<Victim>(`/victims/${id}`)).data,
  rescuers: async () => (await http.get<Rescuer[]>('/rescuers')).data,
  devices: async () => (await http.get<Device[]>('/devices')).data,
  alerts: async () => (await http.get<AlertEvent[]>('/alerts')).data,
  tasks: async () => (await http.get<RescueTask[]>('/tasks')).data,
  assignTask: async (taskId: string, rescuerId: string) =>
    (await http.post<RescueTask>(`/tasks/${taskId}/assign`, { rescuerId })).data,
  routePlan: async (victimId?: string | null, rescuerId?: string | null) =>
    (await http.post<RoutePlan>('/route-plan', { victimId, rescuerId })).data,
  cloudVictimSamples: async (limit = 100) =>
    (await http.get<CloudVictimSummary>('/cloud-victim-samples', { params: { limit } })).data,
  syncCloudVictimSamples: async () =>
    (await http.post<CloudSyncResult>('/cloud-victim-samples/sync')).data
}
