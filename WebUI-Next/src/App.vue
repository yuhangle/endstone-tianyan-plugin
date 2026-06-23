<template>
  <div :class="{ dark: store.isDark }" style="height: 100%;">
    <router-view />
  </div>
</template>

<script setup lang="ts">
import { onMounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useAppStore } from '@/stores/app'
import { isAuthenticated } from '@/api/client'

const router = useRouter()
const store = useAppStore()

watch(() => store.isDark, (val) => {
  document.documentElement.classList.toggle('dark', val)
})

router.beforeEach((to, _from, next) => {
  if (to.name === 'Login') next()
  else if (!isAuthenticated()) next({ name: 'Login' })
  else next()
})

onMounted(() => {
  document.documentElement.classList.toggle('dark', store.isDark)
  window.addEventListener('auth:required', () => {
    store.logout()
    router.push('/login')
  })
})
</script>

<style>
html, body { margin: 0; padding: 0; height: 100%; }
body { font-family: var(--font-family); background: var(--p-surface-ground); }
#app { height: 100%; }
</style>
