// ============================================================
// Embedded WebUI language files (from old WebUI/languages/)
// These are UI-specific translations, NOT the C++ plugin's
// in-game message translations.
// ============================================================

import enUS from '@/locales/en_US.json'
import zhCN from '@/locales/zh_CN.json'
import ruRU from '@/locales/ru_RU.json'

export type LocaleCode = 'en_US' | 'zh_CN' | 'ru_RU'

export interface LocaleDef {
  label: string
  data: Record<string, string>
}

export const locales: Record<LocaleCode, LocaleDef> = {
  en_US: { label: 'English', data: enUS as Record<string, string> },
  zh_CN: { label: '中文', data: zhCN as Record<string, string> },
  ru_RU: { label: 'Русский', data: ruRU as Record<string, string> },
}

export const localeCodes = Object.keys(locales) as LocaleCode[]

/** Look up a translated string by key. Falls back to key itself. */
export function t(lang: LocaleCode, key: string): string {
  return locales[lang]?.data?.[key] ?? locales['en_US']?.data?.[key] ?? key
}

/** Get all locale codes the frontend has embedded */
export function getAvailableLocales(): LocaleCode[] {
  return localeCodes
}
