#include "Database.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cctype>

Database::Database(const std::string& dbPath) : dbPath_(dbPath) {}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database Error] " << (err ? err : "Unknown") << " in SQL: " << sql << std::endl;
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

bool Database::init() {
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "[Database Error] Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    // Enable WAL mode for high concurrency
    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA foreign_keys=ON;");

    // Create tables
    std::string createMenuTable = 
        "CREATE TABLE IF NOT EXISTS menu_items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  category TEXT NOT NULL,"
        "  price REAL NOT NULL,"
        "  is_available INTEGER DEFAULT 1"
        ");";

    std::string createOrdersTable = 
        "CREATE TABLE IF NOT EXISTS orders ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  token TEXT NOT NULL,"
        "  customer_name TEXT,"
        "  order_type TEXT NOT NULL,"
        "  status TEXT NOT NULL,"
        "  total_amount REAL NOT NULL,"
        "  sequence_number INTEGER NOT NULL,"
        "  created_at TEXT NOT NULL,"
        "  updated_at TEXT NOT NULL"
        ");";

    std::string createOrderItemsTable = 
        "CREATE TABLE IF NOT EXISTS order_items ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  order_id INTEGER NOT NULL,"
        "  item_id INTEGER NOT NULL,"
        "  item_name TEXT NOT NULL,"
        "  quantity INTEGER NOT NULL,"
        "  unit_price REAL NOT NULL,"
        "  subtotal REAL NOT NULL,"
        "  FOREIGN KEY(order_id) REFERENCES orders(id) ON DELETE CASCADE"
        ");";

    std::string createDevicesTable = 
        "CREATE TABLE IF NOT EXISTS devices ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  token TEXT UNIQUE NOT NULL,"
        "  name TEXT NOT NULL,"
        "  role TEXT NOT NULL,"
        "  ip_address TEXT,"
        "  created_at TEXT NOT NULL,"
        "  last_connected TEXT NOT NULL,"
        "  last_connected_ts INTEGER NOT NULL DEFAULT 0,"
        "  is_active INTEGER DEFAULT 1"
        ");";

    std::string createPairingCodesTable = 
        "CREATE TABLE IF NOT EXISTS pairing_codes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  code TEXT UNIQUE NOT NULL,"
        "  role TEXT NOT NULL,"
        "  expires_at INTEGER NOT NULL,"
        "  is_used INTEGER DEFAULT 0,"
        "  attempts INTEGER DEFAULT 0,"
        "  created_at TEXT NOT NULL"
        ");";

    std::string createAppSettingsTable = 
        "CREATE TABLE IF NOT EXISTS app_settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");";

    std::string createRevenueAdjustmentsTable = 
        "CREATE TABLE IF NOT EXISTS revenue_adjustments ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  reset_at TEXT NOT NULL,"
        "  amount REAL NOT NULL DEFAULT 0.0,"
        "  reason TEXT"
        ");";

    if (!execute(createMenuTable) || 
        !execute(createOrdersTable) || 
        !execute(createOrderItemsTable) ||
        !execute(createDevicesTable) ||
        !execute(createPairingCodesTable) ||
        !execute(createAppSettingsTable) ||
        !execute(createRevenueAdjustmentsTable)) {
        return false;
    }

    execute("CREATE INDEX IF NOT EXISTS idx_orders_token ON orders(token);");
    execute("CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status);");

    seedMenuItemsIfEmpty();
    seedSettingsIfEmpty();
    return true;
}

void Database::seedMenuItemsIfEmpty() {
    sqlite3_stmt* stmt = nullptr;
    const char* countSql = "SELECT COUNT(*) FROM menu_items;";
    if (sqlite3_prepare_v2(db_, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            if (count > 0) {
                sqlite3_finalize(stmt);
                return;
            }
        }
    }
    if (stmt) sqlite3_finalize(stmt);

    std::cout << "[Database] Seeding initial cafeteria menu items...\n";

    struct InitialItem {
        const char* name;
        const char* category;
        double price;
    };

    InitialItem items[] = {
        {"Espresso", "Beverages", 80.0},
        {"Cappuccino", "Beverages", 120.0},
        {"Caramel Cold Coffee", "Beverages", 150.0},
        {"Cafe Latte", "Beverages", 130.0},
        {"Masala Chai", "Beverages", 40.0},
        {"Peach Iced Tea", "Beverages", 90.0},
        {"Grilled Cheese Panini", "Sandwiches & Snacks", 140.0},
        {"Veggie Club Sandwich", "Sandwiches & Snacks", 160.0},
        {"Paneer Tikka Wrap", "Sandwiches & Snacks", 180.0},
        {"Crispy Chicken Burger", "Sandwiches & Snacks", 210.0},
        {"French Fries (Peri-Peri)", "Sandwiches & Snacks", 110.0},
        {"Chocolate Walnut Brownie", "Desserts & Bakery", 120.0},
        {"Blueberry Muffin", "Desserts & Bakery", 95.0},
        {"Butter Croissant", "Desserts & Bakery", 110.0},
        {"Choco Lava Cake", "Desserts & Bakery", 135.0}
    };

    const char* insertSql = "INSERT INTO menu_items (name, category, price, is_available) VALUES (?, ?, ?, 1);";
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    for (const auto& item : items) {
        sqlite3_stmt* insStmt = nullptr;
        if (sqlite3_prepare_v2(db_, insertSql, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(insStmt, 1, item.name, -1, SQLITE_STATIC);
            sqlite3_bind_text(insStmt, 2, item.category, -1, SQLITE_STATIC);
            sqlite3_bind_double(insStmt, 3, item.price);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
        }
    }

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

void Database::seedSettingsIfEmpty() {
    sqlite3_stmt* stmt = nullptr;
    const char* countSql = "SELECT COUNT(*) FROM app_settings;";
    if (sqlite3_prepare_v2(db_, countSql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int count = sqlite3_column_int(stmt, 0);
            if (count > 0) {
                sqlite3_finalize(stmt);
                return;
            }
        }
    }
    if (stmt) sqlite3_finalize(stmt);

    std::cout << "[Database] Seeding default application settings...\n";

    std::vector<std::pair<std::string, std::string>> defaults = {
        {"cafe_name", "Campus Cafeteria"},
        {"currency_symbol", "₹"},
        {"token_prefix_normal", "N-"},
        {"token_prefix_priority", "P-"},
        {"token_format", "from_101"},
        {"token_next_normal", "101"},
        {"token_next_priority", "101"},
        {"enable_priority_orders", "true"},
        {"enable_cancellations", "true"},
        {"default_order_status", "WAITING"},
        {"time_format", "12h"},
        {"tv_business_name", "Campus Cafeteria"},
        {"tv_show_clock", "true"},
        {"tv_show_completed", "true"},
        {"tv_completed_limit", "10"},
        {"sound_notifications", "true"},
        {"initial_setup_completed", "false"}
    };

    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    const char* insSql = "INSERT OR IGNORE INTO app_settings (key, value) VALUES (?, ?);";
    for (const auto& kv : defaults) {
        sqlite3_stmt* insStmt = nullptr;
        if (sqlite3_prepare_v2(db_, insSql, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(insStmt, 1, kv.first.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insStmt, 2, kv.second.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
        }
    }
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<MenuItem> Database::getMenuItems() {
    std::vector<MenuItem> list;
    const char* sql = "SELECT id, name, category, price, is_available FROM menu_items WHERE is_available = 1 ORDER BY category, id;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MenuItem item;
            item.id = sqlite3_column_int(stmt, 0);
            item.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            item.category = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            item.price = sqlite3_column_double(stmt, 3);
            item.isAvailable = sqlite3_column_int(stmt, 4) != 0;
            list.push_back(item);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool Database::addMenuItem(const std::string& name, const std::string& category, double price, MenuItem& outItem) {
    const char* sql = "INSERT INTO menu_items (name, category, price, is_available) VALUES (?, ?, ?, 1);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, category.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, price);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    outItem.id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    outItem.name = name;
    outItem.category = category;
    outItem.price = price;
    outItem.isAvailable = true;
    return true;
}

bool Database::saveOrder(Order& order) {
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* orderSql = 
        "INSERT INTO orders (token, customer_name, order_type, status, total_amount, sequence_number, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db_, orderSql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    sqlite3_bind_text(stmt, 1, order.token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, order.customerName.c_str(), -1, SQLITE_TRANSIENT);
    std::string typeStr = orderTypeToString(order.type);
    sqlite3_bind_text(stmt, 3, typeStr.c_str(), -1, SQLITE_TRANSIENT);
    std::string statusStr = orderStatusToString(order.status);
    sqlite3_bind_text(stmt, 4, statusStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, order.totalAmount);
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(order.sequenceNumber));
    sqlite3_bind_text(stmt, 7, order.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, order.updatedAt.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "[Database Error] Failed to insert order: " << sqlite3_errmsg(db_) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_finalize(stmt);

    order.id = static_cast<int>(sqlite3_last_insert_rowid(db_));

    const char* itemSql = 
        "INSERT INTO order_items (order_id, item_id, item_name, quantity, unit_price, subtotal) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    for (const auto& it : order.items) {
        sqlite3_stmt* itemStmt = nullptr;
        if (sqlite3_prepare_v2(db_, itemSql, -1, &itemStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(itemStmt, 1, order.id);
            sqlite3_bind_int(itemStmt, 2, it.itemId);
            sqlite3_bind_text(itemStmt, 3, it.itemName.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(itemStmt, 4, it.quantity);
            sqlite3_bind_double(itemStmt, 5, it.unitPrice);
            sqlite3_bind_double(itemStmt, 6, it.subtotal);
            sqlite3_step(itemStmt);
            sqlite3_finalize(itemStmt);
        }
    }

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool Database::updateOrderStatus(const std::string& token, OrderStatus status, const std::string& updatedAt) {
    const char* sql = "UPDATE orders SET status = ?, updated_at = ? WHERE id = ("
                      "  SELECT id FROM orders WHERE token = ? AND status != 'COMPLETED' AND status != 'CANCELLED' ORDER BY id DESC LIMIT 1"
                      ");";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string statusStr = orderStatusToString(status);
    sqlite3_bind_text(stmt, 1, statusStr.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, token.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (sqlite3_changes(db_) == 0) {
        const char* fallbackSql = "UPDATE orders SET status = ?, updated_at = ? WHERE id = ("
                                  "  SELECT id FROM orders WHERE token = ? ORDER BY id DESC LIMIT 1"
                                  ");";
        if (sqlite3_prepare_v2(db_, fallbackSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, statusStr.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, updatedAt.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, token.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    return (rc == SQLITE_DONE);
}

static void loadItemsForOrder(sqlite3* db, Order& order) {
    const char* sql = "SELECT item_id, item_name, quantity, unit_price, subtotal FROM order_items WHERE order_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, order.id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            OrderItem it;
            it.itemId = sqlite3_column_int(stmt, 0);
            it.itemName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            it.quantity = sqlite3_column_int(stmt, 2);
            it.unitPrice = sqlite3_column_double(stmt, 3);
            it.subtotal = sqlite3_column_double(stmt, 4);
            order.items.push_back(it);
        }
        sqlite3_finalize(stmt);
    }
}

bool Database::getOrder(const std::string& token, Order& outOrder) {
    const char* sql = 
        "SELECT id, token, customer_name, order_type, status, total_amount, sequence_number, created_at, updated_at "
        "FROM orders WHERE token = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        outOrder.id = sqlite3_column_int(stmt, 0);
        outOrder.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const unsigned char* custName = sqlite3_column_text(stmt, 2);
        outOrder.customerName = custName ? reinterpret_cast<const char*>(custName) : "";
        outOrder.type = stringToOrderType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        outOrder.status = stringToOrderStatus(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        outOrder.totalAmount = sqlite3_column_double(stmt, 5);
        outOrder.sequenceNumber = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
        outOrder.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        outOrder.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        sqlite3_finalize(stmt);

        loadItemsForOrder(db_, outOrder);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

std::vector<Order> Database::getActiveOrders() {
    std::vector<Order> orders;
    const char* sql = 
        "SELECT id, token, customer_name, order_type, status, total_amount, sequence_number, created_at, updated_at "
        "FROM orders WHERE status != 'COMPLETED' ORDER BY sequence_number ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Order order;
            order.id = sqlite3_column_int(stmt, 0);
            order.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const unsigned char* custName = sqlite3_column_text(stmt, 2);
            order.customerName = custName ? reinterpret_cast<const char*>(custName) : "";
            order.type = stringToOrderType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            order.status = stringToOrderStatus(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
            order.totalAmount = sqlite3_column_double(stmt, 5);
            order.sequenceNumber = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
            order.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            order.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            orders.push_back(order);
        }
        sqlite3_finalize(stmt);
    }

    for (auto& o : orders) {
        loadItemsForOrder(db_, o);
    }
    return orders;
}

std::vector<Order> Database::getOrderHistory(int limit, const std::string& search) {
    std::vector<Order> orders;
    std::string sql = 
        "SELECT id, token, customer_name, order_type, status, total_amount, sequence_number, created_at, updated_at "
        "FROM orders WHERE 1=1 ";

    if (!search.empty()) {
        sql += "AND (token LIKE ? OR customer_name LIKE ?) ";
    }
    sql += "ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        int paramIdx = 1;
        if (!search.empty()) {
            std::string wildcard = "%" + search + "%";
            sqlite3_bind_text(stmt, paramIdx++, wildcard.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, paramIdx++, wildcard.c_str(), -1, SQLITE_TRANSIENT);
        }
        sqlite3_bind_int(stmt, paramIdx, limit);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Order order;
            order.id = sqlite3_column_int(stmt, 0);
            order.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const unsigned char* custName = sqlite3_column_text(stmt, 2);
            order.customerName = custName ? reinterpret_cast<const char*>(custName) : "";
            order.type = stringToOrderType(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
            order.status = stringToOrderStatus(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
            order.totalAmount = sqlite3_column_double(stmt, 5);
            order.sequenceNumber = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
            order.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            order.updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            orders.push_back(order);
        }
        sqlite3_finalize(stmt);
    }

    for (auto& o : orders) {
        loadItemsForOrder(db_, o);
    }
    return orders;
}

bool Database::deleteOrderById(int id) {
    if (!db_) return false;

    // Delete associated line items first
    std::string sqlItems = "DELETE FROM order_items WHERE order_id = ?;";
    sqlite3_stmt* stmtItems = nullptr;
    if (sqlite3_prepare_v2(db_, sqlItems.c_str(), -1, &stmtItems, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmtItems, 1, id);
        sqlite3_step(stmtItems);
        sqlite3_finalize(stmtItems);
    }

    // Delete the order record
    std::string sqlOrder = "DELETE FROM orders WHERE id = ?;";
    sqlite3_stmt* stmtOrder = nullptr;
    if (sqlite3_prepare_v2(db_, sqlOrder.c_str(), -1, &stmtOrder, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmtOrder, 1, id);
    int rc = sqlite3_step(stmtOrder);
    sqlite3_finalize(stmtOrder);
    return (rc == SQLITE_DONE);
}

bool Database::deleteAllOrders() {
    if (!db_) return false;
    char* errMsg = nullptr;
    sqlite3_exec(db_, "DELETE FROM order_items;", nullptr, nullptr, &errMsg);
    int rc = sqlite3_exec(db_, "DELETE FROM orders;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) {
            std::cerr << "[Database Error] Delete all orders failed: " << errMsg << "\n";
            sqlite3_free(errMsg);
        }
        return false;
    }
    return true;
}


DashboardStats Database::getDashboardStats() {
    DashboardStats stats;
    stats.revenueResetAt = getLatestRevenueResetTime();

    const char* sql = 
        "SELECT "
        "  COUNT(*), "
        "  SUM(CASE WHEN status = 'WAITING' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN status = 'PREPARING' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN status = 'READY' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN status = 'COMPLETED' THEN 1 ELSE 0 END), "
        "  SUM(CASE WHEN status = 'COMPLETED' AND created_at >= COALESCE((SELECT MAX(reset_at) FROM revenue_adjustments), '1970-01-01') THEN total_amount ELSE 0 END), "
        "  SUM(CASE WHEN status = 'COMPLETED' THEN total_amount ELSE 0 END) "
        "FROM orders;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.totalOrders = sqlite3_column_int(stmt, 0);
            stats.waitingOrders = sqlite3_column_int(stmt, 1);
            stats.preparingOrders = sqlite3_column_int(stmt, 2);
            stats.readyOrders = sqlite3_column_int(stmt, 3);
            stats.completedOrders = sqlite3_column_int(stmt, 4);
            stats.totalRevenue = sqlite3_column_double(stmt, 5);
            stats.allTimeRevenue = sqlite3_column_double(stmt, 6);
        }
        sqlite3_finalize(stmt);
    }
    return stats;
}

uint64_t Database::getMaxSequenceNumber() {
    uint64_t maxSeq = 0;
    const char* sql = "SELECT MAX(sequence_number) FROM orders;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            maxSeq = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    return maxSeq;
}

int Database::getLastTokenNumber(OrderType type) {
    int maxNum = 100; // Start at 100 so first token is 101
    const char* prefix = (type == OrderType::PRIORITY) ? "P-" : "N-";
    std::string sql = "SELECT token FROM orders WHERE token LIKE '" + std::string(prefix) + "%' ORDER BY id DESC LIMIT 50;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string tok = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (tok.rfind(prefix, 0) == 0) {
                try {
                    int num = std::stoi(tok.substr(2));
                    if (num > maxNum) maxNum = num;
                } catch (...) {}
            }
        }
        sqlite3_finalize(stmt);
    }
    return maxNum;
}

// -------------------------------------------------------------
// Settings Operations
// -------------------------------------------------------------

std::string Database::getSetting(const std::string& key, const std::string& defaultVal) {
    std::string val = defaultVal;
    const char* sql = "SELECT value FROM app_settings WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(stmt, 0);
            if (txt) val = reinterpret_cast<const char*>(txt);
        }
        sqlite3_finalize(stmt);
    }
    return val;
}

bool Database::setSetting(const std::string& key, const std::string& val) {
    const char* sql = "INSERT INTO app_settings (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, val.c_str(), -1, SQLITE_STATIC);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

std::map<std::string, std::string> Database::getAllSettings() {
    std::map<std::string, std::string> map;
    const char* sql = "SELECT key, value FROM app_settings;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string k = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            std::string v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            map[k] = v;
        }
        sqlite3_finalize(stmt);
    }
    return map;
}

bool Database::resetRevenue(const std::string& reason) {
    const char* sql = "INSERT INTO revenue_adjustments (reset_at, amount, reason) VALUES (?, 0.0, ?);";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        std::string nowStr = getCurrentTimestamp();
        sqlite3_bind_text(stmt, 1, nowStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, reason.c_str(), -1, SQLITE_STATIC);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

std::string Database::getLatestRevenueResetTime() {
    std::string resetAt = "";
    const char* sql = "SELECT MAX(reset_at) FROM revenue_adjustments;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(stmt, 0);
            if (txt) resetAt = reinterpret_cast<const char*>(txt);
        }
        sqlite3_finalize(stmt);
    }
    return resetAt;
}

// -------------------------------------------------------------
// Device Authentication & Management
// -------------------------------------------------------------

bool Database::registerDevice(const std::string& token, const std::string& name, const std::string& role, const std::string& ipAddress, DeviceRecord& outDevice) {
    const char* sql = "INSERT INTO devices (token, name, role, ip_address, created_at, last_connected, last_connected_ts, is_active) VALUES (?, ?, ?, ?, ?, ?, ?, 1);";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    std::string nowStr = getCurrentTimestamp();
    int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, ipAddress.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, nowStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, nowStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 7, nowSec);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            outDevice.id = static_cast<int>(sqlite3_last_insert_rowid(db_));
            outDevice.token = token;
            outDevice.name = name;
            outDevice.role = role;
            outDevice.ipAddress = ipAddress;
            outDevice.createdAt = nowStr;
            outDevice.lastConnected = nowStr;
            outDevice.isActive = true;
            outDevice.isOnline = true;
            ok = true;
        }
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool Database::getDeviceByToken(const std::string& token, DeviceRecord& outDevice) {
    const char* sql = "SELECT id, token, name, role, ip_address, created_at, last_connected, last_connected_ts, is_active FROM devices WHERE token = ? AND is_active = 1;";
    sqlite3_stmt* stmt = nullptr;
    bool found = false;
    int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outDevice.id = sqlite3_column_int(stmt, 0);
            outDevice.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            outDevice.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            outDevice.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const unsigned char* ip = sqlite3_column_text(stmt, 4);
            outDevice.ipAddress = ip ? reinterpret_cast<const char*>(ip) : "";
            outDevice.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            outDevice.lastConnected = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            int64_t lastTs = sqlite3_column_int64(stmt, 7);
            outDevice.isActive = (sqlite3_column_int(stmt, 8) == 1);
            outDevice.isOnline = outDevice.isActive && ((nowSec - lastTs) <= 25);
            found = true;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

std::vector<DeviceRecord> Database::getAllDevices() {
    std::vector<DeviceRecord> devices;
    const char* sql = "SELECT id, token, name, role, ip_address, created_at, last_connected, last_connected_ts, is_active FROM devices WHERE is_active = 1 ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;
    int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DeviceRecord d;
            d.id = sqlite3_column_int(stmt, 0);
            d.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            d.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            d.role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            const unsigned char* ip = sqlite3_column_text(stmt, 4);
            d.ipAddress = ip ? reinterpret_cast<const char*>(ip) : "";
            d.createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            d.lastConnected = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            int64_t lastTs = sqlite3_column_int64(stmt, 7);
            d.isActive = (sqlite3_column_int(stmt, 8) == 1);
            d.isOnline = d.isActive && ((nowSec - lastTs) <= 25);
            devices.push_back(d);
        }
        sqlite3_finalize(stmt);
    }
    return devices;
}

bool Database::updateDeviceName(int id, const std::string& name) {
    const char* sql = "UPDATE devices SET name = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool Database::revokeDevice(int id) {
    const char* sql = "UPDATE devices SET is_active = 0 WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

bool Database::updateDeviceHeartbeat(const std::string& token, const std::string& ipAddress) {
    const char* sql = "UPDATE devices SET last_connected = ?, last_connected_ts = ?, ip_address = CASE WHEN ? != '' THEN ? ELSE ip_address END WHERE token = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    std::string nowStr = getCurrentTimestamp();
    int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, nowStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, nowSec);
        sqlite3_bind_text(stmt, 3, ipAddress.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, ipAddress.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, token.c_str(), -1, SQLITE_STATIC);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

// -------------------------------------------------------------
// Pairing Code Operations
// -------------------------------------------------------------

bool Database::createPairingCode(const std::string& code, const std::string& role, int64_t expiresAt) {
    const char* sql = "INSERT INTO pairing_codes (code, role, expires_at, is_used, attempts, created_at) VALUES (?, ?, ?, 0, 0, ?);";
    sqlite3_stmt* stmt = nullptr;
    bool ok = false;
    std::string nowStr = getCurrentTimestamp();
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, role.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, expiresAt);
        sqlite3_bind_text(stmt, 4, nowStr.c_str(), -1, SQLITE_STATIC);
        ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return ok;
}

static std::string stripCodeFormatting(const std::string& str) {
    std::string clean;
    for (char c : str) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            clean += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return clean;
}

bool Database::verifyAndUsePairingCode(const std::string& inputCode, std::string& outRole, std::string& errorMsg) {
    cleanupExpiredPairingCodes();

    std::string cleanInput = stripCodeFormatting(inputCode);
    if (cleanInput.empty()) {
        errorMsg = "Pairing code cannot be empty.";
        return false;
    }

    const char* sql = "SELECT id, code, role, expires_at, is_used, attempts FROM pairing_codes WHERE is_used = 0 ORDER BY id DESC;";
    sqlite3_stmt* stmt = nullptr;
    int targetId = 0;
    std::string targetRole;
    int64_t expiresAt = 0;
    int attempts = 0;
    bool found = false;

    int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string dbCode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::string cleanDbCode = stripCodeFormatting(dbCode);
            if (cleanDbCode == cleanInput) {
                targetId = sqlite3_column_int(stmt, 0);
                targetRole = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                expiresAt = sqlite3_column_int64(stmt, 3);
                attempts = sqlite3_column_int(stmt, 5);
                found = true;
                break;
            }
        }
        sqlite3_finalize(stmt);
    }

    if (!found) {
        errorMsg = "Invalid pairing code. Please verify the 6-digit code.";
        return false;
    }

    if (attempts >= 5) {
        errorMsg = "Too many failed attempts. This code has been invalidated.";
        return false;
    }

    if (nowSec > expiresAt) {
        errorMsg = "Pairing code has expired. Please generate a new code from Cashier settings.";
        return false;
    }

    // Mark code as used
    const char* markSql = "UPDATE pairing_codes SET is_used = 1 WHERE id = ?;";
    sqlite3_stmt* markStmt = nullptr;
    if (sqlite3_prepare_v2(db_, markSql, -1, &markStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(markStmt, 1, targetId);
        sqlite3_step(markStmt);
        sqlite3_finalize(markStmt);
    }

    outRole = targetRole;
    return true;
}

void Database::cleanupExpiredPairingCodes() {
    int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    // Delete codes that expired more than 1 hour ago
    std::string sql = "DELETE FROM pairing_codes WHERE expires_at < " + std::to_string(nowSec - 3600) + ";";
    execute(sql);
}
