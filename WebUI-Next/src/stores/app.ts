import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { isAuthenticated, setToken, clearToken } from '@/api/client'
import { locales, localeCodes, t as localeT } from '@/i18n'
import type { LocaleCode } from '@/i18n'

export const useAppStore = defineStore('app', () => {
  const getDefaultDark = () => {
    const saved = localStorage.getItem('ty_theme');
    if (saved) return saved === 'dark';
    return window.matchMedia('(prefers-color-scheme: dark)').matches;
  };
  const isDark = ref(getDefaultDark())

  watch(isDark, (val) => {
    localStorage.setItem('ty_theme', val ? 'dark' : 'light')
  })

  function toggleTheme() {
    isDark.value = !isDark.value
  }

  const authenticated = ref(isAuthenticated())

  function login(secret: string) {
    setToken(secret)
    authenticated.value = true
  }

  function logout() {
    clearToken()
    authenticated.value = false
  }

  const currentLang = ref<LocaleCode>(
    (localStorage.getItem('ty_lang') as LocaleCode) || 'zh_CN'
  )

  function setLanguage(lang: LocaleCode) {
    if (locales[lang]) {
      currentLang.value = lang
      localStorage.setItem('ty_lang', lang)
    }
  }

  /** Translate a key using the current language */
  function t(key: string): string {
    return localeT(currentLang.value, key)
  }

  /** List of available locales with display labels */
  const langItems = localeCodes.map((code) => ({
    label: locales[code].label,
    value: code,
  }))

  return {
    isDark,
    toggleTheme,
    authenticated,
    login,
    logout,
    currentLang,
    setLanguage,
    t,
    langItems,
  }
})
