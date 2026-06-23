<template>
  <div class="dashboard">
    <!-- Stat Cards -->
    <div class="grid">
      <div class="col-12 sm:col-6 lg:col-3" v-for="s in statCards" :key="s.label">
        <Card class="stat-card">
          <template #content>
            <div class="flex align-items-center gap-3">
              <span style="font-size: 2rem;">{{ s.icon }}</span>
              <div>
                <div style="font-size: 1.5rem; font-weight: 700;">{{ s.value }}</div>
                <div class="text-sm text-secondary">{{ s.label }}</div>
              </div>
            </div>
          </template>
        </Card>
      </div>
    </div>

    <!-- Filters -->
    <Card class="mt-2 flex-shrink-0 filter-card">
      <template #content>
        <div class="flex flex-wrap gap-2 align-items-end">
          <span style="flex: 1; min-width: 140px;">
            <InputText v-model="filters.filterValue" :placeholder="'🔍 ' + store.t('enter_keyword')" class="w-full" @keyup.enter="search(1)" />
          </span>
          <Select v-model="filters.filterType" :options="fieldItems" optionLabel="label" optionValue="value" :placeholder="store.t('filter_field')" class="flex-1" style="min-width: 120px;" showClear />
          <Select v-model="filters.dimension" :options="dimItems" optionLabel="label" optionValue="value" placeholder="Dimension" class="flex-1" style="min-width: 120px;" showClear />
          <Button :label="store.t('reset')" severity="secondary" @click="resetFilters" />
          <Button :label="store.t('search_logs')" @click="search(1)" :loading="loading" />
          <template v-if="done">
            <Chip v-if="queryTime" :label="queryTime" class="text-xs" />
            <Button v-if="logs.length" :label="store.t('export_csv')" severity="secondary" size="small" @click="doExport" />
          </template>
        </div>
        <div class="flex flex-wrap gap-2 mt-2">
          <Button :label="store.t('coord_query')" severity="secondary" size="small" :outlined="!showCoord" @click="showCoord = !showCoord" />
          <template v-if="showCoord">
            <InputNumber v-model="filters.cx" placeholder="X" style="width: 90px;" />
            <InputNumber v-model="filters.cy" placeholder="Y" style="width: 90px;" />
            <InputNumber v-model="filters.cz" placeholder="Z" style="width: 90px;" />
            <InputNumber v-model="filters.radius" placeholder="R" :min="0" style="width: 90px;" />
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

        <!-- Paginator -->
        <Paginator
          v-if="done && totalRecords > 0"
          :first="(currentPage - 1) * pageSize"
          :rows="pageSize"
          :totalRecords="totalRecords"
          :rowsPerPageOptions="[50, 100, 200]"
          @page="onPage"
          class="mt-2 flex-shrink-0"
        />
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
const showCoord = ref(false)

const filters = reactive({
  filterType: '',
  filterValue: '',
  dimension: '',
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
])

const dimItems = computed(() => [
  { label: store.t('all_dimensions'), value: '' },
  { label: store.t('overworld'), value: 'Overworld' },
  { label: store.t('nether'), value: 'Nether' },
  { label: store.t('the_end'), value: 'TheEnd' },
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

async function search(page: number) {
  currentPage.value = page
  loading.value = true
  const params: Record<string, string | number> = { page, page_size: pageSize.value }
  if (filters.filterType) params.filter_type = filters.filterType
  if (filters.filterValue) params.filter_value = filters.filterValue
  if (filters.dimension) params.dimension = filters.dimension
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
  filters.filterType = ''; filters.filterValue = ''; filters.dimension = ''
  filters.cx = null; filters.cy = null; filters.cz = null; filters.radius = null
  showCoord.value = false
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
.stat-card {
  border-radius: 16px;
  box-shadow: 0 4px 20px rgba(0,0,0,0.07);
  transition: transform 0.25s, box-shadow 0.25s;
  overflow: hidden;
}
.stat-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 10px 32px rgba(0,0,0,0.12);
}
.stat-card:deep(.p-card-body) {
  padding: 1rem 1.25rem;
}
.stat-card:deep(.p-card-content) {
  padding: 0;
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
}
.table-card :deep(.p-card-content) {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  padding: 0.75rem;
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
/* Compact paginator — reduce height and padding */
.table-card :deep(.p-paginator) {
  padding: 0.25rem 0;
  min-height: auto;
}
.table-card :deep(.p-paginator .p-paginator-page),
.table-card :deep(.p-paginator .p-paginator-pages .p-paginator-page) {
  min-width: 2rem;
  height: 2rem;
  font-size: 0.8rem;
}
.table-card :deep(.p-paginator .p-paginator-first),
.table-card :deep(.p-paginator .p-paginator-prev),
.table-card :deep(.p-paginator .p-paginator-next),
.table-card :deep(.p-paginator .p-paginator-last) {
  min-width: 2rem;
  height: 2rem;
}
.table-card :deep(.p-paginator .p-dropdown) {
  height: 2rem;
}
.table-card :deep(.p-paginator .p-paginator-current) {
  font-size: 0.8rem;
}
</style>
