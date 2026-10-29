<script setup lang="ts">
const thresholds = [
  { label: '红色血氧阈值', value: '< 90%', scope: '受灾人员腕带' },
  { label: '危险可燃气体', value: '>= 20% LEL', scope: '救援终端' },
  { label: '高温撤离阈值', value: '>= 60℃', scope: '救援终端' },
  { label: '设备离线告警', value: '> 120s', scope: '全设备' },
  { label: '定位低置信度', value: '> 5m', scope: '定位融合' }
]

const roles = [
  { name: 'COMMANDER', desc: '查看全局态势、派发任务、调整优先级' },
  { name: 'RESCUER', desc: '查看自身任务、目标导航和环境风险' },
  { name: 'MEDICAL', desc: '查看生命体征并标注医疗风险' },
  { name: 'ADMIN', desc: '管理设备、阈值、用户和现场配置' }
]
</script>

<template>
  <div class="page-grid settings-grid">
    <section class="panel-shell">
      <header class="panel-header">
        <div>
          <h2>告警阈值配置</h2>
          <p>当前为演练参数，工程版应按现场预案和设备标定调整</p>
        </div>
      </header>
      <div class="config-list">
        <article v-for="item in thresholds" :key="item.label">
          <div>
            <strong>{{ item.label }}</strong>
            <span>{{ item.scope }}</span>
          </div>
          <button>{{ item.value }}</button>
        </article>
      </div>
    </section>

    <section class="panel-shell">
      <header class="panel-header">
        <div>
          <h2>权限角色</h2>
          <p>管理员端预置角色模型</p>
        </div>
      </header>
      <div class="role-list">
        <article v-for="role in roles" :key="role.name">
          <strong>{{ role.name }}</strong>
          <p>{{ role.desc }}</p>
        </article>
      </div>
    </section>

    <section class="panel-shell wide-panel">
      <header class="panel-header">
        <div>
          <h2>接入配置</h2>
          <p>网关、MQTT、星闪设备和后台链路规划</p>
        </div>
      </header>
      <div class="integration-grid">
        <article>
          <span>MQTT Broker</span>
          <strong>mqtt://gateway.local:1883</strong>
        </article>
        <article>
          <span>现场网关</span>
          <strong>GW-001 / 在线</strong>
        </article>
        <article>
          <span>WebSocket</span>
          <strong>/ws/rescue</strong>
        </article>
        <article>
          <span>数据库</span>
          <strong>MySQL 8 / rescue</strong>
        </article>
      </div>
    </section>
  </div>
</template>
