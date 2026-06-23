<template>
  <div class="layout">
    <aside class="sidebar" :class="{ collapsed }">
      <div class="sidebar-header" @click="collapsed = !collapsed">
        <span class="logo">📡</span>
        <span v-if="!collapsed" class="font-bold text-lg">Tianyan</span>
      </div>
      <hr class="sidebar-divider" />

      <div class="nav-list">
        <button class="nav-btn" :class="{ active: route.name === 'Dashboard' }" @click="router.push('/dashboard')">
          <span class="nav-icon">📊</span>
          <span v-if="!collapsed">Dashboard</span>
        </button>
        <button class="nav-btn" :class="{ active: route.name === 'DbInfo' }" @click="router.push('/db-info')">
          <span class="nav-icon">🗄️</span>
          <span v-if="!collapsed">{{ store.t('db_info') }}</span>
        </button>
      </div>

      <div class="sidebar-footer">
        <hr class="sidebar-divider" />
        <button class="nav-btn" @click="store.logout(); router.push('/login')">
          <span class="nav-icon">🚪</span>
          <span v-if="!collapsed">{{ store.t('logout') }}</span>
        </button>
      </div>
    </aside>

    <div class="main-area">
      <div class="topbar">
        <div class="flex align-items-center gap-2">
          <button class="topbar-btn" @click="collapsed = !collapsed">☰</button>
          <span class="font-semibold text-lg">{{ store.t('console_title') }}</span>
        </div>
        <div class="flex align-items-center gap-2">
          <button class="theme-btn" @click="store.toggleTheme()" :title="store.isDark ? 'Switch to Light' : 'Switch to Dark'">
            {{ store.isDark ? '☀️' : '🌙' }}
          </button>
          🌐
          <select v-model="langValue" @change="switchLang" class="lang-select">
            <option v-for="opt in langItems" :key="opt.value" :value="opt.value">{{ opt.label }}</option>
          </select>
        </div>
      </div>

      <div class="content-area">
        <router-view />
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useAppStore } from '@/stores/app'

const router = useRouter()
const route = useRoute()
const store = useAppStore()
const collapsed = ref(true)

const langValue = ref(store.currentLang)
const langItems = store.langItems

function switchLang() {
  store.setLanguage(langValue.value as 'en_US' | 'zh_CN' | 'ru_RU')
}
</script>

<style scoped>
.layout { display: flex; height: 100vh; }
.sidebar {
  width: 200px; background: var(--p-surface-card);
  border-right: 1px solid var(--p-surface-border);
  display: flex; flex-direction: column; padding: 0.75rem;
  transition: width 0.2s; overflow: hidden; flex-shrink: 0;
}
.sidebar.collapsed { width: 52px; }
.sidebar.collapsed .nav-btn {
  justify-content: center; padding: 0.5rem 0; gap: 0;
}
.sidebar-header { display: flex; align-items: center; gap: 0.5rem; cursor: pointer; padding: 0.25rem 0; white-space: nowrap; }
.logo { font-size: 1.75rem; }
.sidebar-divider { border: none; border-top: 1px solid var(--p-surface-border); margin: 0.5rem 0; }
.nav-list { display: flex; flex-direction: column; gap: 0.25rem; }
.nav-btn {
  display: flex; align-items: center; justify-content: flex-start; gap: 0.5rem;
  padding: 0.6rem 0.75rem;
  border: none; background: transparent; border-radius: 12px; cursor: pointer;
  font-size: 0.9rem; color: var(--p-text-color); white-space: nowrap; width: 100%;
  transition: background 0.15s;
}
.nav-btn:hover { background: var(--p-surface-hover); }
.nav-btn.active { background: var(--p-primary-color); color: var(--p-primary-color-text); }
.nav-icon { font-size: 1.2rem; display: inline-flex; align-items: center; justify-content: center; width: 1.5rem; }
.sidebar-footer { margin-top: auto; display: flex; flex-direction: column; gap: 0.25rem; }
.main-area { flex: 1; display: flex; flex-direction: column; min-width: 0; }
.topbar {
  display: flex; align-items: center; justify-content: space-between;
  padding: 0.6rem 1rem; border-bottom: 1px solid var(--p-surface-border);
  background: var(--p-surface-card);
}
.topbar-btn {
  background: none; border: none; font-size: 1.3rem; cursor: pointer;
  padding: 0.25rem 0.5rem; border-radius: 8px;
}
.topbar-btn:hover { background: var(--p-surface-hover); }
.content-area { flex: 1; display: flex; flex-direction: column; overflow-y: auto; padding: 0.75rem 1.5rem; background: var(--p-surface-ground); }
.lang-select {
  background: transparent; border: 1px solid var(--p-surface-border);
  border-radius: 8px; padding: 0.35rem 0.5rem; font-size: 0.875rem;
  color: var(--p-text-color); cursor: pointer; outline: none;
}
.lang-select option { color: #000; background: #fff; }
.theme-btn {
  background: var(--p-surface-hover); border: none; font-size: 1.2rem;
  cursor: pointer; padding: 0.3rem 0.6rem; border-radius: 8px;
  line-height: 1; transition: background 0.15s;
}
.theme-btn:hover { background: var(--p-surface-border); }
</style>
