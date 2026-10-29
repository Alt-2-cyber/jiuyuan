<script setup lang="ts">
defineProps<{
  title: string
  subtitle?: string
  dense?: boolean
}>()
</script>

<template>
  <section class="panel-shell" :class="{ dense }">
    <header class="panel-header">
      <div>
        <h2>{{ title }}</h2>
        <p v-if="subtitle">{{ subtitle }}</p>
      </div>
      <slot name="action" />
    </header>
    <slot />
  </section>
</template>
