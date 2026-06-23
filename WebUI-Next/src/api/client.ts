import axios, { AxiosError, type AxiosInstance } from 'axios'
import type {
  LogQueryResult,
  ExportResult,
  Stats,
  DbInfo,
  Languages,
  DebugQueryResult,
  LogQueryParams,
} from './types'

// ---------- 单例 Axios Instance ----------
const api: AxiosInstance = axios.create({
  baseURL: '/',
  timeout: 30000,
})

// ---------- Token 管理 ----------
const STORAGE_KEY = 'ty_secret'

export function getToken(): string {
  return localStorage.getItem(STORAGE_KEY) ?? ''
}

export function setToken(token: string): void {
  localStorage.setItem(STORAGE_KEY, token)
}

export function clearToken(): void {
  localStorage.removeItem(STORAGE_KEY)
}

export function isAuthenticated(): boolean {
  return !!getToken()
}

// ---------- 请求拦截器：注入 X-Secret ----------
api.interceptors.request.use((config) => {
  const token = getToken()
  if (token) {
    config.headers['X-Secret'] = token
  }
  const lang = localStorage.getItem('ty_lang') || 'zh_CN'
  config.headers['X-Lang'] = lang
  return config
})

// ---------- 响应拦截器：全局 403 处理 ----------
api.interceptors.response.use(
  (res) => res,
  (error: AxiosError) => {
    if (error.response?.status === 403) {
      clearToken()
      window.dispatchEvent(new CustomEvent('auth:required'))
    }
    return Promise.reject(error)
  },
)

// ============================================================
// API 方法
// ============================================================

export async function fetchStats(): Promise<Stats> {
  const { data } = await api.get<Stats>('/api/stats')
  return data
}

export async function fetchLogs(params: LogQueryParams): Promise<LogQueryResult> {
  const clean: Record<string, string | number> = {}
  for (const [k, v] of Object.entries(params)) {
    if (v !== null && v !== undefined && v !== '') {
      clean[k] = v
    }
  }
  const { data } = await api.get<LogQueryResult>('/api/logs', { params: clean })
  return data
}

export async function fetchExport(params: LogQueryParams & { start_page: number; end_page: number }): Promise<ExportResult> {
  const { data } = await api.get<ExportResult>('/api/export', { params })
  return data
}

export async function fetchDbInfo(): Promise<DbInfo> {
  const { data } = await api.get<DbInfo>('/api/db_info')
  return data
}

export async function fetchLanguages(): Promise<Languages> {
  const { data } = await api.get<Languages>('/api/languages')
  return data
}

export async function fetchLanguage(lang: string): Promise<Record<string, string>> {
  const { data } = await api.get<Record<string, string>>(`/api/language/${lang}`)
  return data
}

export async function debugQuery(sql: string): Promise<DebugQueryResult> {
  const { data } = await api.get<DebugQueryResult>('/api/debug/query', { params: { sql } })
  return data
}

export { api }
