const quickRanges = [
  { value: 2, label: "2 小时" },
  { value: 12, label: "12 小时" },
  { value: 24, label: "24 小时" },
  { value: 72, label: "3 天" },
  { value: 168, label: "7 天" }
];

const fallbackPresets = [
  { value: "recent_activity", label: "最近活动" },
  { value: "block_changes", label: "方块改动" },
  { value: "combat", label: "战斗与死亡" },
  { value: "interactions", label: "交互记录" },
  { value: "redstone", label: "红石触发" },
  { value: "explosions", label: "爆炸相关" }
];

const fallbackActions = [
  { value: "block_break", label: "方块破坏" },
  { value: "block_place", label: "方块放置" },
  { value: "entity_damage", label: "实体伤害" },
  { value: "damage", label: "环境伤害" },
  { value: "player_right_click_block", label: "方块触发" },
  { value: "player_right_click_entity", label: "实体交互" },
  { value: "entity_bomb", label: "爆炸源" },
  { value: "block_break_bomb", label: "爆炸破坏" },
  { value: "piston_extend", label: "活塞伸出" },
  { value: "piston_retract", label: "活塞收回" },
  { value: "entity_die", label: "实体死亡" },
  { value: "player_pickup_item", label: "拾取物品" }
];

const state = {
  presets: fallbackPresets,
  actions: fallbackActions,
  worlds: [],
  preset: "recent_activity",
  activeRange: 12,
  results: [],
  nextCursor: null,
  hasMore: false,
  searched: false,
  loading: false,
  loadingMore: false,
  exportLoading: false,
  queryTimeMs: null,
  stats: null
};

const dom = {
  statusText: document.querySelector("#statusText"),
  refreshStats: document.querySelector("#refreshStats"),
  resetFilters: document.querySelector("#resetFilters"),
  presetRow: document.querySelector("#presetRow"),
  rangeRow: document.querySelector("#rangeRow"),
  windowLabel: document.querySelector("#windowLabel"),
  startTime: document.querySelector("#startTime"),
  endTime: document.querySelector("#endTime"),
  playerName: document.querySelector("#playerName"),
  playerSuggestions: document.querySelector("#playerSuggestions"),
  actionType: document.querySelector("#actionType"),
  world: document.querySelector("#world"),
  targetName: document.querySelector("#targetName"),
  targetId: document.querySelector("#targetId"),
  exportLimit: document.querySelector("#exportLimit"),
  centerX: document.querySelector("#centerX"),
  centerY: document.querySelector("#centerY"),
  centerZ: document.querySelector("#centerZ"),
  radius: document.querySelector("#radius"),
  runQuery: document.querySelector("#runQuery"),
  exportCsv: document.querySelector("#exportCsv"),
  clearResults: document.querySelector("#clearResults"),
  activeFilters: document.querySelector("#activeFilters"),
  feedback: document.querySelector("#feedback"),
  resultMeta: document.querySelector("#resultMeta"),
  loadMore: document.querySelector("#loadMore"),
  emptyState: document.querySelector("#emptyState"),
  tableWrap: document.querySelector("#tableWrap"),
  resultBody: document.querySelector("#resultBody"),
  cardList: document.querySelector("#cardList")
};

let playerTimer = null;

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function toDatetimeLocal(timestampMs) {
  const date = new Date(timestampMs);
  const pad = (value) => String(value).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

function toUnixSeconds(value) {
  if (!value) {
    return null;
  }
  const parsed = new Date(value).getTime();
  if (Number.isNaN(parsed)) {
    throw new Error("时间格式无效");
  }
  return Math.floor(parsed / 1000);
}

function formatTime(unixSeconds) {
  if (!unixSeconds) {
    return "-";
  }
  return new Date(unixSeconds * 1000).toLocaleString();
}

function formatPosition(row) {
  const values = [row.pos_x, row.pos_y, row.pos_z].map((value) => {
    const number = Number(value);
    return Number.isFinite(number) ? number.toFixed(1) : "-";
  });
  const distance = row.distance === undefined ? "" : ` / ${row.distance} 格`;
  return `${values.join(", ")} / ${row.world || "-"}${distance}`;
}

function actionLabel(value) {
  const action = state.actions.find((item) => item.value === value);
  return action ? action.label : value || "-";
}

async function api(mode, params = new URLSearchParams()) {
  const query = new URLSearchParams(params);
  query.set("mode", mode);

  const response = await fetch(`/api/query?${query.toString()}`, {
    headers: { accept: "application/json" },
    cache: "no-store"
  });
  const data = await response.json().catch(() => ({}));
  if (!response.ok) {
    throw new Error(data.detail || `请求失败 ${response.status}`);
  }
  return data;
}

function setFeedback(message, type = "success") {
  dom.feedback.textContent = message;
  dom.feedback.className = `feedback ${type}`;
  dom.feedback.hidden = false;
}

function clearFeedback() {
  dom.feedback.textContent = "";
  dom.feedback.className = "feedback";
  dom.feedback.hidden = true;
}

function setStatus(message, type = "") {
  dom.statusText.textContent = message;
  dom.statusText.className = `status-text ${type}`.trim();
}

function renderPresets() {
  dom.presetRow.innerHTML = "";
  for (const preset of state.presets) {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = preset.label;
    button.className = preset.value === state.preset ? "active" : "";
    button.addEventListener("click", () => {
      state.preset = preset.value;
      renderPresets();
      renderActiveFilters();
    });
    dom.presetRow.append(button);
  }
}

function renderRanges() {
  dom.rangeRow.innerHTML = "";
  for (const range of quickRanges) {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = range.label;
    button.className = range.value === state.activeRange ? "active" : "";
    button.addEventListener("click", () => applyQuickRange(range.value));
    dom.rangeRow.append(button);
  }
  dom.windowLabel.textContent = `${state.activeRange} 小时`;
}

function renderSelects() {
  dom.actionType.innerHTML = '<option value="">全部类型</option>';
  for (const action of state.actions) {
    const option = document.createElement("option");
    option.value = action.value;
    option.textContent = action.label;
    dom.actionType.append(option);
  }

  dom.world.innerHTML = '<option value="all">全部维度</option>';
  for (const world of state.worlds) {
    const option = document.createElement("option");
    option.value = world;
    option.textContent = world;
    dom.world.append(option);
  }
}

function activeFilterLabels() {
  const labels = [];
  const preset = state.presets.find((item) => item.value === state.preset);
  if (preset) {
    labels.push(`预设：${preset.label}`);
  }
  if (dom.playerName.value.trim()) {
    labels.push(`玩家：${dom.playerName.value.trim()}`);
  }
  if (dom.actionType.value) {
    labels.push(`行为：${actionLabel(dom.actionType.value)}`);
  }
  if (dom.targetName.value.trim()) {
    labels.push(`目标名称：${dom.targetName.value.trim()}`);
  }
  if (dom.targetId.value.trim()) {
    labels.push(`目标 ID：${dom.targetId.value.trim()}`);
  }
  if (dom.world.value && dom.world.value !== "all") {
    labels.push(`维度：${dom.world.value}`);
  }
  if (dom.centerX.value && dom.centerY.value && dom.centerZ.value && dom.radius.value) {
    labels.push(`范围：${dom.centerX.value}, ${dom.centerY.value}, ${dom.centerZ.value} / ${dom.radius.value}`);
  }
  return labels;
}

function renderActiveFilters() {
  dom.activeFilters.innerHTML = "";
  for (const label of activeFilterLabels()) {
    const chip = document.createElement("span");
    chip.className = "chip";
    chip.textContent = label;
    dom.activeFilters.append(chip);
  }
}

function renderControls() {
  dom.runQuery.disabled = state.loading || state.loadingMore;
  dom.loadMore.disabled = !state.hasMore || state.loading || state.loadingMore;
  dom.exportCsv.disabled = state.loading || state.exportLoading;
  dom.runQuery.textContent = state.loading ? "查询中" : "查询";
  dom.loadMore.textContent = state.loadingMore ? "加载中" : state.hasMore ? "加载更多" : "没有更多";
  dom.exportCsv.textContent = state.exportLoading ? "导出中" : "导出 CSV";
}

function renderResultMeta() {
  if (!state.searched) {
    dom.resultMeta.textContent = "尚未查询";
    return;
  }

  const queryTime = state.queryTimeMs ? ` / ${state.queryTimeMs} ms` : "";
  dom.resultMeta.textContent = `${state.results.length.toLocaleString()} 条${queryTime}`;
}

function statusBadge(status) {
  if (!status) {
    return "";
  }
  return `<span class="status-badge ${escapeHtml(status)}">${escapeHtml(status)}</span>`;
}

function rowHtml(row) {
  return `
    <tr>
      <td class="numeric">${escapeHtml(formatTime(row.time))}</td>
      <td>
        <div class="primary-text">${escapeHtml(row.name || row.id || "未知来源")}</div>
        <div class="secondary-text">${escapeHtml(row.id || "-")}</div>
      </td>
      <td>
        <div class="primary-text">${escapeHtml(actionLabel(row.type))}</div>
        ${statusBadge(row.status)}
      </td>
      <td>
        <div class="primary-text">${escapeHtml(row.obj_name || row.obj_id || "-")}</div>
        <div class="secondary-text">${escapeHtml(row.obj_id || "-")}</div>
      </td>
      <td class="numeric">${escapeHtml(formatPosition(row))}</td>
      <td class="data-text">${escapeHtml(row.data || "-")}</td>
    </tr>`;
}

function cardHtml(row) {
  const source = row.name || row.id || "未知来源";
  const target = row.obj_name || row.obj_id || "-";
  return `
    <article class="result-card">
      <div class="card-line"><span>时间</span><span>${escapeHtml(formatTime(row.time))}</span></div>
      <div class="card-line"><span>来源</span><span>${escapeHtml(source)}</span></div>
      <div class="card-line"><span>行为</span><span>${escapeHtml(actionLabel(row.type))} ${statusBadge(row.status)}</span></div>
      <div class="card-line"><span>目标</span><span>${escapeHtml(target)}</span></div>
      <div class="card-line"><span>位置</span><span>${escapeHtml(formatPosition(row))}</span></div>
      <div class="card-line"><span>详情</span><span>${escapeHtml(row.data || "-")}</span></div>
    </article>`;
}

function renderLoading() {
  dom.emptyState.hidden = true;
  dom.tableWrap.hidden = false;
  dom.cardList.hidden = false;
  dom.resultBody.innerHTML = Array.from({ length: 6 }, () => `
    <tr class="loading-row">
      <td colspan="6"><div class="skeleton"></div></td>
    </tr>`).join("");
  dom.cardList.innerHTML = Array.from({ length: 3 }, () => `
    <article class="result-card"><div class="skeleton"></div></article>`).join("");
}

function renderResults() {
  renderResultMeta();
  renderControls();

  if (state.loading && !state.loadingMore) {
    renderLoading();
    return;
  }

  if (!state.searched) {
    dom.emptyState.hidden = false;
    dom.emptyState.textContent = "选择条件后查询";
    dom.tableWrap.hidden = true;
    dom.cardList.hidden = true;
    return;
  }

  if (state.results.length === 0) {
    dom.emptyState.hidden = false;
    dom.emptyState.textContent = "没有匹配结果";
    dom.tableWrap.hidden = true;
    dom.cardList.hidden = true;
    return;
  }

  dom.emptyState.hidden = true;
  dom.tableWrap.hidden = false;
  dom.cardList.hidden = false;
  dom.resultBody.innerHTML = state.results.map(rowHtml).join("");
  dom.cardList.innerHTML = state.results.map(cardHtml).join("");
}

function applyQuickRange(hours) {
  state.activeRange = hours;
  const now = Date.now();
  dom.endTime.value = toDatetimeLocal(now);
  dom.startTime.value = toDatetimeLocal(now - hours * 3600 * 1000);
  renderRanges();
  renderActiveFilters();
}

function buildQueryParams() {
  const params = new URLSearchParams();
  params.set("limit", "100");
  params.set("preset", state.preset || "recent_activity");

  const startTime = toUnixSeconds(dom.startTime.value);
  const endTime = toUnixSeconds(dom.endTime.value);
  if (startTime !== null) {
    params.set("start_time", String(startTime));
  }
  if (endTime !== null) {
    params.set("end_time", String(endTime));
  }

  const textFields = [
    [dom.playerName, "player_name"],
    [dom.targetName, "target_name"],
    [dom.targetId, "target_id"]
  ];
  for (const [input, name] of textFields) {
    const value = input.value.trim();
    if (value) {
      params.set(name, value);
    }
  }

  if (dom.actionType.value) {
    params.set("action_type", dom.actionType.value);
  }
  if (dom.world.value && dom.world.value !== "all") {
    params.set("world", dom.world.value);
  }

  const coordValues = [dom.centerX.value, dom.centerY.value, dom.centerZ.value, dom.radius.value];
  const hasAnyCoord = coordValues.some((value) => value !== "");
  const hasAllCoord = coordValues.every((value) => value !== "");
  if (hasAnyCoord && !hasAllCoord) {
    throw new Error("坐标过滤需要同时填写 X / Y / Z / 半径");
  }
  if (hasAllCoord) {
    params.set("center_x", dom.centerX.value);
    params.set("center_y", dom.centerY.value);
    params.set("center_z", dom.centerZ.value);
    params.set("radius", dom.radius.value);
  }

  return params;
}

async function loadOptions() {
  const data = await api("options");
  state.presets = data.presets && data.presets.length ? data.presets : fallbackPresets;
  state.actions = data.actions && data.actions.length ? data.actions : fallbackActions;
  state.worlds = data.worlds || [];
  renderPresets();
  renderSelects();
  renderActiveFilters();
}

async function refreshStats() {
  dom.refreshStats.disabled = true;
  try {
    const params = new URLSearchParams({ force_refresh: "true" });
    state.stats = await api("stats", params);
    setFeedback(`状态：约 ${state.stats.total_logs.toLocaleString()} 条，${state.stats.db_size}`);
  } catch (error) {
    setFeedback(error.message || "状态刷新失败", "error");
  } finally {
    dom.refreshStats.disabled = false;
  }
}

async function checkHealth() {
  try {
    const data = await api("health");
    setStatus(data.ok ? "API 已连接" : "API 异常", data.ok ? "ok" : "error");
  } catch (error) {
    setStatus("API 不可用", "error");
  }
}

function clearResults() {
  state.results = [];
  state.nextCursor = null;
  state.hasMore = false;
  state.searched = false;
  state.queryTimeMs = null;
  clearFeedback();
  renderResults();
}

async function runQuery(loadMore = false) {
  if (state.loading || state.loadingMore) {
    return;
  }
  clearFeedback();

  try {
    const params = buildQueryParams();
    if (loadMore && state.nextCursor) {
      params.set("cursor_time", String(state.nextCursor.time));
      params.set("cursor_uuid", state.nextCursor.uuid);
    }

    if (loadMore) {
      state.loadingMore = true;
    } else {
      state.loading = true;
      state.results = [];
      state.nextCursor = null;
      state.hasMore = false;
    }
    renderControls();
    renderResults();

    const data = await api("logs", params);
    state.results = loadMore ? state.results.concat(data.data || []) : (data.data || []);
    state.nextCursor = data.next_cursor;
    state.hasMore = Boolean(data.has_more);
    state.queryTimeMs = data.query_time_ms;
    state.searched = true;
    setFeedback(`本次返回 ${data.count} 条`);
  } catch (error) {
    setFeedback(error.message || "查询失败", "error");
  } finally {
    state.loading = false;
    state.loadingMore = false;
    renderActiveFilters();
    renderResults();
  }
}

function csvEscape(value) {
  return `"${String(value ?? "").replaceAll('"', '""')}"`;
}

async function exportCsv() {
  if (state.exportLoading) {
    return;
  }

  clearFeedback();
  state.exportLoading = true;
  renderControls();

  try {
    const params = buildQueryParams();
    const maxRecords = Math.min(Math.max(Number(dom.exportLimit.value) || 5000, 100), 20000);
    params.set("max_records", String(maxRecords));

    const data = await api("export", params);
    const headers = ["时间", "来源 ID", "来源名称", "行为", "目标 ID", "目标名称", "X", "Y", "Z", "维度", "距离", "状态", "详情"];
    const rows = [headers.map(csvEscape).join(",")];

    for (const row of data.data || []) {
      rows.push([
        formatTime(row.time),
        row.id,
        row.name,
        actionLabel(row.type),
        row.obj_id,
        row.obj_name,
        row.pos_x,
        row.pos_y,
        row.pos_z,
        row.world,
        row.distance,
        row.status,
        row.data
      ].map(csvEscape).join(","));
    }

    const blob = new Blob([`\ufeff${rows.join("\n")}`], { type: "text/csv;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = `tianyan_export_${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.csv`;
    document.body.append(link);
    link.click();
    link.remove();
    URL.revokeObjectURL(url);
    setFeedback(`导出完成：${data.exported_records} 条`);
  } catch (error) {
    setFeedback(error.message || "导出失败", "error");
  } finally {
    state.exportLoading = false;
    renderControls();
  }
}

function resetFilters() {
  state.preset = "recent_activity";
  dom.playerName.value = "";
  dom.actionType.value = "";
  dom.targetName.value = "";
  dom.targetId.value = "";
  dom.world.value = "all";
  dom.centerX.value = "";
  dom.centerY.value = "";
  dom.centerZ.value = "";
  dom.radius.value = "";
  dom.exportLimit.value = "5000";
  applyQuickRange(12);
  clearResults();
  renderPresets();
  renderActiveFilters();
}

function bindEvents() {
  dom.runQuery.addEventListener("click", () => runQuery(false));
  dom.loadMore.addEventListener("click", () => runQuery(true));
  dom.exportCsv.addEventListener("click", exportCsv);
  dom.clearResults.addEventListener("click", clearResults);
  dom.resetFilters.addEventListener("click", resetFilters);
  dom.refreshStats.addEventListener("click", refreshStats);

  for (const element of [dom.playerName, dom.actionType, dom.targetName, dom.targetId, dom.world, dom.centerX, dom.centerY, dom.centerZ, dom.radius]) {
    element.addEventListener("input", renderActiveFilters);
    element.addEventListener("change", renderActiveFilters);
  }

  dom.playerName.addEventListener("input", () => {
    clearTimeout(playerTimer);
    const value = dom.playerName.value.trim();
    if (!value) {
      dom.playerSuggestions.innerHTML = "";
      return;
    }

    playerTimer = setTimeout(async () => {
      try {
        const params = new URLSearchParams({ q: value, limit: "20" });
        const data = await api("players", params);
        dom.playerSuggestions.innerHTML = "";
        for (const player of data.players || []) {
          const option = document.createElement("option");
          option.value = player;
          dom.playerSuggestions.append(option);
        }
      } catch (_) {
        dom.playerSuggestions.innerHTML = "";
      }
    }, 180);
  });
}

async function init() {
  bindEvents();
  renderPresets();
  renderRanges();
  renderSelects();
  applyQuickRange(12);
  renderResults();
  await checkHealth();

  try {
    await loadOptions();
  } catch (error) {
    setFeedback(error.message || "选项加载失败", "warning");
  }
}

init();
