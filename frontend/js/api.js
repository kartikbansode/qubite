/**
 * Cafe Token Management System - Central API Client & Multi-Device Auth Utilities
 */

const API_BASE = (window.location.protocol === 'file:') 
  ? 'http://localhost:8080' 
  : window.location.origin;

window.currentDevice = null;

/**
 * Standard fetch wrapper with automatic Device Token header and 401 handling
 */
async function apiFetch(endpoint, options = {}) {
  const url = `${API_BASE}${endpoint}`;
  const defaultHeaders = {
    'Accept': 'application/json'
  };

  const devToken = localStorage.getItem('cafe_device_token');
  if (devToken) {
    defaultHeaders['X-Device-Token'] = devToken;
  }

  if (options.body && typeof options.body === 'string') {
    defaultHeaders['Content-Type'] = 'application/json';
  }

  const config = {
    ...options,
    headers: {
      ...defaultHeaders,
      ...(options.headers || {})
    }
  };

  try {
    const response = await fetch(url, config);
    const data = await response.json().catch(() => null);

    if (response.status === 401) {
      if (devToken) {
        localStorage.removeItem('cafe_device_token');
        localStorage.removeItem('cafe_device_role');
        localStorage.removeItem('cafe_device_name');
        showToast('Device session was revoked or invalidated by Cashier.', 'error');
      }
      showDeviceSetupModal('Your device was unlinked or revoked. Please pair again.');
      throw new Error((data && data.error) ? data.error : 'Unauthorized device session');
    }

    if (!response.ok) {
      const errMsg = (data && data.error) ? data.error : `HTTP error ${response.status}: ${response.statusText}`;
      throw new Error(errMsg);
    }

    return data;
  } catch (error) {
    console.error(`API Error on [${options.method || 'GET'}] ${endpoint}:`, error);
    throw error;
  }
}

/**
 * Professional SVG Icons for notifications, badges, and modals
 */
const ICONS = {
  info: `<svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/></svg>`,
  success: `<svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>`,
  error: `<svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>`,
  cashier: `<svg class="icon-svg" viewBox="0 0 24 24"><rect x="2" y="4" width="20" height="16" rx="2"/><line x1="2" y1="10" x2="22" y2="10"/><line x1="7" y1="15" x2="12" y2="15"/></svg>`,
  kitchen: `<svg class="icon-svg" viewBox="0 0 24 24"><path d="M6 13.8V4a2 2 0 0 1 4 0v9.8"/><path d="M6 10h4"/><circle cx="12" cy="12" r="10"/></svg>`,
  tv: `<svg class="icon-svg" viewBox="0 0 24 24"><rect x="2" y="3" width="20" height="14" rx="2" ry="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg>`,
  key: `<svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><path d="M21 2l-2 2m-1.5 1.5L14 9l-2-2 2-2 3.5 3.5M3 21l6.5-6.5"/><circle cx="7.5" cy="16.5" r="2.5"/></svg>`,
  check: `<svg class="icon-svg icon-svg-sm" viewBox="0 0 24 24"><polyline points="20 6 9 17 4 12"/></svg>`
};

/**
 * Toast notifications with SVG icons (no emojis)
 */
function showToast(message, type = 'info') {
  let container = document.getElementById('toast-container');
  if (!container) {
    container = document.createElement('div');
    container.id = 'toast-container';
    document.body.appendChild(container);
  }

  const toast = document.createElement('div');
  toast.className = `toast ${type}`;
  
  const iconSvg = ICONS[type] || ICONS.info;
  toast.innerHTML = `<span>${iconSvg}</span> <span>${escapeHtml(message)}</span>`;
  container.appendChild(toast);

  setTimeout(() => {
    toast.style.opacity = '0';
    toast.style.transform = 'translateY(10px)';
    toast.style.transition = 'all 0.3s ease';
    setTimeout(() => toast.remove(), 300);
  }, 3500);
}

// =========================================================================
// CUSTOM READY BELL AUDIO SYSTEM (assets/bell.mp3)
// =========================================================================
let readyBellAudio = null;
let isAudioGestureUnlocked = false;

function initReadyBell() {
  if (!readyBellAudio) {
    try {
      readyBellAudio = new Audio('assets/bell.mp3');
      readyBellAudio.preload = 'auto';
    } catch (e) {
      console.warn('Custom bell audio init notice:', e);
    }
  }
}

// User-gesture unlocker for modern browser autoplay policies
function unlockAudioPlayback() {
  if (!readyBellAudio) initReadyBell();
  isAudioGestureUnlocked = true;
  if (readyBellAudio) {
    try {
      readyBellAudio.load();
    } catch (e) {}
  }
}
window.addEventListener('click', unlockAudioPlayback, { once: true });
window.addEventListener('touchstart', unlockAudioPlayback, { once: true });
window.addEventListener('keydown', unlockAudioPlayback, { once: true });

function playReadyBell(force = false) {
  // If not forcing (e.g. preview/test button), respect TV/System sound settings
  if (!force) {
    if (typeof tvSettings !== 'undefined' && tvSettings.soundNotifications !== undefined) {
      if (!tvSettings.soundNotifications) return;
    }
  }

  initReadyBell();

  if (readyBellAudio) {
    readyBellAudio.currentTime = 0;
    const playPromise = readyBellAudio.play();
    if (playPromise !== undefined) {
      playPromise.catch(err => {
        console.warn('Custom bell.mp3 playback prevented by browser or missing, using fallback chime:', err);
        fallbackSynthesizerChime();
      });
    }
  } else {
    fallbackSynthesizerChime();
  }
}

function fallbackSynthesizerChime() {
  try {
    const AudioCtx = window.AudioContext || window.webkitAudioContext;
    if (!AudioCtx) return;
    const ctx = new AudioCtx();
    if (ctx.state === 'suspended') {
      ctx.resume();
    }
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(587.33, ctx.currentTime); // D5
    osc.frequency.setValueAtTime(880, ctx.currentTime + 0.15); // A5
    gain.gain.setValueAtTime(0.2, ctx.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.7);
    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start();
    osc.stop(ctx.currentTime + 0.7);
  } catch (e) {
    console.warn('Fallback synthesizer chime failed:', e);
  }
}

/**
 * Formatting helpers
 */
function formatCurrency(amount) {
  const num = Number(amount) || 0;
  return '₹' + num.toFixed(2);
}

function formatDateTime(dateStr) {
  if (!dateStr) return '--';
  return dateStr;
}

function escapeHtml(str) {
  if (!str) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

function renderStatusBadge(status) {
  const s = (status || '').toUpperCase();
  let badgeClass = 'badge-waiting';
  if (s === 'PREPARING') badgeClass = 'badge-preparing';
  else if (s === 'READY') badgeClass = 'badge-ready';
  else if (s === 'COMPLETED') badgeClass = 'badge-completed';

  return `<span class="badge ${badgeClass}">${escapeHtml(s)}</span>`;
}

function renderTypeBadge(type) {
  const t = (type || '').toUpperCase();
  if (t === 'PRIORITY') {
    return `<span class="badge badge-priority">PRIORITY</span>`;
  }
  return `<span class="badge badge-normal">NORMAL</span>`;
}

function renderTokenBadge(token, isPriority) {
  const cls = isPriority ? 'token-badge priority' : 'token-badge';
  return `<span class="${cls}">${escapeHtml(token)}</span>`;
}

/**
 * Ping backend for server status indication
 */
async function checkServerHealth() {
  const statusDot = document.getElementById('server-status-dot');
  const statusText = document.getElementById('server-status-text');
  if (!statusDot || !statusText) return;

  try {
    const res = await fetch(`${API_BASE}/api/settings/public`, { method: 'GET', cache: 'no-store' });
    if (res.ok) {
      statusDot.className = 'status-dot';
      statusText.textContent = 'Server Online';
    } else {
      statusDot.className = 'status-dot offline';
      statusText.textContent = 'Server Error';
    }
  } catch (e) {
    statusDot.className = 'status-dot offline';
    statusText.textContent = 'Disconnected';
  }
}

/**
 * Fullscreen toggle helper
 */
function toggleFullScreen() {
  const isFullScreen = document.body.classList.contains('fullscreen-mode') || !!document.fullscreenElement;
  const enterIcon = document.getElementById('icon-fullscreen-enter');
  const exitIcon = document.getElementById('icon-fullscreen-exit');

  if (!isFullScreen) {
    document.body.classList.add('fullscreen-mode');
    if (document.documentElement.requestFullscreen) {
      document.documentElement.requestFullscreen().catch(() => {});
    }
    if (enterIcon) enterIcon.style.display = 'none';
    if (exitIcon) exitIcon.style.display = 'inline-block';
  } else {
    document.body.classList.remove('fullscreen-mode');
    if (document.fullscreenElement && document.exitFullscreen) {
      document.exitFullscreen().catch(() => {});
    }
    if (enterIcon) enterIcon.style.display = 'inline-block';
    if (exitIcon) exitIcon.style.display = 'none';
  }
}

document.addEventListener('fullscreenchange', () => {
  const enterIcon = document.getElementById('icon-fullscreen-enter');
  const exitIcon = document.getElementById('icon-fullscreen-exit');
  if (!document.fullscreenElement) {
    document.body.classList.remove('fullscreen-mode');
    if (enterIcon) enterIcon.style.display = 'inline-block';
    if (exitIcon) exitIcon.style.display = 'none';
  } else {
    document.body.classList.add('fullscreen-mode');
    if (enterIcon) enterIcon.style.display = 'none';
    if (exitIcon) exitIcon.style.display = 'inline-block';
  }
});

// =============================================================================
// MULTI-DEVICE AUTHENTICATION & PAIRING MODAL
// =============================================================================

// =============================================================================
// ROLE-BASED NAVIGATION & MULTI-DEVICE AUTHENTICATION
// =============================================================================

function applyRoleBasedNavigation(role) {
  const currentRole = role || localStorage.getItem('cafe_device_role') || 'CASHIER';
  const currentPath = window.location.pathname.toLowerCase();
  const page = currentPath.split('/').pop() || 'index.html';

  // 1. Sidebar menu filtering
  const navMenu = document.querySelector('.nav-menu');
  if (navMenu) {
    const items = navMenu.querySelectorAll('li');
    items.forEach(li => {
      const link = li.querySelector('a');
      if (!link) return;
      const href = (link.getAttribute('href') || '').toLowerCase();

      let shouldShow = false;
      if (currentRole === 'CASHIER') {
        // Cashier sees ONLY: index.html (Dashboard), cashier.html, history.html, settings.html
        if (href.includes('index.html') || href.includes('cashier.html') || href.includes('history.html') || href.includes('settings.html')) {
          shouldShow = true;
        }
      } else if (currentRole === 'KITCHEN') {
        // Kitchen sees ONLY: kitchen.html, settings.html
        if (href.includes('kitchen.html') || href.includes('settings.html')) {
          shouldShow = true;
        }
      } else if (currentRole === 'TV_DISPLAY') {
        // TV Display sees ONLY: display.html, settings.html
        if (href.includes('display.html') || href.includes('settings.html')) {
          shouldShow = true;
        }
      }
      li.style.display = shouldShow ? '' : 'none';
    });
  }

  // 2. Strict screen access enforcement with centered restricted box
  if (currentRole === 'KITCHEN') {
    if (page === 'index.html' || page === 'cashier.html' || page === 'history.html' || page === 'display.html') {
      showCenteredAccessRestricted('Kitchen', 'kitchen.html', 'Return to Kitchen');
    }
  } else if (currentRole === 'TV_DISPLAY') {
    if (page === 'index.html' || page === 'cashier.html' || page === 'kitchen.html' || page === 'history.html') {
      showCenteredAccessRestricted('TV Display', 'display.html', 'Return to TV Display');
    }
  } else if (currentRole === 'CASHIER') {
    if (page === 'kitchen.html' || page === 'display.html') {
      showCenteredAccessRestricted('Cashier', 'cashier.html', 'Return to Cashier');
    }
  }
}

function showCenteredAccessRestricted(roleName, returnUrl, returnText) {
  document.body.innerHTML = `
    <div class="access-restricted-wrapper">
      <div class="access-restricted-card">
        <div class="access-restricted-icon">
          <svg class="icon-svg icon-svg-xl" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
        </div>
        <h2>Access Restricted</h2>
        <p>This screen is not available for <strong>${escapeHtml(roleName)}</strong> workstations. Your device is configured for ${escapeHtml(roleName)} operations.</p>
        <div style="display: flex; gap: 12px; margin-top: 24px; justify-content: center; width: 100%;">
          <a href="${returnUrl}" class="btn btn-primary" style="flex: 1; text-align: center;">${escapeHtml(returnText)}</a>
          <button type="button" class="btn btn-secondary" onclick="quitDeviceSession()" style="flex: 1; border-color: #fca5a5; color: #dc2626;">Quit Session</button>
        </div>
      </div>
    </div>
  `;
}

function quitDeviceSession() {
  if (confirm('Are you sure you want to quit this workstation session? This device will be unlinked and you can select a new role or re-pair.')) {
    localStorage.removeItem('cafe_device_token');
    localStorage.removeItem('cafe_device_role');
    localStorage.removeItem('cafe_device_name');
    window.currentDevice = null;
    showToast('Workstation session ended.', 'info');
    setTimeout(() => {
      window.location.href = 'index.html';
    }, 400);
  }
}

async function checkDeviceAuthentication() {
  const devToken = localStorage.getItem('cafe_device_token');
  const role = localStorage.getItem('cafe_device_role');
  const isDisplayPage = window.location.pathname.endsWith('display.html');

  // Immediately apply role-based navigation from localStorage
  if (role) {
    applyRoleBasedNavigation(role);
  }

  if (!devToken) {
    if (!isDisplayPage) {
      showDeviceSetupModal();
    }
    return;
  }

  try {
    const authData = await fetch(`${API_BASE}/api/device/auth`, {
      method: 'GET',
      headers: {
        'Accept': 'application/json',
        'X-Device-Token': devToken
      }
    }).then(r => r.json());

    if (authData && authData.authenticated) {
      window.currentDevice = authData;
      localStorage.setItem('cafe_device_role', authData.role);
      localStorage.setItem('cafe_device_name', authData.name);
      renderDeviceIndicator();

      applyRoleBasedNavigation(authData.role);
    } else {
      localStorage.removeItem('cafe_device_token');
      localStorage.removeItem('cafe_device_role');
      localStorage.removeItem('cafe_device_name');
      if (!isDisplayPage) {
        showDeviceSetupModal('Your device session was revoked or expired. Please select a role to connect.');
      }
    }
  } catch (err) {
    console.warn('Device authentication check warning:', err);
    if (role) applyRoleBasedNavigation(role);
  }
}

function renderDeviceIndicator() {
  const topbar = document.querySelector('.topbar-actions');
  if (!topbar) return;

  const device = window.currentDevice || {
    name: localStorage.getItem('cafe_device_name') || 'Main Counter POS',
    role: localStorage.getItem('cafe_device_role') || 'CASHIER'
  };

  const devToken = localStorage.getItem('cafe_device_token');
  if (!devToken && !window.currentDevice) return;

  let badge = document.getElementById('topbar-device-badge');
  if (!badge) {
    badge = document.createElement('div');
    badge.id = 'topbar-device-badge';
    topbar.insertBefore(badge, topbar.firstChild);
  }

  const role = device.role || 'CASHIER';
  const roleText = (role === 'CASHIER') ? 'Cashier' : (role === 'KITCHEN' ? 'Kitchen' : 'TV Display');
  const roleClass = (role === 'CASHIER') ? 'role-cashier' : (role === 'KITCHEN' ? 'role-kitchen' : 'role-tv');
  const nameText = device.name || roleText;

  badge.className = `topbar-device-badge ${roleClass}`;
  badge.title = `Active Session: ${nameText} (${roleText})`;

  badge.innerHTML = `
    <div class="device-badge-indicator">
      <span class="device-badge-dot"></span>
      <svg class="device-badge-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <rect x="2" y="3" width="20" height="14" rx="2" ry="2"/>
        <line x1="8" y1="21" x2="16" y2="21"/>
        <line x1="12" y1="17" x2="12" y2="21"/>
      </svg>
      <span class="device-badge-name">${escapeHtml(nameText)}</span>
    </div>
    <span class="device-badge-divider"></span>
    <span class="device-badge-role">${escapeHtml(roleText)}</span>
  `;
}

async function showDeviceSetupModal(alertMsg = '') {
  let existing = document.getElementById('device-setup-modal');
  if (existing) existing.remove();

  // Query system status
  let sysStatus = { initialSetupCompleted: false, hasCashier: false };
  try {
    const res = await fetch(`${API_BASE}/api/system/status`).then(r => r.json());
    if (res) sysStatus = res;
  } catch (e) {}

  const backdrop = document.createElement('div');
  backdrop.id = 'device-setup-modal';
  backdrop.className = 'modal-backdrop';

  backdrop.innerHTML = `
    <div class="modal-card modal-card-lg">
      <div class="modal-header">
        <h3>
          <svg class="icon-svg icon-svg-md" viewBox="0 0 24 24"><rect x="2" y="3" width="20" height="14" rx="2" ry="2"/><line x1="8" y1="21" x2="16" y2="21"/><line x1="12" y1="17" x2="12" y2="21"/></svg>
          Device Setup &amp; Role Selection
        </h3>
      </div>
      <div class="modal-body">
        ${alertMsg ? `<div class="revenue-reset-warning" style="margin-bottom:16px;">${escapeHtml(alertMsg)}</div>` : ''}
        <p style="font-size: 13px; color: var(--text-muted); margin-bottom: 16px;">
          Select the operating role for this workstation on the cafeteria network:
        </p>

        <div class="role-select-grid">
          <div class="role-card active" data-role="CASHIER" id="role-card-cashier">
            <div class="role-card-icon">${ICONS.cashier}</div>
            <div class="role-card-title">Cashier</div>
            <div class="role-card-desc">Master POS, order creation, sales analytics &amp; settings.</div>
          </div>

          <div class="role-card" data-role="KITCHEN" id="role-card-kitchen">
            <div class="role-card-icon">${ICONS.kitchen}</div>
            <div class="role-card-title">Kitchen Display</div>
            <div class="role-card-desc">Active orders queue, preparing/ready status progression.</div>
          </div>

          <div class="role-card" data-role="TV_DISPLAY" id="role-card-tv">
            <div class="role-card-icon">${ICONS.tv}</div>
            <div class="role-card-title">TV Display</div>
            <div class="role-card-desc">Customer-facing fullscreen status board (Preparing &amp; Ready).</div>
          </div>
        </div>

        <!-- Role Form Fields -->
        <div id="role-setup-form-fields">
          <!-- Cashier First-Time Setup Extra Fields -->
          <div id="cashier-first-time-fields" style="${sysStatus.initialSetupCompleted ? 'display: none;' : ''}">
            <div class="settings-row" style="margin-bottom: 14px;">
              <label for="setup-cafe-name">Cafeteria / Business Name</label>
              <input type="text" id="setup-cafe-name" class="input" value="${escapeHtml(sysStatus.cafeName || 'Campus Cafeteria')}">
            </div>
            <div class="settings-row" style="margin-bottom: 14px;">
              <label for="setup-currency">Currency Symbol</label>
              <input type="text" id="setup-currency" class="input" value="${escapeHtml(sysStatus.currencySymbol || '₹')}" style="max-width: 120px;">
            </div>
          </div>

          <!-- Common Device Name Field -->
          <div class="settings-row" style="margin-bottom: 14px;">
            <label for="setup-device-name" id="label-device-name">Workstation Label</label>
            <input type="text" id="setup-device-name" class="input" value="Main Counter POS" required>
            <span class="settings-row-desc" id="desc-device-name">A recognizable name for this device in device management.</span>
          </div>

          <!-- Pairing Code Field (For Kitchen & TV Display) -->
          <div id="field-pairing-code-wrapper" style="display: none; margin-bottom: 12px;">
            <div class="settings-row">
              <label for="setup-pairing-code">6-Digit Pairing Code</label>
              <input type="text" id="setup-pairing-code" class="pairing-code-input" placeholder="000-000" maxlength="7" autocomplete="off">
              <div style="display:flex;align-items:center;gap:6px;font-size:11px;color:var(--text-muted);margin-top:6px;">
                ${ICONS.key}
                <span>Get 6-digit code from Cashier &rarr; Settings &rarr; Device Management (or enter 000000 for quick connect)</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      <div class="modal-footer">
        <button type="button" class="btn btn-primary" id="btn-submit-setup">Connect Workstation</button>
      </div>
    </div>
  `;

  document.body.appendChild(backdrop);

  // Logic for selecting role cards
  let selectedRole = 'CASHIER';
  const roleCards = backdrop.querySelectorAll('.role-card');
  const cashierExtraFields = document.getElementById('cashier-first-time-fields');
  const devNameInput = document.getElementById('setup-device-name');
  const labelDevName = document.getElementById('label-device-name');
  const pairingCodeWrapper = document.getElementById('field-pairing-code-wrapper');
  const codeInput = document.getElementById('setup-pairing-code');
  const btnSubmit = document.getElementById('btn-submit-setup');

  function updateRoleUI(role) {
    selectedRole = role;
    if (role === 'CASHIER') {
      cashierExtraFields.style.display = sysStatus.initialSetupCompleted ? 'none' : 'block';
      labelDevName.textContent = 'Cashier Workstation Name';
      devNameInput.value = 'Main Counter POS';
      pairingCodeWrapper.style.display = 'none';
      btnSubmit.textContent = sysStatus.initialSetupCompleted ? 'Connect as Cashier' : 'Initialize Master Cashier';
    } else if (role === 'KITCHEN') {
      cashierExtraFields.style.display = 'none';
      labelDevName.textContent = 'Kitchen Screen Name';
      devNameInput.value = 'Kitchen Screen 1';
      pairingCodeWrapper.style.display = 'block';
      codeInput.value = '';
      codeInput.placeholder = 'XXX-XXX';
      btnSubmit.textContent = 'Connect as Kitchen Display';
    } else if (role === 'TV_DISPLAY') {
      cashierExtraFields.style.display = 'none';
      labelDevName.textContent = 'TV Display Screen Name';
      devNameInput.value = 'TV Display Screen';
      pairingCodeWrapper.style.display = 'block';
      codeInput.value = '000-000';
      codeInput.placeholder = '000-000';
      btnSubmit.textContent = 'Connect as TV Display';
    }
  }

  roleCards.forEach(card => {
    card.addEventListener('click', () => {
      roleCards.forEach(c => c.classList.remove('active'));
      card.classList.add('active');
      updateRoleUI(card.dataset.role);
    });
  });

  // Auto-format 6-digit code with hyphen: XXX-XXX
  codeInput.addEventListener('input', (e) => {
    let val = e.target.value.replace(/[^0-9]/g, '');
    if (val.length > 3) {
      val = val.substring(0, 3) + '-' + val.substring(3, 6);
    }
    e.target.value = val;
  });

  // Submit Handler
  btnSubmit.addEventListener('click', async () => {
    btnSubmit.disabled = true;
    btnSubmit.textContent = 'Connecting...';

    const deviceName = devNameInput.value.trim() || 'Workstation Screen';

    try {
      if (selectedRole === 'CASHIER' && !sysStatus.initialSetupCompleted) {
        // Master setup
        const cafeName = document.getElementById('setup-cafe-name')?.value.trim() || 'Campus Cafeteria';
        const currencySymbol = document.getElementById('setup-currency')?.value.trim() || '₹';

        const res = await fetch(`${API_BASE}/api/device/setup-master`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ cafeName, currencySymbol, deviceName })
        }).then(r => r.json());

        if (res && res.token) {
          localStorage.setItem('cafe_device_token', res.token);
          localStorage.setItem('cafe_device_role', 'CASHIER');
          localStorage.setItem('cafe_device_name', res.name);
          backdrop.remove();
          showToast('Master Cashier initialized successfully!', 'success');
          setTimeout(() => { window.location.href = 'cashier.html'; }, 500);
          return;
        } else {
          showToast(res?.error || 'Setup failed.', 'error');
          btnSubmit.disabled = false;
          btnSubmit.textContent = 'Initialize Master Cashier';
          return;
        }
      }

      // Pairing for Cashier (if initialized), Kitchen, or TV Display
      let code = codeInput.value.trim();
      if (selectedRole === 'CASHIER') {
        code = '000000';
      } else if (!code) {
        code = '000000';
      }

      const res = await fetch(`${API_BASE}/api/device/pair`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ code, deviceName, role: selectedRole })
      }).then(r => r.json());

      if (res && res.token) {
        localStorage.setItem('cafe_device_token', res.token);
        localStorage.setItem('cafe_device_role', res.role);
        localStorage.setItem('cafe_device_name', res.name);
        backdrop.remove();
        showToast(`Connected successfully as ${res.role}!`, 'success');

        setTimeout(() => {
          if (res.role === 'KITCHEN') window.location.href = 'kitchen.html';
          else if (res.role === 'TV_DISPLAY') window.location.href = 'display.html';
          else window.location.href = 'cashier.html';
        }, 500);
      } else {
        showToast(res?.error || 'Connection failed. Verify code.', 'error');
        btnSubmit.disabled = false;
        btnSubmit.textContent = 'Connect Workstation';
      }
    } catch (err) {
      showToast('Connection error: ' + err.message, 'error');
      btnSubmit.disabled = false;
      btnSubmit.textContent = 'Connect Workstation';
    }
  });
}

// Check device auth on load
document.addEventListener('DOMContentLoaded', () => {
  renderDeviceIndicator();
  checkServerHealth();
  setInterval(checkServerHealth, 10000);
  checkDeviceAuthentication();
});
