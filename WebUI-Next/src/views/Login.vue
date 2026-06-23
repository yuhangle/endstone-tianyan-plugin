<template>
  <div class="login-page">
    <div class="login-lang-bar">
      <button class="theme-btn" @click="store.toggleTheme()" :title="store.isDark ? 'Switch to Light' : 'Switch to Dark'">
        {{ store.isDark ? '☀️' : '🌙' }}
      </button>
      🌐
      <select v-model="langValue" @change="switchLang" class="lang-select">
        <option v-for="opt in langItems" :key="opt.value" :value="opt.value">{{ opt.label }}</option>
      </select>
    </div>
    <div class="login-card">
      <div class="text-center mb-4">
        <div class="login-logo">📡</div>
        <h1 class="text-2xl font-bold mb-1">Tianyan</h1>
        <p class="text-secondary text-sm">{{ store.t('login_title') }}</p>
      </div>

      <Message v-if="error" severity="error" :life="3000" class="mb-3">{{ error }}</Message>

      <div class="flex flex-column gap-3">
        <InputText
          v-model="secret"
          type="password"
          :placeholder="store.t('enter_secret')"
          class="w-full"
          @keyup.enter="handleLogin"
        />

        <Button :label="store.t('enter_console')" :loading="loading" @click="handleLogin" class="w-full" />
      </div>

      <p class="text-xs text-center text-secondary mt-4">Tianyan Protect Plugin</p>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAppStore } from '@/stores/app'
import { fetchStats } from '@/api/client'

const router = useRouter()
const store = useAppStore()
const secret = ref('')
const loading = ref(false)
const error = ref('')

const langValue = ref(store.currentLang)
const langItems = store.langItems

function switchLang() {
  store.setLanguage(langValue.value as 'en_US' | 'zh_CN' | 'ru_RU')
}

async function handleLogin() {
  if (!secret.value) return
  error.value = ''
  loading.value = true
  try {
    store.login(secret.value)
    await fetchStats()
    router.push('/dashboard')
  } catch {
    store.logout()
    error.value = store.t('auth_failed')
  }
  loading.value = false
}

onMounted(() => {
  // Ensure current lang is synced to the selector
  langValue.value = store.currentLang
})
</script>

<style scoped>
.login-page {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 100vh;
  background: var(--p-surface-ground);
}
.login-lang-bar {
  position: fixed;
  top: 1rem;
  right: 1.5rem;
  display: flex;
  align-items: center;
  gap: 0.4rem;
  z-index: 10;
}
.login-card {
  background: var(--p-surface-card);
  border-radius: 24px;
  padding: 2.5rem;
  width: 100%;
  max-width: 400px;
  margin: 0 1rem;
}
.login-logo {
  font-size: 3rem;
  margin-bottom: 0.5rem;
}
.lang-select {
  background: transparent;
  border: 1px solid var(--p-surface-border);
  border-radius: 8px;
  padding: 0.35rem 0.5rem;
  font-size: 0.875rem;
  color: var(--p-text-color);
  cursor: pointer;
  outline: none;
}
.lang-select option {
  color: #000;
  background: #fff;
}
.theme-btn {
  background: rgba(255,255,255,0.1); border: 1px solid var(--p-surface-border);
  font-size: 1.2rem; cursor: pointer; padding: 0.3rem 0.6rem; border-radius: 8px;
  line-height: 1; transition: background 0.15s;
}
.theme-btn:hover { background: rgba(255,255,255,0.2); }
</style>
