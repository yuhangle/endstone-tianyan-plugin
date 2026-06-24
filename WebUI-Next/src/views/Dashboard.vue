<template>
  <div class="dashboard">
    <!-- Stat Cards (compact) -->
    <div class="grid">
      <div class="col-12 sm:col-6 lg:col-3" v-for="s in statCards" :key="s.label">
        <Card class="stat-card">
          <template #content>
            <div class="flex align-items-center gap-2">
              <span class="stat-icon">{{ s.icon }}</span>
              <div>
                <div class="stat-value">{{ s.value }}</div>
                <div class="stat-label">{{ s.label }}</div>
              </div>
            </div>
          </template>
        </Card>
      </div>
    </div>

    <!-- Filters -->
    <Card class="mt-2 flex-shrink-0 filter-card">
      <template #content>
        <!-- Row 1: Date range + field filter + keyword -->
        <div class="flex flex-wrap gap-2 align-items-end mb-2">
          <DatePicker v-model="startTime" showTime hourFormat="24" :placeholder="store.t('start_time')" showIcon class="date-picker" />
          <DatePicker v-model="endTime" showTime hourFormat="24" :placeholder="store.t('end_time')" showIcon class="date-picker" />
          <Select v-model="filters.filterType" :options="fieldItems" optionLabel="label" optionValue="value" :placeholder="store.t('filter_field')" class="compact-select" showClear />
          <InputText v-model="filters.filterValue" :placeholder="store.t('keyword')" class="compact-input" @keyup.enter="search(1)" />
        </div>
        <!-- Row 2: Dimension + coord (always visible) + actions (right-aligned) -->
        <div class="flex flex-wrap gap-2 align-items-end">
          <Select v-model="dimensionMode" :options="dimOptions" optionLabel="label" optionValue="value" :placeholder="store.t('all_dimensions')" class="compact-select" showClear />
          <InputText v-if="dimensionMode === '__custom__'" v-model="customDimension" :placeholder="store.t('enter_dimension')" class="compact-input" style="max-width: 140px;" @keyup.enter="search(1)" />
          <InputNumber v-model="filters.cx" placeholder="X" class="coord-input" />
          <InputNumber v-model="filters.cy" placeholder="Y" class="coord-input" />
          <InputNumber v-model="filters.cz" placeholder="Z" class="coord-input" />
          <InputNumber v-model="filters.radius" placeholder="R" :min="0" class="coord-input coord-r" />
          <span style="flex: 1; min-width: 0"></span>
          <Button :label="store.t('reset')" severity="secondary" @click="resetFilters" />
          <Button :label="store.t('search_logs')" @click="search(1)" :loading="loading" />
          <template v-if="done">
            <Chip v-if="queryTime" :label="queryTime" class="text-xs" />
            <Button v-if="logs.length" :label="store.t('export_csv')" severity="secondary" size="small" @click="doExport" />
          </template>
        </div>
      </template>
    </Card>

    <!-- Table Card — flex-grow to fill remaining screen space -->
    <Card class="mt-2 table-card">
      <template #content>
        <div class="table-container" v-if="done && logs.length">
          <DataTable
            :value="logs"
            scrollable
            scrollHeight="flex"
            tableStyle="min-width: 60rem"
            stripedRows
            :loading="loading"
            size="small"
          >
            <Column field="time" :header="store.t('time')" style="width: 155px;">
              <template #body="{ data }">
                <span class="text-sm">{{ fmtTime(data.time) }}</span>
              </template>
            </Column>
            <Column :header="store.t('actor')" style="width: 150px;">
              <template #body="{ data }">
                <div class="font-semibold text-sm">{{ data.name || '-' }}</div>
                <div class="text-xs text-secondary">{{ data.id }}</div>
              </template>
            </Column>
            <Column field="type" header="Type" style="width: 100px;">
              <template #body="{ data }">
                <Tag :value="data.type" :severity="typeSeverity(data.type)" />
              </template>
            </Column>
            <Column :header="store.t('target')" style="width: 150px;">
              <template #body="{ data }">
                <div class="font-semibold text-sm">{{ data.obj_name || '-' }}</div>
                <div class="text-xs text-secondary">{{ data.obj_id }}</div>
              </template>
            </Column>
            <Column :header="store.t('location')" style="width: 250px;">
              <template #body="{ data }">
                <span class="text-sm">{{ fmtPos(data) }}</span>
                <Tag :value="data.world" severity="info" class="ml-1" rounded />
              </template>
            </Column>
            <Column field="data" :header="store.t('details')">
              <template #body="{ data }">
                <span class="text-sm text-secondary">{{ data.data || '-' }}</span>
              </template>
            </Column>
          </DataTable>
        </div>

        <!-- Loading state -->
        <div v-if="loading" class="table-state">
          <span style="font-size: 2rem;">⏳</span>
          <p class="mt-2 text-secondary">{{ store.t('loading') }}</p>
        </div>

        <!-- Empty state -->
        <div v-if="done && !loading && !logs.length" class="table-state">
          <span style="font-size: 3rem;">📭</span>
          <p class="text-lg mt-2">{{ store.t('no_data') }}</p>
        </div>

        <!-- Paginator + jump-to-page (one compact row) -->
        <div v-if="done && totalRecords > 0" class="flex align-items-center gap-1 mt-1 paginator-row">
          <Paginator
            :first="(currentPage - 1) * pageSize"
            :rows="pageSize"
            :totalRecords="totalRecords"
            :rowsPerPageOptions="[50, 100, 200]"
            @page="onPage"
            class="flex-1"
          />
          <InputNumber v-model="jumpPage" :min="1" :max="totalPages" :placeholder="store.t('jump_page_placeholder')" class="jump-input" />
          <Button :label="store.t('jump_page')" severity="secondary" size="small" @click="doJumpPage" class="p-1" />
          <span v-if="jumpError" class="text-xs text-red-500" style="max-width: 140px;">{{ jumpError }}</span>
        </div>
      </template>
    </Card>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, computed, onMounted } from 'vue'
import { useAppStore } from '@/stores/app'
import { fetchStats, fetchLogs } from '@/api/client'
import type { Stats, LogEntry } from '@/api/types'

const store = useAppStore()

const stats = ref<Stats | null>(null)
const logs = ref<LogEntry[]>([])
const loading = ref(false)
const done = ref(false)
const currentPage = ref(1)
const totalPages = ref(0)
const totalRecords = ref(0)
const pageSize = ref(100)
const queryTime = ref('')

// 跳页功能
const jumpPage = ref<number | null>(null)
const jumpError = ref('')

// 日期时间筛选
const startTime = ref<Date | null>(null)
const endTime = ref<Date | null>(null)

// 维度筛选（自带预定义 + 自定义）
const dimensionMode = ref('')
const customDimension = ref('')

const filters = reactive({
  filterType: '',
  filterValue: '',
  cx: null as number | null,
  cy: null as number | null,
  cz: null as number | null,
  radius: null as number | null,
})

const fieldItems = computed(() => [
  { label: store.t('all_fields'), value: '' },
  { label: store.t('actor_name'), value: 'name' },
  { label: store.t('actor_id'), value: 'id' },
  { label: store.t('action_type'), value: 'type' },
  { label: store.t('target_name'), value: 'obj_name' },
  { label: store.t('target_id'), value: 'obj_id' },
])

const dimOptions = computed(() => [
  { label: store.t('all_dimensions'), value: '' },
  { label: store.t('overworld'), value: 'Overworld' },
  { label: store.t('nether'), value: 'Nether' },
  { label: store.t('the_end'), value: 'TheEnd' },
  { label: store.t('custom'), value: '__custom__' },
])

const statCards = computed(() => [
  { label: store.t('total_logs'), value: stats.value?.total_logs?.toLocaleString() ?? '--', icon: '📝' },
  { label: store.t('db_size'), value: stats.value?.db_size ?? '--', icon: '💾' },
  { label: store.t('query_time'), value: queryTime.value || '--', icon: '⚡' },
  { label: 'Status', value: stats.value ? 'Online' : '--', icon: '✅' },
])

function typeSeverity(type: string) {
  if (type.includes('place')) return 'success'
  if (type.includes('break') || type.includes('bomb')) return 'danger'
  if (type.includes('damage') || type.includes('die')) return 'warn'
  if (type.includes('interact') || type.includes('click')) return 'info'
  return 'secondary'
}

function fmtTime(ts: number) { return new Date(ts * 1000).toLocaleString() }
function fmtPos(row: LogEntry) { return `${row.pos_x?.toFixed(1)}, ${row.pos_y?.toFixed(1)}, ${row.pos_z?.toFixed(1)}` }

function onPage(event: { page: number; rows: number }) {
  pageSize.value = event.rows
  search(event.page + 1)
}

function doJumpPage() {
  if (!jumpPage.value || jumpPage.value < 1) {
    jumpError.value = store.t('invalid_page_number')
    return
  }
  if (jumpPage.value > totalPages.value) {
    jumpError.value = store.t('page_exceeds_max').replace('{max}', String(totalPages.value))
    return
  }
  jumpError.value = ''
  search(jumpPage.value)
  jumpPage.value = null
}

async function search(page: number) {
  currentPage.value = page
  loading.value = true
  const params: Record<string, string | number> = { page, page_size: pageSize.value }
  if (filters.filterType) params.filter_type = filters.filterType
  if (filters.filterValue) params.filter_value = filters.filterValue
  if (startTime.value) params.start_time = Math.floor(startTime.value.getTime() / 1000)
  if (endTime.value) params.end_time = Math.floor(endTime.value.getTime() / 1000)
  // 维度：自定义模式优先使用 customDimension
  const dim = dimensionMode.value === '__custom__' ? customDimension.value : dimensionMode.value
  if (dim) params.dimension = dim
  if (filters.cx != null && filters.cy != null && filters.cz != null && filters.radius != null) {
    params.center_x = Number(filters.cx)
    params.center_y = Number(filters.cy)
    params.center_z = Number(filters.cz)
    params.radius = Number(filters.radius)
  }
  try {
    const res = await fetchLogs(params)
    logs.value = res.data
    totalPages.value = res.total_pages
    totalRecords.value = res.total
    queryTime.value = `${res.query_time_ms.toFixed(1)} ms`
  } catch { /* */ }
  done.value = true
  loading.value = false
}

function resetFilters() {
  filters.filterType = ''; filters.filterValue = ''
  filters.cx = null; filters.cy = null; filters.cz = null; filters.radius = null
  startTime.value = null; endTime.value = null
  dimensionMode.value = ''; customDimension.value = ''
  search(1)
}

async function doExport() {
  if (totalRecords.value === 0) return
  const { fetchExport } = await import('@/api/client')
  try {
    const res = await fetchExport({ start_page: 1, end_page: Math.min(500, totalPages.value), page_size: pageSize.value })
    if (!res.data?.length) return
    const rows = res.data.map((r: LogEntry) => [
      new Date(r.time * 1000).toISOString(), r.id,
      `"${(r.name || '').replace(/"/g, '""')}"`, r.type,
      r.obj_id, `"${(r.obj_name || '').replace(/"/g, '""')}"`,
      r.world, String(r.pos_x), String(r.pos_y), String(r.pos_z),
      `"${(r.data || '').replace(/"/g, '""')}"`,
    ])
    const csv = '﻿' + [
      ['Time', 'SourceID', 'SourceName', 'Type', 'TargetID', 'TargetName', 'World', 'X', 'Y', 'Z', 'Data'].join(','),
      ...rows.map((e: string[]) => e.join(',')),
    ].join('\n')
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' })
    const a = document.createElement('a')
    a.href = URL.createObjectURL(blob)
    a.download = `tianyan_export_${Date.now()}.csv`
    a.click()
    URL.revokeObjectURL(a.href)
  } catch { /* */ }
}

onMounted(async () => {
  const [s] = await Promise.allSettled([fetchStats(), search(1)])
  if (s.status === 'fulfilled') stats.value = s.value
})
</script>

<style scoped>
.dashboard {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;
}

/* ---- Compact stat cards ---- */
.stat-card {
  border-radius: 12px;
  box-shadow: 0 2px 12px rgba(0,0,0,0.06);
  transition: transform 0.2s, box-shadow 0.2s;
  overflow: hidden;
}
.stat-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 6px 20px rgba(0,0,0,0.10);
}
.stat-card:deep(.p-card-body) {
  padding: 0.5rem 0.75rem;
}
.stat-card:deep(.p-card-content) {
  padding: 0;
}
.stat-icon {
  font-size: 1.5rem;
  line-height: 1;
}
.stat-value {
  font-size: 1.2rem;
  font-weight: 700;
  line-height: 1.3;
}
.stat-label {
  font-size: 0.75rem;
  color: var(--p-text-muted-color);
}

/* Make the table card flex to fill remaining screen height */
.table-card {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
}
.table-card :deep(.p-card-body) {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  padding: 0;
}
.table-card :deep(.p-card-content) {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  padding: 0.5rem 0.75rem;
}
/* The scrollable table wrapper fills its flex parent */
.table-container {
  flex: 1;
  min-height: 0;
  position: relative;
}
/* Compact filter card */
.filter-card :deep(.p-card-content) {
  padding-top: 0.5rem;
  padding-bottom: 0.5rem;
}
/* Compact inputs in filter card */
.filter-card .date-picker {
  max-width: 230px;
}
.filter-card .compact-select {
  max-width: 210px;
}
.filter-card .compact-select:deep(.p-select-label) {
  font-size: 0.85rem;
}
.filter-card .compact-input {
  max-width: 180px;
}
/* Coordinate input boxes: fix width, prevent flex stretch */
:deep(.coord-input) {
  width: 75px !important;
  flex: none !important;
}
:deep(.coord-input .p-inputtext) {
  width: 100% !important;
  min-width: 0 !important;
}
:deep(.coord-r) {
  width: 55px !important;
}
/* Centered state messages */
.table-state {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 200px;
  color: var(--p-text-muted-color);
}
/* Paginator row: compact, all in one line */
.paginator-row {
  min-height: 2rem;
}
.paginator-row :deep(.p-paginator) {
  padding: 0;
  min-height: auto;
  gap: 1px;
}
.paginator-row :deep(.p-paginator .p-paginator-page),
.paginator-row :deep(.p-paginator .p-paginator-pages .p-paginator-page) {
  min-width: 1.8rem;
  height: 1.6rem;
  font-size: 0.75rem;
}
.paginator-row :deep(.p-paginator .p-paginator-first),
.paginator-row :deep(.p-paginator .p-paginator-prev),
.paginator-row :deep(.p-paginator .p-paginator-next),
.paginator-row :deep(.p-paginator .p-paginator-last) {
  min-width: 1.8rem;
  height: 1.6rem;
}
.paginator-row :deep(.p-paginator .p-dropdown) {
  height: 1.6rem;
}
.paginator-row :deep(.p-paginator .p-paginator-current) {
  font-size: 0.75rem;
}
/* Jump input: extra compact */
.jump-input :deep(.p-inputtext) {
  width: 60px;
  padding: 0.2rem 0.4rem;
  font-size: 0.75rem;
  height: 1.6rem;
}
</style>
