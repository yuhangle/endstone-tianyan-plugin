<template>
  <Card>
    <template #title>🗄️ {{ store.t('db_info') }}</template>
    <template #content>
      <div v-if="info" class="grid">
        <div class="col-12 md:col-6">
          <div class="flex flex-column gap-3">
            <div class="flex"><span class="font-semibold w-8rem">{{ store.t('table_count') }}:</span><span>{{ info.tables?.join(', ') || 'N/A' }}</span></div>
            <div class="flex"><span class="font-semibold w-8rem">{{ store.t('index_count') }}:</span><span>{{ info.indexes?.join(', ') || 'N/A' }}</span></div>
            <div class="flex"><span class="font-semibold w-8rem">{{ store.t('total_rows') }}:</span><span>{{ info.total_rows?.toLocaleString() }}</span></div>
            <div class="flex"><span class="font-semibold w-8rem">{{ store.t('db_size') }}:</span><span>{{ dbSize }}</span></div>
          </div>
        </div>
        <div class="col-12 mt-3">
          <h3 class="text-lg mb-2">📋 Column Schema</h3>
          <DataTable :value="info.columns" scrollable scrollHeight="300" tableStyle="min-width: 30rem" stripedRows size="small">
            <Column field="name" :header="store.t('time')" />
            <Column field="type" header="Type" />
            <Column field="notnull" header="Not Null">
              <template #body="{ data }">{{ data.notnull ? '✅' : '❌' }}</template>
            </Column>
            <Column field="pk" header="PK">
              <template #body="{ data }">{{ data.pk ? '✅' : '❌' }}</template>
            </Column>
          </DataTable>
        </div>
      </div>
      <div v-else-if="!loading" class="text-center py-5 text-secondary">{{ store.t('no_data') }}</div>
    </template>
  </Card>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useAppStore } from '@/stores/app'
import { fetchDbInfo, fetchStats } from '@/api/client'
import type { DbInfo } from '@/api/types'

const store = useAppStore()
const info = ref<DbInfo | null>(null)
const dbSize = ref('')
const loading = ref(true)

onMounted(async () => {
  try {
    const [db, st] = await Promise.all([fetchDbInfo(), fetchStats()])
    info.value = db
    dbSize.value = st.db_size
  } catch { /* */ }
  loading.value = false
})
</script>
