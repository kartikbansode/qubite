/**
 * Customer-Facing TV Display (display.js)
 * Live status updates with 2.5s polling (Light Mode, Industry Minimalist)
 */

let isDisplayPolling = false;
let knownReadyTokens = new Set();
let knownCompletedTokens = new Set();
let isInitialLoad = true;
let tvSettings = {
  cafeName: 'Campus Cafeteria',
  tvBusinessName: 'Campus Cafeteria',
  tvShowClock: true,
  tvShowCompleted: true,
  tvCompletedLimit: 10,
  soundNotifications: true,
  timeFormat: '12h'
};

function playChimeSound() {
  playReadyBell(false);
}

function toggleTvAudio() {
  tvSettings.soundNotifications = !tvSettings.soundNotifications;
  const iconOn = document.getElementById('icon-audio-on');
  const iconOff = document.getElementById('icon-audio-off');
  const textState = document.getElementById('text-audio-state');

  if (iconOn && iconOff) {
    iconOn.style.display = tvSettings.soundNotifications ? 'block' : 'none';
    iconOff.style.display = tvSettings.soundNotifications ? 'none' : 'block';
  }
  if (textState) {
    textState.textContent = tvSettings.soundNotifications ? 'Bell Alert' : 'Muted';
  }

  if (tvSettings.soundNotifications) {
    playReadyBell(true); // Preview sound & unlock audio
    showToast('Ready bell sound alert enabled', 'info');
  } else {
    showToast('Ready bell sound muted', 'info');
  }
}


async function loadTvSettings() {
  try {
    const data = await apiFetch('/api/settings/public');
    if (data) {
      tvSettings = { ...tvSettings, ...data };
      applyTvSettings();
    }
  } catch (e) {
    console.warn('Could not load public TV settings:', e);
  }
}

function applyTvSettings() {
  const brandTitle = document.querySelector('.tv-brand-title');
  if (brandTitle && tvSettings.tvBusinessName) {
    brandTitle.textContent = tvSettings.tvBusinessName;
  }

  const clockBox = document.querySelector('.tv-time-box');
  if (clockBox) {
    clockBox.style.display = tvSettings.tvShowClock ? 'block' : 'none';
  }

  const completedSection = document.getElementById('tv-completed-column');
  if (completedSection) {
    completedSection.style.display = tvSettings.tvShowCompleted ? 'flex' : 'none';
  }
}

function updateLiveClock() {
  const clockEl = document.getElementById('tv-live-clock');
  const dateEl = document.getElementById('tv-live-date');
  if (!clockEl || !dateEl) return;

  const now = new Date();
  const is12h = tvSettings.timeFormat !== '24h';
  
  clockEl.textContent = now.toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: is12h
  });

  dateEl.textContent = now.toLocaleDateString([], {
    weekday: 'short',
    month: 'short',
    day: 'numeric'
  });
}

async function fetchDisplayData() {
  if (isDisplayPolling) return;
  isDisplayPolling = true;

  try {
    // 1. Fetch active orders (preparing, ready, completed)
    const activeData = await apiFetch('/api/orders/active');

    const preparing = (activeData && activeData.preparing) ? activeData.preparing : [];
    const ready = (activeData && activeData.ready) ? activeData.ready : [];
    const completedAll = (activeData && activeData.completed) ? [...activeData.completed] : [];

    // Sort completed orders so that the very recent ones appear at the top
    completedAll.sort((a, b) => {
      const timeA = a.updatedAt ? new Date(a.updatedAt).getTime() : 0;
      const timeB = b.updatedAt ? new Date(b.updatedAt).getTime() : 0;
      if (timeA !== timeB) return timeB - timeA;
      if (a.id && b.id) return b.id - a.id;
      return 0;
    });

    const limit = parseInt(tvSettings.tvCompletedLimit, 10) || 10;
    const completed = completedAll.slice(0, limit);

    // Check for newly ready or newly completed tokens to play sound on TV Display
    const currentReadyTokens = new Set(ready.map(o => o.token));
    const currentCompletedTokens = new Set(completed.map(o => o.token));

    if (!isInitialLoad) {
      let shouldPlayBell = false;

      // Check if order became Ready for pickup
      for (const tok of currentReadyTokens) {
        if (!knownReadyTokens.has(tok)) {
          shouldPlayBell = true;
          break;
        }
      }

      if (shouldPlayBell) playChimeSound();
    }

    knownReadyTokens = currentReadyTokens;
    knownCompletedTokens = currentCompletedTokens;
    isInitialLoad = false;

    renderPreparingColumn(preparing);
    renderReadyColumn(ready);
    renderCompletedColumn(completed);

    const updatedEl = document.getElementById('tv-last-updated');
    if (updatedEl) {
      const now = new Date();
      updatedEl.textContent = now.toLocaleTimeString([], { hour12: tvSettings.timeFormat !== '24h' });
    }

  } catch (err) {
    console.warn('TV Display fetch error:', err);
  } finally {
    isDisplayPolling = false;
  }
}

function renderPreparingColumn(orders) {
  const listEl = document.getElementById('tv-preparing-list');
  const countEl = document.getElementById('tv-count-preparing');
  if (!listEl) return;

  if (countEl) countEl.textContent = orders.length;

  if (orders.length === 0) {
    listEl.innerHTML = '<div class="tv-empty-box">No orders in preparation</div>';
    return;
  }

  listEl.innerHTML = orders.map(ord => {
    const isPriority = ord.orderType === 'PRIORITY';
    return `
      <div class="tv-order-card ${isPriority ? 'priority-card' : ''}">
        <div class="tv-token-large">${escapeHtml(ord.token)}</div>
        <div>
          ${isPriority ? '<span class="tv-token-badge-priority">PRIORITY</span>' : ''}
        </div>
      </div>
    `;
  }).join('');
}

function renderReadyColumn(orders) {
  const listEl = document.getElementById('tv-ready-list');
  const countEl = document.getElementById('tv-count-ready');
  if (!listEl) return;

  if (countEl) countEl.textContent = orders.length;

  if (orders.length === 0) {
    listEl.innerHTML = '<div class="tv-empty-box">No orders ready for pickup</div>';
    return;
  }

  listEl.innerHTML = orders.map(ord => {
    const isPriority = ord.orderType === 'PRIORITY';
    return `
      <div class="tv-order-card ready-highlight ${isPriority ? 'priority-card' : ''}">
        <div class="tv-token-large ready-text">${escapeHtml(ord.token)}</div>
        <div>
          ${isPriority ? '<span class="tv-token-badge-priority">PRIORITY</span>' : ''}
        </div>
      </div>
    `;
  }).join('');
}

function renderCompletedColumn(orders) {
  const listEl = document.getElementById('tv-completed-list');
  const countEl = document.getElementById('tv-count-completed');
  if (!listEl) return;

  if (countEl) countEl.textContent = orders.length;

  if (orders.length === 0) {
    listEl.innerHTML = '<div class="tv-empty-box">No completed orders</div>';
    return;
  }

  listEl.innerHTML = orders.map(ord => {
    const isPriority = ord.type === 'PRIORITY' || ord.orderType === 'PRIORITY';
    return `
      <div class="tv-order-card" style="opacity: 0.75;">
        <div class="tv-token-large" style="font-size: 32px; color: #64748b;">${escapeHtml(ord.token)}</div>
        <div>
          ${isPriority ? '<span class="tv-token-badge-priority">PRIORITY</span>' : ''}
        </div>
      </div>
    `;
  }).join('');
}

document.addEventListener('DOMContentLoaded', () => {
  loadTvSettings();
  setInterval(loadTvSettings, 15000); // Check settings changes every 15s

  updateLiveClock();
  setInterval(updateLiveClock, 1000);

  fetchDisplayData();
  // Live polling every 2.5 seconds
  setInterval(fetchDisplayData, 2500);
});
