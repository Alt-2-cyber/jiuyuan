export type RiskLevel = 'RED' | 'ORANGE' | 'YELLOW' | 'GREEN' | 'GRAY'
export type DeviceStatus = 'ONLINE' | 'WARNING' | 'OFFLINE'
export type TaskStatus = 'PENDING' | 'ASSIGNED' | 'IN_PROGRESS' | 'COMPLETED' | 'PAUSED'

export interface VitalSigns {
  spo2: number
  heartRate: number
  systolic: number
  diastolic: number
  temperature: number
  motionState: string
  signalQuality: number
  battery: number
}

export interface LocationPoint {
  x: number
  y: number
  z: number
  distanceToNearestRescuer: number
  accuracy: number
  azimuth: number
}

export interface EnvironmentSample {
  temperature: number
  humidity: number
  combustibleGas: number
  combustibleUnit: string
  co: number
  oxygen: number
  riskLevel: RiskLevel
}

export interface Victim {
  id: string
  name: string
  age: number
  gender: string
  deviceId: string
  level: RiskLevel
  priorityScore: number
  vitals: VitalSigns
  location: LocationPoint
  assignedRescuerId: string | null
  taskId: string | null
  status: string
  reasons: string[]
  lastSeenAt: string
}

export interface Rescuer {
  id: string
  name: string
  team: string
  deviceId: string
  status: DeviceStatus
  location: LocationPoint
  environment: EnvironmentSample
  currentTaskId: string | null
  targetVictimId: string | null
  battery: number
  lastSeenAt: string
}

export interface Device {
  id: string
  name: string
  type: string
  ownerId: string
  status: DeviceStatus
  battery: number
  firmwareVersion: string
  linkQuality: string
  lastSeenAt: string
}

export interface AlertEvent {
  id: string
  type: string
  severity: RiskLevel
  title: string
  detail: string
  personId: string
  deviceId: string
  status: string
  firstSeenAt: string
  lastSeenAt: string
}

export interface RescueTask {
  id: string
  victimId: string
  rescuerId: string | null
  status: TaskStatus
  priority: RiskLevel
  priorityScore: number
  reason: string
  createdAt: string
  assignedAt: string | null
  completedAt: string | null
}

export interface TrendPoint {
  time: string
  spo2: number
  heartRate: number
  gas: number
  alerts: number
}

export interface MetricCard {
  label: string
  value: string
  unit: string
  trend: string
  level: RiskLevel
}

export interface SiteOverview {
  siteId: string
  siteName: string
  phase: string
  serverTime: string
  metrics: MetricCard[]
  riskDistribution: Record<RiskLevel, number>
  trends: TrendPoint[]
  priorityQueue: Victim[]
  latestAlerts: AlertEvent[]
  activeRescuers: Rescuer[]
}

export interface MapZone {
  name: string
  x: number
  y: number
  w: number
  h: number
  risk: RiskLevel
}

export interface RescueMapPayload {
  siteId: string
  name: string
  width: number
  height: number
  zones: MapZone[]
  victims: Victim[]
  rescuers: Rescuer[]
}

export interface RouteWaypoint {
  x: number
  y: number
  label: string
  type: 'start' | 'target' | 'waypoint' | string
  risk: RiskLevel
}

export interface RoutePlan {
  id: string
  rescuerId: string
  rescuerName: string
  victimId: string
  victimName: string
  realVictimData: boolean
  totalDistance: number
  estimatedMinutes: number
  priority: RiskLevel
  strategy: string
  warnings: string[]
  waypoints: RouteWaypoint[]
  generatedAt: string
}

export interface CloudVictimSample {
  id: number
  deviceId: string
  serviceId: string
  eventTime: string
  seq: number | null
  spo2: number | null
  heartRate: number | null
  systolic: number | null
  battery: number | null
  sos: boolean | null
  source: string
  raw: Record<string, unknown>
  createdAt: string
}

export interface CloudVictimSummary {
  total: number
  latest: CloudVictimSample | null
  items: CloudVictimSample[]
  cloudEnabled: boolean
  configuredDeviceId: string
  serviceId: string
}

export interface CloudSyncResult {
  success: boolean
  message: string
  inserted: number
  latest: CloudVictimSample | null
}
