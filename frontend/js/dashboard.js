/**
 * Dashboard JavaScript logic
 */

async function loadDashboard() {
  try {
    const data = await apiFetch('/api/dashboard');
    if (!data) return;

    // 1. Update Aggregate Metrics
    const stats = data.stats || {};
    document.getElementById('stat-total').textContent = stats.totalOrders || 0;
    document.getElementById('stat-waiting').textContent = stats.waitingOrders || 0;
    document.getElementById('stat-preparing').textContent = stats.preparingOrders || 0;
    document.getElementById('stat-ready').textContent = stats.readyOrders || 0;
    document.getElementById('stat-revenue').textContent = formatCurrency(stats.totalRevenue || 0);

    // 2. Load Active Queues (Waiting, Preparing, Ready)
    const activeData = await apiFetch('/api/orders/active');
    renderActiveQueues(activeData);

    // 3. Render Recent Orders
    renderRecentOrders(data.recentOrders || []);

  } catch (error) {
    console.error('Failed to load dashboard data:', error);
  }
}

function renderActiveQueues(activeData) {
  if (!activeData) return;

  const waitingListEl = document.getElementById('queue-waiting-list');
  const preparingListEl = document.getElementById('queue-preparing-list');
  const readyListEl = document.getElementById('queue-ready-list');

  const counts = activeData.counts || {};
  document.getElementById('badge-waiting-count').textContent = `${counts.totalWaiting || 0} In Queue`;
  document.getElementById('badge-preparing-count').textContent = `${counts.preparing || 0} Cooking`;
  document.getElementById('badge-ready-count').textContent = `${counts.ready || 0} Ready`;

  // Render Waiting List
  const waiting = activeData.waiting || [];
  if (waiting.length === 0) {
    waitingListEl.innerHTML = '<div class="empty-state">No orders currently waiting in queue.</div>';
  } else {
    waitingListEl.innerHTML = waiting.map((ord, idx) => {
      const isPriority = ord.orderType === 'PRIORITY';
      const itemsCount = (ord.items || []).reduce((sum, it) => sum + it.quantity, 0);
      return `
        <div class="mini-order-card ${isPriority ? 'priority' : ''}">
          <div>
            <div style="display: flex; align-items: center; gap: 8px;">
              ${renderTokenBadge(ord.token, isPriority)}
              <span style="font-weight: 600; font-size: 13px;">${escapeHtml(ord.customerName)}</span>
            </div>
            <div style="font-size: 12px; color: var(--text-muted); margin-top: 4px;">
              ${itemsCount} item(s) • ${formatCurrency(ord.totalAmount)}
            </div>
          </div>
          <div style="text-align: right;">
            ${renderTypeBadge(ord.orderType)}
            <div style="font-size: 11px; color: var(--text-muted); margin-top: 4px;">#${idx + 1} in line</div>
          </div>
        </div>
      `;
    }).join('');
  }

  // Render Preparing List
  const preparing = activeData.preparing || [];
  if (preparing.length === 0) {
    preparingListEl.innerHTML = '<div class="empty-state">Kitchen is idle. No orders preparing.</div>';
  } else {
    preparingListEl.innerHTML = preparing.map(ord => {
      const isPriority = ord.orderType === 'PRIORITY';
      const itemsSummary = (ord.items || []).map(it => `${it.quantity}x ${it.itemName}`).join(', ');
      return `
        <div class="mini-order-card ${isPriority ? 'priority' : ''}">
          <div>
            <div style="display: flex; align-items: center; gap: 8px;">
              ${renderTokenBadge(ord.token, isPriority)}
              <span style="font-weight: 600; font-size: 13px;">${escapeHtml(ord.customerName)}</span>
            </div>
            <div style="font-size: 11px; color: var(--text-muted); margin-top: 4px; max-width: 200px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
              ${escapeHtml(itemsSummary)}
            </div>
          </div>
          <div>
            ${renderStatusBadge('PREPARING')}
          </div>
        </div>
      `;
    }).join('');
  }

  // Render Ready List
  const ready = activeData.ready || [];
  if (ready.length === 0) {
    readyListEl.innerHTML = '<div class="empty-state">No orders waiting for customer pickup.</div>';
  } else {
    readyListEl.innerHTML = ready.map(ord => {
      const isPriority = ord.orderType === 'PRIORITY';
      return `
        <div class="mini-order-card ${isPriority ? 'priority' : ''}">
          <div>
            <div style="display: flex; align-items: center; gap: 8px;">
              ${renderTokenBadge(ord.token, isPriority)}
              <span style="font-weight: 600; font-size: 13px;">${escapeHtml(ord.customerName)}</span>
            </div>
            <div style="font-size: 12px; color: var(--text-muted); margin-top: 4px;">
              Total: ${formatCurrency(ord.totalAmount)}
            </div>
          </div>
          <div>
            ${renderStatusBadge('READY')}
          </div>
        </div>
      `;
    }).join('');
  }
}

function renderRecentOrders(recentOrders) {
  const tbody = document.getElementById('recent-orders-tbody');
  if (!tbody) return;

  if (recentOrders.length === 0) {
    tbody.innerHTML = '<tr><td colspan="7" class="empty-state">No orders placed yet.</td></tr>';
    return;
  }

  tbody.innerHTML = recentOrders.map(ord => {
    const isPriority = ord.orderType === 'PRIORITY';
    const itemsSummary = (ord.items || []).map(it => `${it.quantity}x ${it.itemName}`).join(', ');
    return `
      <tr>
        <td>${renderTokenBadge(ord.token, isPriority)}</td>
        <td><strong>${escapeHtml(ord.customerName)}</strong></td>
        <td>${escapeHtml(itemsSummary || 'No items')}</td>
        <td>${renderTypeBadge(ord.orderType)}</td>
        <td><strong>${formatCurrency(ord.totalAmount)}</strong></td>
        <td>${renderStatusBadge(ord.status)}</td>
        <td style="color: var(--text-muted); font-size: 12px;">${formatDateTime(ord.createdAt)}</td>
      </tr>
    `;
  }).join('');
}

document.addEventListener('DOMContentLoaded', () => {
  loadDashboard();
  // Auto-refresh dashboard every 4 seconds
  setInterval(loadDashboard, 4000);
});
