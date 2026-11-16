/**
 * Kitchen System (KDS) JavaScript
 */

let isRequestPending = false;

async function fetchKitchenOrders() {
  if (isRequestPending) return;
  isRequestPending = true;

  try {
    const data = await apiFetch('/api/orders/active');
    if (data) {
      renderBoard(data);
    }
  } catch (err) {
    console.error('Kitchen polling failed:', err);
  } finally {
    isRequestPending = false;
  }
}

function renderBoard(data) {
  const waitingCards = document.getElementById('waiting-cards-container');
  const preparingCards = document.getElementById('preparing-cards-container');
  const readyCards = document.getElementById('ready-cards-container');

  const waitingList = data.waiting || [];
  const preparingList = data.preparing || [];
  const readyList = data.ready || [];

  document.getElementById('count-waiting').textContent = waitingList.length;
  document.getElementById('count-preparing').textContent = preparingList.length;
  document.getElementById('count-ready').textContent = readyList.length;

  // 1. Render Waiting Queue
  if (waitingList.length === 0) {
    waitingCards.innerHTML = '<div class="empty-state">No orders in queue.</div>';
  } else {
    waitingCards.innerHTML = waitingList.map(ord => {
      const isPriority = ord.orderType === 'PRIORITY';
      return `
        <div class="order-card ${isPriority ? 'priority' : ''}">
          <div class="order-card-top">
            <div>
              ${renderTokenBadge(ord.token, isPriority)}
              <div style="font-weight: 700; font-size: 14px; margin-top: 6px;">${escapeHtml(ord.customerName)}</div>
              <div class="order-meta-info">Placed: ${formatDateTime(ord.createdAt)}</div>
            </div>
            <div>
              ${renderTypeBadge(ord.orderType)}
            </div>
          </div>

          <div class="items-box">
            ${(ord.items || []).map(it => `
              <div class="item-line">
                <span><span class="qty">${it.quantity}x</span> ${escapeHtml(it.itemName)}</span>
              </div>
            `).join('')}
          </div>

          <div class="card-actions">
            <button class="btn btn-primary btn-sm" style="width: 100%;" onclick="startOrder('${ord.token}')">
              <svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><polygon points="5 3 19 12 5 21 5 3"/></svg>
              Start Preparing
            </button>
          </div>
        </div>
      `;
    }).join('');
  }

  // 2. Render Preparing
  if (preparingList.length === 0) {
    preparingCards.innerHTML = '<div class="empty-state">No orders currently cooking.</div>';
  } else {
    preparingCards.innerHTML = preparingList.map(ord => {
      const isPriority = ord.orderType === 'PRIORITY';
      return `
        <div class="order-card ${isPriority ? 'priority' : ''}">
          <div class="order-card-top">
            <div>
              ${renderTokenBadge(ord.token, isPriority)}
              <div style="font-weight: 700; font-size: 14px; margin-top: 6px;">${escapeHtml(ord.customerName)}</div>
              <div class="order-meta-info">Started: ${formatDateTime(ord.updatedAt || ord.createdAt)}</div>
            </div>
            <div>
              ${renderTypeBadge(ord.orderType)}
            </div>
          </div>

          <div class="items-box">
            ${(ord.items || []).map(it => `
              <div class="item-line">
                <span><span class="qty">${it.quantity}x</span> ${escapeHtml(it.itemName)}</span>
              </div>
            `).join('')}
          </div>

          <div class="card-actions">
            <button class="btn btn-success btn-sm" style="width: 100%;" onclick="markReady('${ord.token}')">
              <svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"/><path d="M13.73 21a2 2 0 0 1-3.46 0"/></svg>
              Mark as Ready
            </button>
          </div>
        </div>
      `;
    }).join('');
  }

  // 3. Render Ready
  if (readyList.length === 0) {
    readyCards.innerHTML = '<div class="empty-state">No orders awaiting pickup.</div>';
  } else {
    readyCards.innerHTML = readyList.map(ord => {
      const isPriority = ord.orderType === 'PRIORITY';
      return `
        <div class="order-card ${isPriority ? 'priority' : ''}">
          <div class="order-card-top">
            <div>
              ${renderTokenBadge(ord.token, isPriority)}
              <div style="font-weight: 700; font-size: 14px; margin-top: 6px;">${escapeHtml(ord.customerName)}</div>
              <div class="order-meta-info">Ready: ${formatDateTime(ord.updatedAt)}</div>
            </div>
            <div>
              ${renderTypeBadge(ord.orderType)}
            </div>
          </div>

          <div class="items-box">
            ${(ord.items || []).map(it => `
              <div class="item-line">
                <span><span class="qty">${it.quantity}x</span> ${escapeHtml(it.itemName)}</span>
              </div>
            `).join('')}
          </div>

          <div class="card-actions">
            <button class="btn btn-secondary btn-sm" style="width: 100%;" onclick="completeOrder('${ord.token}')">
              <svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"/></svg>
              Complete & Collected
            </button>
          </div>
        </div>
      `;
    }).join('');
  }
}

async function startOrder(token) {
  try {
    const res = await apiFetch(`/api/orders/${encodeURIComponent(token)}/start`, { method: 'POST' });
    if (res.success) {
      showToast(`Order ${token} moved to PREPARING!`, 'success');
      fetchKitchenOrders();
    } else {
      showToast(res.error || 'Failed to start order', 'error');
    }
  } catch (err) {
    showToast('Error: ' + err.message, 'error');
  }
}

async function markReady(token) {
  try {
    const res = await apiFetch(`/api/orders/${encodeURIComponent(token)}/ready`, { method: 'POST' });
    if (res.success) {
      showToast(`Order ${token} marked READY!`, 'success');
      fetchKitchenOrders();
    } else {
      showToast(res.error || 'Failed to mark ready', 'error');
    }
  } catch (err) {
    showToast('Error: ' + err.message, 'error');
  }
}

async function completeOrder(token) {
  try {
    const res = await apiFetch(`/api/orders/${encodeURIComponent(token)}/complete`, { method: 'POST' });
    if (res.success) {
      showToast(`Order ${token} COMPLETED and archived!`, 'success');
      fetchKitchenOrders();
    } else {
      showToast(res.error || 'Failed to complete order', 'error');
    }
  } catch (err) {
    showToast('Error: ' + err.message, 'error');
  }
}

async function callNextOrder() {
  const btn = document.getElementById('btn-call-next');
  btn.disabled = true;

  try {
    const nextData = await apiFetch('/api/orders/next');
    if (nextData && nextData.hasNext && nextData.order) {
      const ord = nextData.order;
      // Start this order
      await startOrder(ord.token);
    } else {
      showToast('No waiting orders in queue.', 'info');
    }
  } catch (err) {
    showToast('Failed to call next order: ' + err.message, 'error');
  } finally {
    btn.disabled = false;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  fetchKitchenOrders();
  // Auto-refresh every 2.5 seconds as required
  setInterval(fetchKitchenOrders, 2500);
});
