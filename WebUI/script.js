/**
 * TitanEye Console Script
 * 基于原生 JS (ES6+) 实现，无依赖
 */

// --- 全局状态管理 ---
const state = {
    token: localStorage.getItem('ty_secret') || '',
    apiUrl: localStorage.getItem('ty_api_base') || 'http://127.0.0.1:8098',
    currentPage: 1,
    totalPages: 1,
    totalRecords: 0,
    pageSize: 100,
    logs: [],
    currentLang: localStorage.getItem('ty_lang') || 'zh_CN',
    langData: {}
};

// --- DOM 快捷选择器 ---
const $ = (selector) => document.querySelector(selector);
const show = (el) => el.style.display = 'flex';
const hide = (el) => el.style.display = 'none';

// --- UI 工具函数 ---

function toast(msg, type = 'success') {
    const container = $('#toast-container');
    const el = document.createElement('div');
    el.className = `toast ${type}`;
    el.innerHTML = `<span>${msg}</span><span style="cursor:pointer;margin-left:10px" onclick="this.parentElement.remove()">✕</span>`;
    container.appendChild(el);
    setTimeout(() => {
        el.style.opacity = '0';
        setTimeout(() => el.remove(), 300);
    }, 3000);
}

function setLoading(isLoading, text = '加载中...') {
    const overlay = $('#loading-overlay');
    $('#loading-text').innerText = text;
    overlay.style.display = isLoading ? 'flex' : 'none';
}

// --- 语言处理函数 ---

async function loadAvailableLanguages() {
    try {
        const response = await fetch(`${state.apiUrl}/api/languages`);
        const data = await response.json();
        return data.languages;
    } catch (error) {
        console.error("Failed to load languages:", error);
        return ['en_US', 'zh_CN']; // 默认语言列表
    }
}

async function loadLanguage(langCode) {
    try {
        const response = await fetch(`${state.apiUrl}/api/language/${langCode}`);
        if (!response.ok) {
            throw new Error(`Failed to load language: ${langCode}`);
        }
        const data = await response.json();
        state.langData = data;
        state.currentLang = langCode;
        localStorage.setItem('ty_lang', langCode);
        applyTranslations();

        // 更新语言选择器的选中状态
        updateLanguageSelectors();

        // 设置整个文档的语言属性，影响日期选择器等浏览器组件
        document.documentElement.lang = langCode;

        return true;
    } catch (error) {
        console.error("Failed to load language file:", error);
        // 尝试加载默认语言
        if (langCode !== 'en_US') {
            return await loadLanguage('en_US');
        }
        return false;
    }
}

function updateLanguageSelectors() {
    // 更新所有语言选择器的值
    const selectors = ['#language-select-login', '#language-select-main'];
    selectors.forEach(selector => {
        const el = $(selector);
        if (el) {
            el.value = state.currentLang;
        }
    });
}

function applyTranslations() {
    // 翻译所有带有data-i18n属性的元素
    document.querySelectorAll('[data-i18n]').forEach(element => {
        const key = element.getAttribute('data-i18n');
        if (state.langData[key]) {
            // 特殊处理：对于select元素，只更新其option的文本，不改变值
            if (element.tagName === 'SELECT') {
                // 保存当前选中的值
                const currentValue = element.value;
                // 更新所有option的文本
                Array.from(element.options).forEach(option => {
                    const optionKey = option.getAttribute('data-i18n');
                    if (optionKey && state.langData[optionKey]) {
                        option.textContent = state.langData[optionKey];
                    }
                });
                // 恢复选中的值
                element.value = currentValue;
            } else {
                // 其他元素直接更新文本
                element.textContent = state.langData[key];
            }
        }
    });

    // 翻译placeholder
    document.querySelectorAll('[data-placeholder-i18n]').forEach(element => {
        const key = element.getAttribute('data-placeholder-i18n');
        if (state.langData[key]) {
            element.placeholder = state.langData[key];
        }
    });

    // 更新页面标题
    if (state.langData['page_title']) {
        document.title = state.langData['page_title'];
    }

    // 更新分页信息
    updatePageInfo();
}

function updatePageInfo() {
    if (!state.totalRecords && state.totalRecords !== 0) return;

    const start = (state.currentPage - 1) * state.pageSize + 1;
    const end = Math.min(state.currentPage * state.pageSize, state.totalRecords);
    const displayEnd = state.totalRecords === 0 ? 0 : end;
    const displayStart = state.totalRecords === 0 ? 0 : start;

    const pageInfoText = state.langData['page_info'] || '显示第 {start} - {end} 条，共 {total} 条';
    const formattedText = pageInfoText
        .replace('{start}', displayStart.toLocaleString())
        .replace('{end}', displayEnd.toLocaleString())
        .replace('{total}', state.totalRecords.toLocaleString());

    $('#page-info').innerText = formattedText;
    $('#page-total').innerText = state.totalPages;
}

// --- API 通信核心 ---

async function apiCall(endpoint, params = {}) {
    const url = new URL(`${state.apiUrl}/api${endpoint}`);
    if (params) {
        Object.keys(params).forEach(key => {
            if (params[key] !== null && params[key] !== '' && params[key] !== undefined) {
                url.searchParams.append(key, params[key]);
            }
        });
    }

    try {
        const response = await fetch(url, {
            headers: {
                'X-Secret': state.token,
                'X-Lang': state.currentLang
            }
        });

        if (response.status === 403) {
            toast(state.langData['auth_failed'] || '密钥错误或未登录', 'error');
            handleLogout();
            throw new Error('Auth failed');
        }

        if (!response.ok) {
            const err = await response.json();
            throw new Error(err.detail || (state.langData['request_failed'] || '请求失败'));
        }

        return await response.json();
    } catch (error) {
        console.error("API Error:", error);
        if (error.message !== 'Auth failed') {
            toast(error.message, 'error');
        }
        throw error;
    }
}

// --- 业务逻辑 ---

async function handleLogin() {
    const urlRaw = $('#api-url').value;
    const url = urlRaw.replace(/\/$/, '');
    const secret = $('#api-secret').value;

    if (!url || !secret) {
        toast(state.langData['fill_all_info'] || '请填写完整信息', 'error');
        return;
    }

    state.apiUrl = url;
    state.token = secret;
    setLoading(true, state.langData['connecting'] || '连接服务器中...');

    try {
        const stats = await apiCall('/stats');
        localStorage.setItem('ty_api_base', state.apiUrl);
        localStorage.setItem('ty_secret', state.token);

        $('#login-view').style.display = 'none';
        $('#main-view').style.display = 'flex';
        updateStatsUI(stats);
        toast(state.langData['login_success'] || '登录成功');

        // 登录后尝试加载一次数据
        fetchLogs(1);

    } catch (e) {
        console.error("Login error:", e);
    } finally {
        setLoading(false);
    }
}

function handleLogout() {
    localStorage.removeItem('ty_secret');
    state.token = '';
    $('#main-view').style.display = 'none';
    $('#login-view').style.display = 'flex';
}

async function fetchStats() {
    setLoading(true, state.langData['refreshing_stats'] || '刷新状态...');
    try {
        const data = await apiCall('/stats');
        updateStatsUI(data);
        toast(state.langData['stats_refreshed'] || '状态已刷新');
    } catch(e) {
        console.error("Failed to fetch stats:", e);
    } finally {
        setLoading(false);
    }
}

function updateStatsUI(data) {
    $('#stat-total').innerText = data.total_logs ? data.total_logs.toLocaleString() : '--';
    $('#stat-size').innerText = data.db_size || '--';
    if (data.query_time_ms) {
        $('#stat-time').innerText = data.query_time_ms + ' ms';
    }
}

function getQueryParams() {
    const startVal = $('#filter-start').value;
    const endVal = $('#filter-end').value;
    const startTime = startVal ? Math.floor(new Date(startVal).getTime() / 1000) : null;
    const endTime = endVal ? Math.floor(new Date(endVal).getTime() / 1000) : null;

    const x = $('#coord-x').value;
    const y = $('#coord-y').value;
    const z = $('#coord-z').value;
    const r = $('#coord-r').value;
    let hasCoords = (x!=='' && y!=='' && z!=='' && r!=='');

    let dim = $('#coord-dim').value;
    if (dim === 'custom') dim = $('#coord-dim-custom').value;
    if (dim === 'all') dim = null;

    return {
        page_size: parseInt($('#page-size').value),
        filter_type: $('#filter-type').value,
        filter_value: $('#filter-value').value,
        start_time: startTime,
        end_time: endTime,
        center_x: hasCoords ? parseFloat(x) : null,
        center_y: hasCoords ? parseFloat(y) : null,
        center_z: hasCoords ? parseFloat(z) : null,
        radius: hasCoords ? parseFloat(r) : null,
        dimension: dim
    };
}

async function fetchLogs(page) {
    setLoading(true, state.langData['querying_data'] || '查询数据中...');
    state.currentPage = page;
    const params = { ...getQueryParams(), page: page };

    try {
        const res = await apiCall('/logs', params);
        state.logs = res.data;
        state.totalRecords = res.total;
        state.totalPages = res.total_pages;

        if(res.query_time_ms) $('#stat-time').innerText = res.query_time_ms + ' ms';

        renderTable(res.data, params.center_x !== null);
        renderPagination();

        if (res.data.length === 0) {
            $('#empty-state').style.display = 'block';
            $('#logs-table').style.display = 'none';
        } else {
            $('#empty-state').style.display = 'none';
            $('#logs-table').style.display = 'table';
        }

    } catch (e) {
        console.error("Failed to fetch logs:", e);
    } finally {
        setLoading(false);
    }
}

function renderTable(logs, showDistance) {
    const tbody = $('#logs-body');
    tbody.innerHTML = '';
    const thDist = $('#th-dist');
    thDist.style.display = showDistance ? 'table-cell' : 'none';

    logs.forEach(log => {
        const tr = document.createElement('tr');
        const timeStr = new Date(log.time * 1000).toLocaleString(state.currentLang.replace('_', '-'));
        const distStr = log.distance ? `${parseFloat(log.distance).toFixed(1)}m` : '-';
        const distCell = showDistance ? `<td class="dist-cell">${distStr}</td>` : '';

        const sourceHtml = `
            <div class="cell-content">${log.name || '-'}</div>
            <div class="sub-text">${log.id || ''}</div>
        `;
        const targetHtml = `
            <div class="cell-content">${log.obj_name || '-'}</div>
            <div class="sub-text">${log.obj_id || ''}</div>
        `;

        tr.innerHTML = `
            <td style="color:#666;">${timeStr}</td>
            <td>${sourceHtml}</td>
            <td><span class="tag tag-action">${log.type}</span></td>
            <td>${targetHtml}</td>
            <td>
                <div class="coord-cell" style="display:flex;align-items:center;gap:4px;">
                    <span>${log.pos_x.toFixed(1)}, ${log.pos_y.toFixed(1)}, ${log.pos_z.toFixed(1)}</span>
                    <span class="tag tag-world">${log.world}</span>
                </div>
            </td>
            ${distCell}
            <td class="data-json">${log.data || ''}</td>
        `;
        tbody.appendChild(tr);
    });
}

function renderPagination() {
    $('#page-curr').innerText = state.currentPage;
    $('#page-total').innerText = state.totalPages;
    $('#btn-prev').disabled = state.currentPage <= 1;
    $('#btn-next').disabled = state.currentPage >= state.totalPages;

    // 重置跳转页输入框
    $('#jump-page-input').value = '';

    updatePageInfo();
}

function changePage(delta) {
    const newPage = state.currentPage + delta;
    if (newPage >= 1 && newPage <= state.totalPages) fetchLogs(newPage);
}

function jumpToPage() {
    const input = $('#jump-page-input');
    const pageStr = input.value.trim();

    if (!pageStr) {
        toast(state.langData['enter_page_number'] || '请输入页码', 'error');
        input.focus();
        return;
    }

    const page = parseInt(pageStr);

    if (isNaN(page) || page < 1) {
        toast(state.langData['invalid_page_number'] || '请输入有效的页码', 'error');
        input.value = '';
        input.focus();
        return;
    }

    if (page > state.totalPages) {
        toast((state.langData['page_exceeds_max'] || '页码不能超过最大页数 {max}').replace('{max}', state.totalPages), 'error');
        input.value = '';
        input.focus();
        return;
    }

    fetchLogs(page);
}

function resetFilters() {
    $('#filter-start').value = '';
    $('#filter-end').value = '';
    $('#filter-type').value = '';
    $('#filter-value').value = '';
    $('#coord-x').value = '';
    $('#coord-y').value = '';
    $('#coord-z').value = '';
    $('#coord-r').value = '10';
    $('#coord-dim').value = 'all';
    $('#coord-dim-custom').style.display = 'none';
    $('#coord-dim-custom').value = '';
    toast(state.langData['filters_reset'] || '筛选条件已重置');
}

async function showDbInfo() {
    setLoading(true);
    try {
        const [info, stats] = await Promise.all([
            apiCall('/db_info'),
            apiCall('/stats')
        ]);

        const msg = `
${state.langData['db_size'] || '数据库大小'}: ${stats.db_size}
-------------------------
${state.langData['table_count'] || '表数量'}: ${info.tables?.length || 0}
${state.langData['index_count'] || '索引数量'}: ${info.indexes?.length || 0}
${state.langData['column_count'] || '字段数量'}: ${info.columns?.length || 0}
${state.langData['total_rows'] || '总行数 (预估)'}: ${info.total_rows || 0}
        `;
        alert(msg);
    } catch(e) {
        console.error("Failed to fetch DB info:", e);
    } finally {
        setLoading(false);
    }
}

async function exportData() {
    if (state.totalRecords === 0) {
        toast(state.langData['no_data_to_export'] || '没有数据可导出', 'error');
        return;
    }

    if (!confirm(state.langData['confirm_export'] || '确定要导出当前筛选条件下的数据吗？')) return;

    setLoading(true, state.langData['generating_csv'] || '正在生成CSV...');
    const params = { ...getQueryParams(), start_page: 1, end_page: 500 };

    try {
        const res = await apiCall('/export', params);
        if (res.data && res.data.length > 0) {
            const headers = ["Time", "SourceID", "SourceName", "Type", "TargetID", "TargetName", "World", "X", "Y", "Z", "Data"];
            const rows = res.data.map(r => [
                new Date(r.time * 1000).toLocaleString(state.currentLang.replace('_', '-')),
                r.id, r.name, r.type, r.obj_id, r.obj_name, r.world,
                r.pos_x, r.pos_y, r.pos_z,
                `"${(r.data||'').replace(/"/g, '""')}"`
            ]);

            const csvContent = "\ufeff" + [headers.join(","), ...rows.map(e => e.join(","))].join("\n");
            const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
            const link = document.createElement("a");
            link.href = URL.createObjectURL(blob);
            link.download = `tianyan_export_${new Date().getTime()}.csv`;
            link.click();
            toast((state.langData['export_success'] || '成功导出 {count} 条记录').replace('{count}', res.data.length.toLocaleString()));
        } else {
            toast(state.langData['no_data_returned'] || '服务端未返回数据', 'error');
        }
    } catch (e) {
        console.error("Export error:", e);
    } finally {
        setLoading(false);
    }
}

// 初始化语言选择器
async function initLanguageSelectors(languages) {
    const selectLogin = $('#language-select-login');
    const selectMain = $('#language-select-main');

    if (!selectLogin || !selectMain) return;

    // 清空现有选项
    selectLogin.innerHTML = '';
    selectMain.innerHTML = '';

    // 添加语言选项
    languages.forEach(lang => {
        const optionLogin = document.createElement('option');
        optionLogin.value = lang;

        // 根据语言代码显示友好的名称
        let displayName = lang;
        if (lang === 'en_US') displayName = 'English';
        if (lang === 'zh_CN') displayName = '中文';
        if (lang === 'ru_RU') displayName = 'Русский';

        optionLogin.textContent = displayName;
        selectLogin.appendChild(optionLogin);

        const optionMain = optionLogin.cloneNode(true);
        selectMain.appendChild(optionMain);
    });

    // 设置当前语言
    updateLanguageSelectors();

    // 添加事件监听器
    selectLogin.addEventListener('change', async (e) => {
        await loadLanguage(e.target.value);
    });

    selectMain.addEventListener('change', async (e) => {
        await loadLanguage(e.target.value);
    });
}

// 自动登录函数
async function autoLogin() {
    if (!state.token) return false;

    setLoading(true, state.langData['connecting'] || '连接服务器中...');
    try {
        const stats = await apiCall('/stats');

        $('#login-view').style.display = 'none';
        $('#main-view').style.display = 'flex';
        updateStatsUI(stats);

        // 加载数据
        fetchLogs(1);
        return true;
    } catch (e) {
        console.error("Auto login failed:", e);
        // 如果自动登录失败，清除token
        localStorage.removeItem('ty_secret');
        state.token = '';
        return false;
    } finally {
        setLoading(false);
    }
}

// 初始化函数
async function init() {
    // 设置初始显示状态
    $('#login-view').style.display = 'flex';
    $('#main-view').style.display = 'none';

    // 设置表单值
    $('#api-url').value = state.apiUrl;
    $('#api-secret').value = state.token;

    // 获取可用语言列表
    const languages = await loadAvailableLanguages();
    await initLanguageSelectors(languages);

    // 加载当前语言
    await loadLanguage(state.currentLang);

    // 尝试自动登录
    if (state.token) {
        await autoLogin();
    }

    // 设置事件监听器
    $('#btn-login').addEventListener('click', handleLogin);
    $('#btn-logout').addEventListener('click', handleLogout);
    $('#btn-refresh-stats').addEventListener('click', fetchStats);
    $('#btn-search').addEventListener('click', () => fetchLogs(1));
    $('#btn-reset').addEventListener('click', resetFilters);
    $('#btn-export').addEventListener('click', exportData);
    $('#btn-prev').addEventListener('click', () => changePage(-1));
    $('#btn-next').addEventListener('click', () => changePage(1));
    $('#btn-db-info').addEventListener('click', showDbInfo);
    $('#btn-jump-page').addEventListener('click', jumpToPage);

    // 跳转页输入框支持回车键
    $('#jump-page-input').addEventListener('keyup', (e) => {
        if (e.key === 'Enter') jumpToPage();
    });

    $('#filter-value').addEventListener('keyup', (e) => {
        if (e.key === 'Enter') fetchLogs(1);
    });

    $('#page-size').addEventListener('change', () => {
        state.pageSize = parseInt($('#page-size').value);
        fetchLogs(1);
    });

    $('#coord-dim').addEventListener('change', (e) => {
        $('#coord-dim-custom').style.display = e.target.value === 'custom' ? 'block' : 'none';
    });
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', init);