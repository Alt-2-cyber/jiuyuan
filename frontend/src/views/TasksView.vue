<script setup lang="ts">
import { computed, ref } from 'vue'
import PanelShell from '@/components/PanelShell.vue'
import RiskBadge from '@/components/RiskBadge.vue'
import { useRescueStore } from '@/stores/rescueStore'

const store = useRescueStore()
const selectedRescuers = ref<Record<string, string>>({})
const selectedVictimId = ref<string>('')
const selectedRescuerId = ref<string>('')

const availableRescuers = computed(() => store.rescuers)
const routePlan = computed(() => store.routePlan)

const victimName = (id: string) => store.victims.find((item) => item.id === id)?.name || id
const rescuerName = (id: string | null) => store.rescuers.find((item) => item.id === id)?.name || '未派发'

const assign = async (taskId: string) => {
  const rescuerId = selectedRescuers.value[taskId]
  if (!rescuerId) return
  await store.assignTask(taskId, rescuerId)
}

const plan = async () => {
  await store.planRoute(selectedVictimId.value || null, selectedRescuerId.value || null)
}
</script>

<template>
  <div class="page-stack">
    <PanelShell title="路径规划" subtitle="使用当前受灾端、救援端和风险区生成可视化路线">
      <div class="route-control">
        <select v-model="selectedVictimId">
          <option value="">自动选择最高优先级目标</option>
          <option v-for="victim in store.victims" :key="victim.id" :value="victim.id">
            {{ victim.id }} / {{ victim.name }} / {{ victim.status }}
          </option>
        </select>
        <select v-model="selectedRescuerId">
          <option value="">自动选择最近可用救援端</option>
          <option v-for="rescuer in availableRescuers" :key="rescuer.id" :value="rescuer.id">
            {{ rescuer.id }} / {{ rescuer.name }} / {{ rescuer.deviceId }}
          </option>
        </select>
        <button class="primary-button" @click="plan">重新规划</button>
      </div>

      <article v-if="routePlan" class="route-plan-card task-route-card">
        <header>
          <div>
            <strong>{{ routePlan.rescuerName || routePlan.rescuerId }} → {{ routePlan.victimName || routePlan.victimId }}</strong>
            <span>{{ routePlan.realVictimData ? '真实受灾端数据' : '演示背景目标' }}</span>
          </div>
          <RiskBadge :level="routePlan.priority" compact />
        </header>
        <p>{{ routePlan.strategy }}</p>
        <div class="route-kpis">
          <div>
            <span>规划距离</span>
            <b>{{ routePlan.totalDistance.toFixed(1) }}m</b>
          </div>
          <div>
            <span>预计耗时</span>
            <b>{{ routePlan.estimatedMinutes }} 分钟</b>
          </div>
          <div>
            <span>目标编号</span>
            <b>{{ routePlan.victimId || '--' }}</b>
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

    <PanelShell title="任务调度台" subtitle="真实样本任务置顶，演示任务仅作调度界面展示">
      <div class="task-board">
        <article v-for="task in store.tasks" :key="task.id" class="task-card">
          <header>
            <div>
              <strong>{{ task.id }}</strong>
              <span>{{ victimName(task.victimId) }} / {{ task.victimId }}</span>
            </div>
            <RiskBadge :level="task.priority" compact />
          </header>
          <p>{{ task.reason }}</p>
          <dl>
            <div>
              <dt>状态</dt>
              <dd>{{ task.status }}</dd>
            </div>
            <div>
              <dt>评分</dt>
              <dd>{{ task.priorityScore.toFixed(1) }}</dd>
            </div>
            <div>
              <dt>救援员</dt>
              <dd>{{ rescuerName(task.rescuerId) }}</dd>
            </div>
          </dl>
          <div class="task-actions">
            <select v-model="selectedRescuers[task.id]">
              <option value="">选择救援员</option>
              <option v-for="rescuer in availableRescuers" :key="rescuer.id" :value="rescuer.id">
                {{ rescuer.name }} / {{ rescuer.team }}
              </option>
            </select>
            <button @click="assign(task.id)">派发</button>
          </div>
        </article>
      </div>
    </PanelShell>
  </div>
</template>
