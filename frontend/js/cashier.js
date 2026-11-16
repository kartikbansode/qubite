/**
 * Cashier JavaScript
 */

let allMenuItems = [];
let currentCategory = 'All';
let searchQuery = '';
let cart = []; // Array of { id, name, price, quantity }
let selectedOrderType = 'NORMAL';

async function loadMenu() {
  try {
    const data = await apiFetch('/api/menu');
    allMenuItems = (data && data.menu) ? data.menu : [];
    updateCategoryTabs();
    renderMenu();
  } catch (err) {
    showToast('Failed to load menu: ' + err.message, 'error');
  }
}

function updateCategoryTabs() {
  const container = document.getElementById('category-tabs');
  if (!container || allMenuItems.length === 0) return;

  // Extract unique categories preserving order
  const categories = ['All'];
  allMenuItems.forEach(item => {
    if (item.category && !categories.includes(item.category)) {
      categories.push(item.category);
    }
  });

  // Only re-render tabs if categories changed
  const existingButtons = container.querySelectorAll('.tab-btn');
  const currentTabTexts = Array.from(existingButtons).map(b => b.getAttribute('data-cat') || b.textContent.trim());
  const isMatch = categories.length === currentTabTexts.length && categories.every((c, i) => c === currentTabTexts[i]);
  if (!isMatch) {
    container.innerHTML = categories.map(cat => `
      <button class="tab-btn ${cat === currentCategory ? 'active' : ''}" data-cat="${escapeHtml(cat)}" onclick="filterCategory('${escapeHtml(cat)}')">
        ${escapeHtml(cat === 'All' ? 'All Items' : cat)}
      </button>
    `).join('');
  }
}

function filterCategory(category) {
  currentCategory = category;
  const tabs = document.querySelectorAll('#category-tabs .tab-btn');
  tabs.forEach(btn => {
    const btnCat = btn.getAttribute('data-cat') || btn.textContent.trim();
    if (btnCat === category || (category === 'All' && btnCat.includes('All'))) {
      btn.classList.add('active');
    } else {
      btn.classList.remove('active');
    }
  });
  renderMenu();
}

function handleMenuSearch(query) {
  searchQuery = (query || '').trim().toLowerCase();
  const clearBtn = document.getElementById('search-clear-btn');
  if (clearBtn) {
    clearBtn.style.display = searchQuery.length > 0 ? 'flex' : 'none';
  }
  renderMenu();
}

function clearMenuSearch() {
  searchQuery = '';
  const searchInput = document.getElementById('menu-search-input');
  if (searchInput) {
    searchInput.value = '';
    searchInput.focus();
  }
  const clearBtn = document.getElementById('search-clear-btn');
  if (clearBtn) clearBtn.style.display = 'none';
  renderMenu();
}

function getFilteredMenuItems() {
  return allMenuItems.filter(item => {
    const matchesCat = (currentCategory === 'All' || item.category === currentCategory);
    if (!searchQuery) {
      return matchesCat;
    }
    // If search query is active, search across name and category
    // If category is not "All", search within that category; if "All", search all items
    const matchesQuery = item.name.toLowerCase().includes(searchQuery) ||
                         item.category.toLowerCase().includes(searchQuery);
    return matchesCat ? matchesQuery : (currentCategory === 'All' && matchesQuery);
  });
}

function handleSearchKeyDown(event) {
  if (event.key === 'Enter') {
    event.preventDefault();
    const visibleItems = getFilteredMenuItems();
    if (visibleItems.length > 0) {
      const first = visibleItems[0];
      addToCart(first.id);
      showToast(`Added "${first.name}" to cart`, 'info');
      // Highlight search input text so user can immediately type another item
      const input = document.getElementById('menu-search-input');
      if (input) input.select();
    } else {
      showToast('No matching items to add.', 'warning');
    }
  } else if (event.key === 'Escape') {
    clearMenuSearch();
  }
}

function renderMenu() {
  const grid = document.getElementById('menu-items-grid');
  if (!grid) return;

  const filtered = getFilteredMenuItems();

  if (filtered.length === 0) {
    if (searchQuery) {
      grid.innerHTML = `
        <div class="empty-state" style="grid-column: 1/-1; padding: 32px 16px;">
          <p style="margin-bottom: 8px;">No menu items matching <strong>"${escapeHtml(searchQuery)}"</strong>.</p>
          <button class="btn btn-primary btn-sm" onclick="openAddItemModal('${escapeHtml(searchQuery)}')">
            + Add "${escapeHtml(searchQuery)}" to Menu
          </button>
        </div>
      `;
    } else {
      grid.innerHTML = '<div class="empty-state" style="grid-column: 1/-1;">No items found in this category.</div>';
    }
    return;
  }

  grid.innerHTML = filtered.map(item => `
    <div class="menu-card" onclick="addToCart(${item.id})">
      <div>
        <div class="menu-card-cat">${escapeHtml(item.category)}</div>
        <div class="menu-card-name">${escapeHtml(item.name)}</div>
      </div>
      <div class="menu-card-bottom">
        <span class="menu-card-price">${formatCurrency(item.price)}</span>
        <button class="btn btn-secondary btn-sm" style="border-radius: 50%; width: 28px; height: 28px; padding: 0;" title="Add to cart">+</button>
      </div>
    </div>
  `).join('');
}

/* Modal: Add Menu Item */
function openAddItemModal(suggestedName = '') {
  const modal = document.getElementById('add-item-modal');
  if (!modal) return;

  const nameInput = document.getElementById('new-item-name');
  const catSelect = document.getElementById('new-item-category');
  const customCatInput = document.getElementById('new-item-custom-category');
  const priceInput = document.getElementById('new-item-price');

  if (nameInput) nameInput.value = (typeof suggestedName === 'string') ? suggestedName : '';
  if (priceInput) priceInput.value = '';
  if (customCatInput) {
    customCatInput.value = '';
    customCatInput.style.display = 'none';
  }

  if (catSelect) {
    if (currentCategory && currentCategory !== 'All') {
      let optionExists = false;
      for (let i = 0; i < catSelect.options.length; i++) {
        if (catSelect.options[i].value === currentCategory) {
          catSelect.selectedIndex = i;
          optionExists = true;
          break;
        }
      }
      if (!optionExists) {
        catSelect.value = 'Beverages';
      }
    } else {
      catSelect.value = 'Beverages';
    }
  }

  modal.classList.add('active');
  setTimeout(() => {
    if (nameInput) nameInput.focus();
  }, 100);
}

function closeAddItemModal() {
  const modal = document.getElementById('add-item-modal');
  if (modal) modal.classList.remove('active');
}

function handleCategorySelectChange(val) {
  const customInput = document.getElementById('new-item-custom-category');
  if (!customInput) return;
  if (val === 'CUSTOM') {
    customInput.style.display = 'block';
    customInput.required = true;
    customInput.focus();
  } else {
    customInput.style.display = 'none';
    customInput.required = false;
  }
}

async function submitNewMenuItem() {
  const nameInput = document.getElementById('new-item-name');
  const catSelect = document.getElementById('new-item-category');
  const customCatInput = document.getElementById('new-item-custom-category');
  const priceInput = document.getElementById('new-item-price');
  const saveBtn = document.getElementById('btn-save-menu-item');

  const name = nameInput.value.trim();
  let category = catSelect.value;
  if (category === 'CUSTOM') {
    category = customCatInput.value.trim();
  }
  const price = parseFloat(priceInput.value);

  if (!name) {
    showToast('Please enter an item name.', 'error');
    if (nameInput) nameInput.focus();
    return;
  }
  if (!category) {
    showToast('Please specify a category.', 'error');
    return;
  }
  if (isNaN(price) || price < 0) {
    showToast('Please enter a valid price.', 'error');
    if (priceInput) priceInput.focus();
    return;
  }

  saveBtn.disabled = true;
  saveBtn.textContent = 'Saving...';

  try {
    const res = await apiFetch('/api/menu', {
      method: 'POST',
      body: JSON.stringify({ name, category, price })
    });

    if (res.success && res.item) {
      showToast(`Item "${res.item.name}" added to menu!`, 'success');
      allMenuItems.push(res.item);
      updateCategoryTabs();
      closeAddItemModal();
      clearMenuSearch();
      // Add the newly created item straight to the cart
      addToCart(res.item.id);
    } else {
      showToast(res.error || 'Failed to add menu item.', 'error');
    }
  } catch (err) {
    showToast('Error: ' + err.message, 'error');
  } finally {
    saveBtn.disabled = false;
    saveBtn.textContent = 'Save Item';
  }
}

function addToCart(itemId) {
  const item = allMenuItems.find(i => i.id === itemId);
  if (!item) return;

  const existing = cart.find(i => i.id === itemId);
  if (existing) {
    existing.quantity++;
  } else {
    cart.push({
      id: item.id,
      name: item.name,
      price: item.price,
      quantity: 1
    });
  }
  renderCart();
}

function updateQuantity(itemId, delta) {
  const existing = cart.find(i => i.id === itemId);
  if (!existing) return;

  existing.quantity += delta;
  if (existing.quantity <= 0) {
    cart = cart.filter(i => i.id !== itemId);
  }
  renderCart();
}

function clearCart() {
  cart = [];
  document.getElementById('customer-name-input').value = '';
  setOrderType('NORMAL');
  renderCart();
}

function setOrderType(type) {
  selectedOrderType = type;
  const btnNormal = document.getElementById('btn-type-normal');
  const btnPriority = document.getElementById('btn-type-priority');

  if (type === 'PRIORITY') {
    btnPriority.classList.add('active');
    btnNormal.classList.remove('active');
  } else {
    btnNormal.classList.add('active');
    btnPriority.classList.remove('active');
  }
}

function renderCart() {
  const listEl = document.getElementById('cart-items-list');
  const subtotalEl = document.getElementById('summary-subtotal');
  const totalEl = document.getElementById('summary-total');
  const btnGen = document.getElementById('btn-generate-token');

  if (cart.length === 0) {
    listEl.innerHTML = '<div class="empty-state"  style="font-weight: 100;">No items added yet. Click menu items on the left to add.</div>';
    subtotalEl.textContent = formatCurrency(0);
    totalEl.textContent = formatCurrency(0);
    btnGen.disabled = true;
    btnGen.style.opacity = '0.6';
    return;
  }

  btnGen.disabled = false;
  btnGen.style.opacity = '1';

  let total = 0;
  listEl.innerHTML = cart.map(item => {
    const itemSubtotal = item.price * item.quantity;
    total += itemSubtotal;
    return `
      <div class="cart-item">
        <div>
          <div class="cart-item-title">${escapeHtml(item.name)}</div>
          <div class="cart-item-price">${formatCurrency(item.price)} each</div>
        </div>
        <div class="qty-controls">
          <button class="qty-btn" onclick="updateQuantity(${item.id}, -1)">-</button>
          <span style="font-size: 13px; font-weight: 700; min-width: 18px; text-align: center;">${item.quantity}</span>
          <button class="qty-btn" onclick="updateQuantity(${item.id}, 1)">+</button>
          <span style="font-weight: 700; font-size: 13px; margin-left: 6px;">${formatCurrency(itemSubtotal)}</span>
        </div>
      </div>
    `;
  }).join('');

  subtotalEl.textContent = formatCurrency(total);
  totalEl.textContent = formatCurrency(total);
}

async function submitOrder() {
  if (cart.length === 0) {
    showToast('Please add at least one menu item to the order.', 'error');
    return;
  }

  const customerName = document.getElementById('customer-name-input').value.trim();
  const btn = document.getElementById('btn-generate-token');
  btn.disabled = true;
  btn.textContent = 'Generating Token...';

  const payload = {
    customerName: customerName || 'Guest',
    orderType: selectedOrderType,
    items: cart.map(item => ({
      itemId: item.id,
      itemName: item.name,
      quantity: item.quantity,
      unitPrice: item.price
    }))
  };

  try {
    const res = await apiFetch('/api/orders', {
      method: 'POST',
      body: JSON.stringify(payload)
    });

    if (res.success && res.order) {
      showToast(`Token ${res.order.token} created successfully!`, 'success');
      showTokenModal(res.order);
      clearCart();
      loadActiveOrders();
    } else {
      showToast(res.error || 'Failed to place order.', 'error');
    }
  } catch (err) {
    showToast('Error: ' + err.message, 'error');
  } finally {
    btn.disabled = false;
    btn.innerHTML = `<svg class="icon-svg" viewBox="0 0 24 24"><path d="M2 9a3 3 0 0 1 0 6v2a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-2a3 3 0 0 1 0-6V7a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2z"/><line x1="13" y1="5" x2="13" y2="7"/><line x1="13" y1="11" x2="13" y2="13"/><line x1="13" y1="17" x2="13" y2="19"/></svg> Generate Token`;
  }
}

function showTokenModal(order) {
  const modal = document.getElementById('token-modal');
  const tokenNum = document.getElementById('modal-token-number');
  const tokenType = document.getElementById('modal-token-type');
  const custName = document.getElementById('modal-customer-name');
  const ordTime = document.getElementById('modal-order-time');
  const ordTotal = document.getElementById('modal-order-total');

  tokenNum.textContent = order.token;
  if (order.orderType === 'PRIORITY') {
    tokenNum.className = 'token-big-num priority';
    tokenType.innerHTML = `<span class="badge badge-priority">PRIORITY ORDER</span>`;
  } else {
    tokenNum.className = 'token-big-num';
    tokenType.innerHTML = `<span class="badge badge-normal">NORMAL ORDER</span>`;
  }

  custName.textContent = 'Customer: ' + (order.customerName || 'Guest');
  ordTime.textContent = 'Placed at: ' + formatDateTime(order.createdAt);
  ordTotal.textContent = 'Total: ' + formatCurrency(order.totalAmount);

  const slipCafeName = document.getElementById('modal-slip-cafe-name');
  if (slipCafeName) {
    const cafeName = localStorage.getItem('cafe_name') || 'Campus Cafeteria';
    slipCafeName.textContent = cafeName;
  }

  modal.classList.add('active');
}

function printTokenSlip() {
  window.print();
}

function closeTokenModal() {
  document.getElementById('token-modal').classList.remove('active');
}

async function loadActiveOrders() {
  try {
    const data = await apiFetch('/api/orders/active');
    if (!data) return;

    const tbody = document.getElementById('cashier-active-orders-tbody');
    const badge = document.getElementById('cashier-active-count');
    
    const all = [
      ...(data.waiting || []),
      ...(data.preparing || []),
      ...(data.ready || [])
    ];

    if (badge) {
      badge.textContent = `${all.length} Active Orders`;
    }

    if (!tbody) return;

    if (all.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" class="empty-state">No active orders right now.</td></tr>';
      return;
    }

    tbody.innerHTML = all.map(ord => {
      const isPriority = ord.orderType === 'PRIORITY';
      return `
        <tr>
          <td>${renderTokenBadge(ord.token, isPriority)}</td>
          <td><strong>${escapeHtml(ord.customerName)}</strong></td>
          <td>${renderTypeBadge(ord.orderType)}</td>
          <td>${formatCurrency(ord.totalAmount)}</td>
          <td>${renderStatusBadge(ord.status)}</td>
          <td style="color: var(--text-muted); font-size: 12px;">${formatDateTime(ord.createdAt)}</td>
        </tr>
      `;
    }).join('');

  } catch (err) {
    console.error('Failed to load active orders:', err);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  loadMenu();
  loadActiveOrders();
  setInterval(loadActiveOrders, 5000);
});
