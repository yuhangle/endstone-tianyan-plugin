// ============================================================
// API 类型定义 — 与 C++ 后端 nlohmann::json 响应形状一致
// ============================================================

// ---------- Log Entry ----------
export interface LogEntry {
  uuid: string
  id: string
  name: string
  pos_x: number
  pos_y: number
  pos_z: number
  world: string
  obj_id: string
  obj_name: string
  time: number       // Unix timestamp (seconds)
  type: string
  data: string
  status: string
  distance?: number  // only present in spatial queries
}

// ---------- /api/stats ----------
export interface Stats {
  total_logs: number
  db_size: string
  server_time: number
}

// ---------- /api/logs ----------
export interface LogQueryResult {
  data: LogEntry[]
  total: number
  page: number
  page_size: number
  total_pages: number
  query_time_ms: number
}

// ---------- /api/export ----------
export interface ExportResult {
  data: LogEntry[]
  total_records: number
  exported_records: number
  pages_exported: number
  start_page: number
  end_page: number
  total_pages: number
}

// ---------- /api/db_info ----------
export interface ColumnInfo {
  name: string
  type: string
  notnull: number
  pk: number
}

export interface DbInfo {
  tables: string[]
  indexes: string[]
  columns: ColumnInfo[]
  total_rows: number
}

// ---------- /api/languages ----------
export interface Languages {
  languages: string[]
  default: string
}

// ---------- /api/debug/query ----------
export interface DebugQueryResult {
  success: boolean
  row_count?: number
  data?: Record<string, string>[]
  total?: number
  error?: string
}

// ============================================================
// 前端内部类型
// ============================================================

export interface LogQueryParams {
  page?: number
  page_size?: number
  start_time?: number | null
  end_time?: number | null
  filter_type?: string
  filter_value?: string
  center_x?: number | null
  center_y?: number | null
  center_z?: number | null
  radius?: number | null
  dimension?: string
}
