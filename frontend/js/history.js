/**
 * Order History & Token Lookup JavaScript
 */

let cachedOrders = [];

async function loadHistory(search = '') {
  try {
    const url = search ? `/api/orders/history?search=${encodeURIComponent(search)}` : '/api/orders/history';
    const data = await apiFetch(url);
    cachedOrders = (data && data.orders) ? data.orders : [];
    renderHistoryTable(cachedOrders);
  } catch (err) {
    showToast('Failed to load order history: ' + err.message, 'error');
  }
}

function renderHistoryTable(orders) {
  const tbody = document.getElementById('history-tbody');
  const countBadge = document.getElementById('history-count-badge');
  if (!tbody) return;

  if (countBadge) {
    countBadge.textContent = `${orders.length} Records`;
  }

  if (orders.length === 0) {
    tbody.innerHTML = '<tr><td colspan="9" class="empty-state">No matching orders found.</td></tr>';
    return;
  }

  tbody.innerHTML = orders.map(ord => {
    const isPriority = ord.orderType === 'PRIORITY';
    const itemsSummary = (ord.items || []).map(it => `${it.quantity}x ${it.itemName}`).join(', ');
    const completedTime = (ord.status === 'COMPLETED') ? formatDateTime(ord.updatedAt) : '--';

    return `
      <tr>
        <td>${renderTokenBadge(ord.token, isPriority)}</td>
        <td><strong>${escapeHtml(ord.customerName)}</strong></td>
        <td style="max-width: 250px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${escapeHtml(itemsSummary)}">
          ${escapeHtml(itemsSummary || 'No items')}
        </td>
        <td>${renderTypeBadge(ord.orderType)}</td>
        <td><strong>${formatCurrency(ord.totalAmount)}</strong></td>
        <td>${renderStatusBadge(ord.status)}</td>
        <td style="font-size: 12px; color: var(--text-muted);">${formatDateTime(ord.createdAt)}</td>
        <td style="font-size: 12px; color: var(--text-muted);">${completedTime}</td>
        <td>
          <div style="display: flex; gap: 6px; align-items: center;">
            <button class="btn btn-secondary btn-sm" onclick="viewOrderDetails('${ord.token}')" title="View Order Details">
              View
            </button>
            <button class="btn btn-danger-outline btn-sm" onclick="promptDeleteOrder(${ord.id}, '${ord.token}')" title="Delete Order ${ord.token}">
              <svg class="icon-svg icon-svg-xs" viewBox="0 0 24 24"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>
              Delete
            </button>
          </div>
        </td>
      </tr>
    `;
  }).join('');
}

async function searchByToken() {
  const query = document.getElementById('token-search-input').value.trim();
  if (!query) {
    loadHistory();
    hideSpotlight();
    return;
  }

  // First try direct C++ Hash Table lookup if formatted like a token
  if (/^[NnPptT]-\d+$/.test(query)) {
    try {
      const res = await apiFetch(`/api/orders/${encodeURIComponent(query.toUpperCase())}`);
      if (res && res.success && res.order) {
        showSpotlight(res.order);
        showToast(`Token ${res.order.token} found instantly!`, 'success');
        return;
      }
    } catch (e) {
      // If not found in direct token lookup, fall back to general history search
    }
  }

  // Fall back to general search
  hideSpotlight();
  loadHistory(query);
}

function handleSearchKey(event) {
  if (event.key === 'Enter') {
    searchByToken();
  }
}

function resetSearch() {
  document.getElementById('token-search-input').value = '';
  hideSpotlight();
  loadHistory();
}

function showSpotlight(order) {
  const spotlight = document.getElementById('token-spotlight');
  const content = document.getElementById('spotlight-content');
  const isPriority = order.orderType === 'PRIORITY';

  content.innerHTML = `
    <div style="display: flex; justify-content: space-between; align-items: flex-start; flex-wrap: wrap; gap: 16px;">
      <div>
        <div style="display: flex; align-items: center; gap: 10px;">
          ${renderTokenBadge(order.token, isPriority)}
          <h4 style="font-size: 16px; font-weight: 700;">Customer: ${escapeHtml(order.customerName)}</h4>
        </div>
        <div style="font-size: 13px; color: var(--text-muted); margin-top: 6px;">
          Order Type: ${renderTypeBadge(order.orderType)} &nbsp;•&nbsp; 
          Current Status: ${renderStatusBadge(order.status)} &nbsp;•&nbsp;
          Created: ${formatDateTime(order.createdAt)}
        </div>
      </div>
      <div style="text-align: right;">
        <span style="font-size: 13px; color: var(--text-muted);">Total Amount</span>
        <div style="font-size: 22px; font-weight: 800; color: var(--primary);">${formatCurrency(order.totalAmount)}</div>
      </div>
    </div>
    <div class="details-box">
      <strong>Items in this order:</strong>
      <ul style="margin-top: 8px; padding-left: 20px; font-size: 13px;">
        ${(order.items || []).map(it => `
          <li>${it.quantity}x ${escapeHtml(it.itemName)} @ ${formatCurrency(it.unitPrice)} = ${formatCurrency(it.subtotal)}</li>
        `).join('')}
      </ul>
    </div>
  `;

  spotlight.style.display = 'block';
}

function hideSpotlight() {
  document.getElementById('token-spotlight').style.display = 'none';
}

let pendingDeleteTarget = null;
let currentViewingOrderId = null;
let currentViewingOrderToken = null;

function viewOrderDetails(token) {
  const order = cachedOrders.find(o => o.token === token);
  if (!order) return;

  currentViewingOrderId = order.id;
  currentViewingOrderToken = order.token;

  const modal = document.getElementById('details-modal');
  const body = document.getElementById('modal-details-body');
  const isPriority = order.orderType === 'PRIORITY';

  body.innerHTML = `
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px;">
      <div>
        ${renderTokenBadge(order.token, isPriority)}
        <span style="font-weight: 700; margin-left: 8px;">${escapeHtml(order.customerName)}</span>
      </div>
      <div>
        ${renderStatusBadge(order.status)}
      </div>
    </div>

    <div style="font-size: 13px; color: var(--text-muted); margin-bottom: 12px; line-height: 1.6;">
      <div><strong>Order Type:</strong> ${renderTypeBadge(order.orderType)}</div>
      <div><strong>Placed At:</strong> ${formatDateTime(order.createdAt)}</div>
      <div><strong>Last Updated:</strong> ${formatDateTime(order.updatedAt)}</div>
    </div>

    <div class="details-box">
      <div style="font-weight: 600; font-size: 13px; margin-bottom: 8px;">Itemized Receipt:</div>
      <table style="width: 100%; font-size: 13px;">
        <thead>
          <tr style="border-bottom: 1px solid var(--border-color);">
            <th style="padding: 6px 0; background: none;">Item</th>
            <th style="padding: 6px 0; background: none; text-align: center;">Qty</th>
            <th style="padding: 6px 0; background: none; text-align: right;">Price</th>
            <th style="padding: 6px 0; background: none; text-align: right;">Total</th>
          </tr>
        </thead>
        <tbody>
          ${(order.items || []).map(it => `
            <tr>
              <td style="padding: 6px 0;">${escapeHtml(it.itemName)}</td>
              <td style="padding: 6px 0; text-align: center;">${it.quantity}</td>
              <td style="padding: 6px 0; text-align: right;">${formatCurrency(it.unitPrice)}</td>
              <td style="padding: 6px 0; text-align: right; font-weight: 600;">${formatCurrency(it.subtotal)}</td>
            </tr>
          `).join('')}
        </tbody>
      </table>
      <div style="border-top: 1px dashed var(--border-color); margin-top: 10px; padding-top: 10px; display: flex; justify-content: space-between; font-weight: 700; font-size: 15px;">
        <span>Grand Total:</span>
        <span style="color: var(--primary);">${formatCurrency(order.totalAmount)}</span>
      </div>
    </div>
  `;

  modal.classList.add('active');
}

function closeDetailsModal() {
  document.getElementById('details-modal').classList.remove('active');
  currentViewingOrderId = null;
  currentViewingOrderToken = null;
}

function promptDeleteOrder(id, token) {
  pendingDeleteTarget = { type: 'single', id, token };
  document.getElementById('delete-modal-title').textContent = `Delete Order ${token}?`;
  document.getElementById('delete-modal-desc').textContent = 
    `Are you sure you want to delete order #${id} (Token: ${token})? This record will be permanently removed from history.`;
  document.getElementById('btn-confirm-delete').textContent = 'Delete Order';
  document.getElementById('delete-confirm-modal').classList.add('active');
}

function promptClearAllOrders() {
  if (cachedOrders.length === 0) {
    showToast('There are no order records in history to delete.', 'info');
    return;
  }
  pendingDeleteTarget = { type: 'all' };
  document.getElementById('delete-modal-title').textContent = 'Delete All Orders?';
  document.getElementById('delete-modal-desc').textContent = 
    `Warning: This will permanently delete all ${cachedOrders.length} order records from the database and queue history. This action cannot be undone.`;
  document.getElementById('btn-confirm-delete').textContent = 'Delete All Orders';
  document.getElementById('delete-confirm-modal').classList.add('active');
}

function closeDeleteModal() {
  document.getElementById('delete-confirm-modal').classList.remove('active');
  pendingDeleteTarget = null;
}

async function executePendingDelete() {
  if (!pendingDeleteTarget) return;

  const btn = document.getElementById('btn-confirm-delete');
  btn.disabled = true;
  btn.textContent = 'Deleting...';

  try {
    if (pendingDeleteTarget.type === 'single') {
      const res = await apiFetch(`/api/orders/${pendingDeleteTarget.id}/delete`, {
        method: 'POST',
        body: JSON.stringify({ token: pendingDeleteTarget.token })
      });
      if (res && res.success) {
        showToast(`Order ${pendingDeleteTarget.token} deleted successfully.`, 'success');
        closeDeleteModal();
        closeDetailsModal();
        hideSpotlight();
        loadHistory();
      } else {
        showToast(res?.error || 'Failed to delete order.', 'error');
      }
    } else if (pendingDeleteTarget.type === 'all') {
      const res = await apiFetch('/api/orders/clear-all', {
        method: 'POST',
        body: JSON.stringify({})
      });
      if (res && res.success) {
        showToast('All order records have been deleted.', 'success');
        closeDeleteModal();
        closeDetailsModal();
        hideSpotlight();
        loadHistory();
      } else {
        showToast(res?.error || 'Failed to clear all orders.', 'error');
      }
    }
  } catch (err) {
    showToast('Delete error: ' + err.message, 'error');
  } finally {
    btn.disabled = false;
    btn.textContent = 'Delete Permanently';
  }
}

function deleteCurrentModalOrder() {
  if (currentViewingOrderId) {
    promptDeleteOrder(currentViewingOrderId, currentViewingOrderToken);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  loadHistory();
});

