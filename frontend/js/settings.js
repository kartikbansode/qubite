/**
 * Settings & Device Management Controller (settings.js)
 */

let activeTab = 'general';
let pairingTimerInterval = null;
let selectedPairingRole = 'KITCHEN';

document.addEventListener('DOMContentLoaded', () => {
  setupTabs();
  configureTabsForRole();
  updateCurrentSessionCard();
  loadSettings();
  refreshTokenPreview();
  const role = localStorage.getItem('cafe_device_role') || 'CASHIER';
  if (role === 'CASHIER') {
    loadRevenueStats();
    loadDevicesList();
  }

  const btnTestBell = document.getElementById('btn-test-bell');
  if (btnTestBell) {
    btnTestBell.addEventListener('click', () => {
      playReadyBell(true);
      showToast('Playing ready bell sound (assets/bell.mp3)...', 'info');
    });
  }
});

function configureTabsForRole() {
  const currentRole = localStorage.getItem('cafe_device_role') || 'CASHIER';

  const tabGeneral = document.querySelector('.settings-tab-btn[data-tab="general"]');
  const tabOrders = document.querySelector('.settings-tab-btn[data-tab="orders"]');
  const tabDisplay = document.querySelector('.settings-tab-btn[data-tab="display"]');
  const tabRevenue = document.querySelector('.settings-tab-btn[data-tab="revenue"]');
  const tabDevices = document.querySelector('.settings-tab-btn[data-tab="devices"]');

  const devTableCard = document.getElementById('devices-table-card');

  if (currentRole === 'KITCHEN') {
    // Kitchen sees Display & Device Management (with Quit Session)
    if (tabGeneral) tabGeneral.style.display = 'none';
    if (tabOrders) tabOrders.style.display = 'none';
    if (tabRevenue) tabRevenue.style.display = 'none';
    if (tabDisplay) tabDisplay.style.display = '';
    if (tabDevices) tabDevices.style.display = '';
    if (devTableCard) devTableCard.style.display = 'none';

    // Switch default tab to display
    if (tabDisplay) tabDisplay.click();
  } else if (currentRole === 'TV_DISPLAY') {
    // TV Display sees Display & Device Management (with Quit Session)
    if (tabGeneral) tabGeneral.style.display = 'none';
    if (tabOrders) tabOrders.style.display = 'none';
    if (tabRevenue) tabRevenue.style.display = 'none';
    if (tabDisplay) tabDisplay.style.display = '';
    if (tabDevices) tabDevices.style.display = '';
    if (devTableCard) devTableCard.style.display = 'none';

    if (tabDisplay) tabDisplay.click();
  } else {
    // Cashier sees ALL tabs
    if (tabGeneral) tabGeneral.style.display = '';
    if (tabOrders) tabOrders.style.display = '';
    if (tabDisplay) tabDisplay.style.display = '';
    if (tabRevenue) tabRevenue.style.display = '';
    if (tabDevices) tabDevices.style.display = '';
    if (devTableCard) devTableCard.style.display = '';
  }
}

function updateCurrentSessionCard() {
  const role = localStorage.getItem('cafe_device_role') || 'CASHIER';
  const name = localStorage.getItem('cafe_device_name') || ((role === 'CASHIER') ? 'Master Cashier POS' : (role === 'KITCHEN' ? 'Kitchen Screen' : 'TV Display Screen'));

  const nameEl = document.getElementById('session-device-name');
  const badgeEl = document.getElementById('session-device-role-badge');
  const statusEl = document.getElementById('session-device-status-text');

  if (nameEl) nameEl.textContent = name;
  if (badgeEl) {
    badgeEl.textContent = role;
    if (role === 'CASHIER') badgeEl.className = 'badge badge-completed';
    else if (role === 'KITCHEN') badgeEl.className = 'badge badge-preparing';
    else badgeEl.className = 'badge badge-ready';
  }
  if (statusEl) {
    statusEl.innerHTML = `<span class="device-status-dot online"></span><span>Connected as <strong>${escapeHtml(role)}</strong> (Active Session)</span>`;
  }
}

// =============================================================================
// TAB NAVIGATION
// =============================================================================

function setupTabs() {
  const tabButtons = document.querySelectorAll('.settings-tab-btn');
  tabButtons.forEach(btn => {
    btn.addEventListener('click', () => {
      tabButtons.forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.settings-panel').forEach(p => p.classList.remove('active'));

      btn.classList.add('active');
      activeTab = btn.dataset.tab;
      const targetPanel = document.getElementById(`panel-${activeTab}`);
      if (targetPanel) targetPanel.classList.add('active');

      if (activeTab === 'revenue') loadRevenueStats();
      if (activeTab === 'devices') loadDevicesList();
      if (activeTab === 'general') refreshTokenPreview();
    });
  });
}

// =============================================================================
// SETTINGS LOAD & SAVE
// =============================================================================

async function loadSettings() {
  try {
    const data = await apiFetch('/api/settings');
    if (!data) return;

    // General
    setVal('setting-cafe-name', data.cafe_name || 'Campus Cafeteria');
    setVal('setting-currency-symbol', data.currency_symbol || '₹');
    setVal('setting-time-format', data.time_format || '12h');
    setVal('setting-token-format', data.token_format || 'from_101');
    setVal('setting-token-prefix-normal', data.token_prefix_normal || 'N-');
    setVal('setting-token-prefix-priority', data.token_prefix_priority || 'P-');

    // Orders
    setChecked('setting-enable-priority-orders', data.enable_priority_orders !== 'false');
    setChecked('setting-enable-cancellations', data.enable_cancellations !== 'false');
    setVal('setting-default-order-status', data.default_order_status || 'WAITING');

    // TV Display
    setVal('setting-tv-business-name', data.tv_business_name || data.cafe_name || 'Campus Cafeteria');
    setVal('setting-tv-completed-limit', data.tv_completed_limit || '10');
    setChecked('setting-tv-show-clock', data.tv_show_clock !== 'false');
    setChecked('setting-tv-show-completed', data.tv_show_completed !== 'false');
    setChecked('setting-sound-notifications', data.sound_notifications !== 'false');

  } catch (err) {
    showToast('Failed to load settings: ' + err.message, 'error');
  }
}

async function saveAllSettings() {
  const saveBtn = document.getElementById('btn-save-settings');
  saveBtn.disabled = true;
  saveBtn.textContent = 'Saving...';

  const payload = {
    // General
    cafe_name: getVal('setting-cafe-name'),
    currency_symbol: getVal('setting-currency-symbol'),
    time_format: getVal('setting-time-format'),
    token_format: getVal('setting-token-format'),
    token_prefix_normal: getVal('setting-token-prefix-normal'),
    token_prefix_priority: getVal('setting-token-prefix-priority'),

    // Orders
    enable_priority_orders: getChecked('setting-enable-priority-orders'),
    enable_cancellations: getChecked('setting-enable-cancellations'),
    default_order_status: getVal('setting-default-order-status'),

    // TV Display
    tv_business_name: getVal('setting-tv-business-name'),
    tv_completed_limit: getVal('setting-tv-completed-limit'),
    tv_show_clock: getChecked('setting-tv-show-clock'),
    tv_show_completed: getChecked('setting-tv-show-completed'),
    sound_notifications: getChecked('setting-sound-notifications')
  };

  try {
    const res = await apiFetch('/api/settings', {
      method: 'POST',
      body: JSON.stringify(payload)
    });

    if (res && res.success) {
      showToast('Settings saved successfully!', 'success');
      refreshTokenPreview();
    } else {
      showToast(res?.error || 'Failed to update settings.', 'error');
    }
  } catch (err) {
    showToast('Error saving settings: ' + err.message, 'error');
  } finally {
    saveBtn.disabled = false;
    saveBtn.innerHTML = `
      <svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/></svg>
      Save Changes
    `;
  }
}

async function refreshTokenPreview() {
  try {
    const preview = await apiFetch('/api/settings/token-preview');
    if (preview) {
      const normalEl = document.getElementById('preview-normal-token');
      const priorityEl = document.getElementById('preview-priority-token');
      if (normalEl) normalEl.textContent = 'Normal: ' + (preview.nextNormal || '--');
      if (priorityEl) priorityEl.textContent = 'Priority: ' + (preview.nextPriority || '--');
    }
  } catch (e) {}
}

// =============================================================================
// REVENUE & SALES BASELINE
// =============================================================================

async function loadRevenueStats() {
  try {
    const data = await apiFetch('/api/dashboard');
    if (!data || !data.stats) return;

    const stats = data.stats;
    const curSym = getVal('setting-currency-symbol') || '₹';

    const actEl = document.getElementById('stat-active-revenue');
    const allEl = document.getElementById('stat-alltime-revenue');
    const compEl = document.getElementById('stat-completed-count');
    const resetTimeEl = document.getElementById('stat-last-reset-time');

    if (actEl) actEl.textContent = curSym + Number(stats.totalRevenue || 0).toFixed(2);
    if (allEl) allEl.textContent = curSym + Number(stats.allTimeRevenue || stats.totalRevenue || 0).toFixed(2);
    if (compEl) compEl.textContent = stats.completedOrders || 0;
    if (resetTimeEl) resetTimeEl.textContent = stats.revenueResetAt || 'Initial State (No baseline resets)';

  } catch (e) {
    console.warn('Revenue fetch failed:', e);
  }
}

function openResetRevenueModal() {
  document.getElementById('modal-reset-revenue').style.display = 'flex';
}

function closeResetRevenueModal() {
  document.getElementById('modal-reset-revenue').style.display = 'none';
}

async function confirmResetRevenue() {
  const reason = document.getElementById('reset-revenue-reason').value.trim() || 'Manual Shift Reset';
  closeResetRevenueModal();

  try {
    const res = await apiFetch('/api/settings/revenue/reset', {
      method: 'POST',
      body: JSON.stringify({ reason })
    });

    if (res && res.success) {
      showToast('Active revenue baseline reset to ₹0!', 'success');
      loadRevenueStats();
    } else {
      showToast(res?.error || 'Failed to reset revenue baseline.', 'error');
    }
  } catch (err) {
    showToast('Error resetting revenue: ' + err.message, 'error');
  }
}

// =============================================================================
// TOKEN SEQUENCE RESET MODAL
// =============================================================================

function openResetTokenModal() {
  document.getElementById('modal-reset-token').style.display = 'flex';
}

function closeResetTokenModal() {
  document.getElementById('modal-reset-token').style.display = 'none';
}

async function confirmResetTokenSequence() {
  const startNormal = parseInt(document.getElementById('reset-start-normal').value, 10) || 101;
  const startPriority = parseInt(document.getElementById('reset-start-priority').value, 10) || 101;
  closeResetTokenModal();

  try {
    const res = await apiFetch('/api/settings/token-reset', {
      method: 'POST',
      body: JSON.stringify({ startingNormal: startNormal, startingPriority: startPriority })
    });

    if (res && res.success) {
      showToast('Token numbering sequence reset successfully!', 'success');
      refreshTokenPreview();
    } else {
      showToast(res?.error || 'Failed to reset sequence.', 'error');
    }
  } catch (err) {
    showToast('Error resetting sequence: ' + err.message, 'error');
  }
}

// =============================================================================
// DEVICE MANAGEMENT & PAIRING
// =============================================================================

async function loadDevicesList() {
  const tbody = document.getElementById('devices-table-body');
  if (!tbody) return;

  try {
    const data = await apiFetch('/api/device/list');
    if (!data || !data.devices || data.devices.length === 0) {
      tbody.innerHTML = `<tr><td colspan="7" style="text-align:center;color:var(--text-muted);padding:30px;">No devices found.</td></tr>`;
      return;
    }

    tbody.innerHTML = data.devices.map(dev => {
      const isOnline = dev.isOnline;
      const statusHtml = isOnline
        ? `<span class="device-status-dot online"></span><span style="font-weight:600;color:#16a34a;font-size:12px;">Online</span>`
        : `<span class="device-status-dot offline"></span><span style="color:#94a3b8;font-size:12px;">Offline</span>`;

      let roleBadge = '<span class="badge badge-normal">NORMAL</span>';
      if (dev.role === 'CASHIER') roleBadge = '<span class="badge badge-completed">Cashier</span>';
      else if (dev.role === 'KITCHEN') roleBadge = '<span class="badge badge-preparing">Kitchen</span>';
      else if (dev.role === 'TV_DISPLAY') roleBadge = '<span class="badge badge-ready">TV Display</span>';

      const isCurrent = dev.isCurrentDevice;

      return `
        <tr>
          <td>${statusHtml}</td>
          <td>
            <strong>${escapeHtml(dev.name)}</strong>
            ${isCurrent ? '<span style="font-size:10px;background:#e2e8f0;padding:1px 5px;border-radius:3px;margin-left:6px;font-weight:600;">THIS DEVICE</span>' : ''}
          </td>
          <td>${roleBadge}</td>
          <td><code style="font-size:12px;color:#475569;">${escapeHtml(dev.ipAddress || 'LAN')}</code></td>
          <td style="font-size:12px;color:var(--text-muted);">${escapeHtml(dev.createdAt)}</td>
          <td style="font-size:12px;color:var(--text-muted);">${escapeHtml(dev.lastConnected)}</td>
          <td style="text-align: right;">
            <div class="device-actions-cell">
              <button class="btn btn-secondary" style="padding:4px 8px;font-size:11px;" onclick="promptRenameDevice(${dev.id}, '${escapeHtml(dev.name)}')">
                Rename
              </button>
              ${!isCurrent ? `
                <button class="btn btn-secondary" style="padding:4px 8px;font-size:11px;color:#b91c1c;border-color:#fca5a5;" onclick="confirmRevokeDevice(${dev.id}, '${escapeHtml(dev.name)}')">
                  Revoke
                </button>
              ` : ''}
            </div>
          </td>
        </tr>
      `;
    }).join('');

  } catch (err) {
    tbody.innerHTML = `<tr><td colspan="7" style="text-align:center;color:#b91c1c;padding:20px;">Failed to load devices: ${escapeHtml(err.message)}</td></tr>`;
  }
}

async function promptRenameDevice(id, currentName) {
  const newName = prompt('Enter new label for this workstation:', currentName);
  if (!newName || newName.trim() === '' || newName.trim() === currentName) return;

  try {
    const res = await apiFetch(`/api/device/${id}/rename`, {
      method: 'POST',
      body: JSON.stringify({ name: newName.trim() })
    });
    if (res && res.success) {
      showToast('Device renamed successfully.', 'success');
      loadDevicesList();
    } else {
      showToast(res?.error || 'Rename failed.', 'error');
    }
  } catch (err) {
    showToast('Error renaming device: ' + err.message, 'error');
  }
}

async function confirmRevokeDevice(id, name) {
  if (!confirm(`Are you sure you want to revoke and disconnect "${name}"? It will immediately return to the device setup screen.`)) {
    return;
  }

  try {
    const res = await apiFetch(`/api/device/${id}/revoke`, {
      method: 'POST',
      body: JSON.stringify({})
    });
    if (res && res.success) {
      showToast('Device access revoked.', 'success');
      loadDevicesList();
    } else {
      showToast(res?.error || 'Revoke failed.', 'error');
    }
  } catch (err) {
    showToast('Error revoking device: ' + err.message, 'error');
  }
}

// Pairing Code Generator Modal
function openGeneratePairingCodeModal() {
  document.getElementById('modal-pairing-code').style.display = 'flex';
  document.getElementById('pairing-code-step-select').style.display = 'block';
  document.getElementById('pairing-code-step-display').style.display = 'none';
  selectCodeRole('KITCHEN');
}

function closePairingCodeModal() {
  document.getElementById('modal-pairing-code').style.display = 'none';
  if (pairingTimerInterval) {
    clearInterval(pairingTimerInterval);
    pairingTimerInterval = null;
  }
}

function selectCodeRole(role) {
  selectedPairingRole = role;
  const kitchenCard = document.getElementById('code-role-kitchen');
  const tvCard = document.getElementById('code-role-tv');
  if (role === 'KITCHEN') {
    kitchenCard.classList.add('active');
    tvCard.classList.remove('active');
  } else {
    tvCard.classList.add('active');
    kitchenCard.classList.remove('active');
  }
}

async function requestNewPairingCode() {
  try {
    const res = await apiFetch('/api/device/pairing-code', {
      method: 'POST',
      body: JSON.stringify({ role: selectedPairingRole })
    });

    if (res && res.code) {
      document.getElementById('pairing-code-step-select').style.display = 'none';
      document.getElementById('pairing-code-step-display').style.display = 'block';

      document.getElementById('generated-code-display').textContent = res.code;
      document.getElementById('target-role-label').textContent = (res.role === 'KITCHEN') ? 'Kitchen Display' : 'TV Display';

      startPairingCountdown(res.expiresIn || 120);
    } else {
      showToast(res?.error || 'Failed to generate code.', 'error');
    }
  } catch (err) {
    showToast('Error generating code: ' + err.message, 'error');
  }
}

function startPairingCountdown(seconds) {
  if (pairingTimerInterval) clearInterval(pairingTimerInterval);
  let remaining = seconds;
  const badge = document.getElementById('pairing-countdown');

  function update() {
    const m = Math.floor(remaining / 60);
    const s = remaining % 60;
    badge.textContent = `${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
    if (remaining <= 0) {
      clearInterval(pairingTimerInterval);
      badge.textContent = 'EXPIRED';
      badge.style.color = '#b91c1c';
      document.getElementById('generated-code-display').style.opacity = '0.4';
    }
    remaining--;
  }

  update();
  pairingTimerInterval = setInterval(update, 1000);
}

// =============================================================================
// DOM VALUE HELPERS
// =============================================================================

function getVal(id) {
  const el = document.getElementById(id);
  return el ? el.value.trim() : '';
}

function setVal(id, val) {
  const el = document.getElementById(id);
  if (el) el.value = val;
}

function getChecked(id) {
  const el = document.getElementById(id);
  return el ? el.checked : false;
}

function setChecked(id, bool) {
  const el = document.getElementById(id);
  if (el) el.checked = Boolean(bool);
}
