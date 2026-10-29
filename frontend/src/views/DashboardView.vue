<script setup lang="ts">
import { computed } from 'vue'
import MetricTile from '@/components/MetricTile.vue'
import PanelShell from '@/components/PanelShell.vue'
import RescueMap from '@/components/RescueMap.vue'
import RiskBadge from '@/components/RiskBadge.vue'
import TrendChart from '@/components/TrendChart.vue'
import VitalStrip from '@/components/VitalStrip.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()
const overview = computed(() => store.overview)
const realVictim = computed(() => store.realVictims[0] || null)
const routePlan = computed(() => store.routePlan)

const formatVital = (value: number, suffix: string) => (value > 0 ? `${value}${suffix}` : '未采集')
</script>

<template>
  <div class="page-grid dashboard-grid">
    <section class="metrics-row">
      <MetricTile v-for="metric in overview?.metrics || []" :key="metric.label" :metric="metric" />
    </section>

    <PanelShell title="现场态势图" subtitle="地下商场 A 区 / 真实上报优先显示，路线由算法生成" class="map-panel">
      <RescueMap :payload="store.map" :route-plan="routePlan" />
    </PanelShell>

    <PanelShell title="救援优先级" subtitle="按生命体征、位置、环境风险综合排序" class="priority-panel">
      <div class="priority-list">
        <VitalStrip v-for="victim in overview?.priorityQueue || []" :key="victim.id" :victim="victim" />
      </div>
    </PanelShell>

    <PanelShell title="风险趋势" subtitle="最近 60 分钟最低血氧、最高心率、可燃气体变化" class="trend-panel">
      <TrendChart :trends="overview?.trends || []" />
    </PanelShell>

    <PanelShell title="真实数据锚点" subtitle="来自华为云同步到 MySQL 的受灾端最新样本" class="ingest-mini-panel">
      <article v-if="realVictim" class="real-sample-card">
        <div class="real-sample-head">
          <div>
            <strong>{{ realVictim.name }}</strong>
            <span>{{ realVictim.deviceId }}</span>
          </div>
          <RiskBadge :level="realVictim.level" compact />
        </div>
        <dl>
          <div>
            <dt>血氧</dt>
            <dd>{{ formatVital(realVictim.vitals.spo2, '%') }}</dd>
          </div>
          <div>
            <dt>心率</dt>
            <dd>{{ formatVital(realVictim.vitals.heartRate, ' bpm') }}</dd>
          </div>
          <div>
            <dt>救援端距离</dt>
            <dd>{{ realVictim.location.distanceToNearestRescuer.toFixed(1) }} m</dd>
          </div>
          <div>
            <dt>更新时间</dt>
            <dd>{{ realVictim.lastSeenAt }}</dd>
          </div>
        </dl>
      </article>
      <div v-else class="real-empty">
        暂无真实云端样本。请先在“华为云数据”页面同步，或等待设备上报。
      </div>
    </PanelShell>

    <PanelShell title="救援路径规划" subtitle="A* 网格路径，避开高风险区域并优先真实目标" class="route-panel">
      <article v-if="routePlan" class="route-plan-card">
        <header>
          <div>
            <strong>{{ routePlan.rescuerName || routePlan.rescuerId }} → {{ routePlan.victimName || routePlan.victimId }}</strong>
            <span>{{ routePlan.realVictimData ? '目标为真实设备数据' : '目标为演示背景数据' }}</span>
          </div>
          <RiskBadge :level="routePlan.priority" compact />
        </header>
        <p>{{ routePlan.strategy }}</p>
        <div class="route-kpis">
          <div>
            <span>距离</span>
            <b>{{ routePlan.totalDistance.toFixed(1) }}m</b>
          </div>
          <div>
            <span>预计</span>
            <b>{{ routePlan.estimatedMinutes }} 分钟</b>
          </div>
          <div>
            <span>路径点</span>
            <b>{{ routePlan.waypoints.length }}</b>
          </div>
        </div>
        <ul>
          <li v-for="warning in routePlan.warnings" :key="warning">{{ warning }}</li>
        </ul>
      </article>
    </PanelShell>

    <PanelShell title="活跃告警" subtitle="待处置或已确认的关键事件" class="alerts-panel">
      <div class="alert-stack">
        <article v-for="alert in overview?.latestAlerts || []" :key="alert.id" class="alert-row">
          <RiskBadge :level="alert.severity" compact />
          <div>
            <strong>{{ alert.title }}</strong>
            <p>{{ alert.detail }}</p>
          </div>
          <span>{{ alert.status }}</span>
        </article>
      </div>
    </PanelShell>

    <PanelShell title="救援员安全状态" subtitle="环境传感与任务状态" class="rescuers-panel">
      <div class="rescuer-cards">
        <article v-for="rescuer in overview?.activeRescuers || []" :key="rescuer.id" class="rescuer-card">
          <div>
            <strong>{{ rescuer.name }}</strong>
            <span>{{ rescuer.team }} / {{ rescuer.deviceId }}</span>
          </div>
          <RiskBadge :level="rescuer.environment.riskLevel" compact />
          <dl>
            <div>
              <dt>温度</dt>
              <dd>{{ rescuer.environment.temperature }}℃</dd>
            </div>
            <div>
              <dt>可燃气</dt>
              <dd>{{ rescuer.environment.combustibleGas }}{{ rescuer.environment.combustibleUnit }}</dd>
            </div>
            <div>
              <dt>任务</dt>
              <dd>{{ rescuer.currentTaskId || '待命' }}</dd>
            </div>
          </dl>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
