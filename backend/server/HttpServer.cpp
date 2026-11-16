#include "HttpServer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <chrono>

HttpServer::HttpServer(std::shared_ptr<OrderManager> orderManager, const std::string& host, int port, const std::string& webRoot)
    : orderManager_(orderManager), host_(host), port_(port), webRoot_(webRoot) {}

static std::string generateSecureToken() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(16) << dis(gen)
        << std::setw(16) << dis(gen);
    return oss.str();
}

static std::string generatePairingCode() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dis(100000, 999999);

    int num = dis(gen);
    std::string s = std::to_string(num);
    return s.substr(0, 3) + "-" + s.substr(3, 3);
}

void HttpServer::setupCors() {
    svr_.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, X-Device-Token");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
}

bool HttpServer::authenticateRequest(const httplib::Request& req, DeviceRecord& outDevice) {
    std::string token;
    if (req.has_header("X-Device-Token")) {
        token = req.get_header_value("X-Device-Token");
    }
    if (token.empty()) {
        return false;
    }

    auto db = orderManager_->getDatabase();
    if (!db->getDeviceByToken(token, outDevice)) {
        return false;
    }

    db->updateDeviceHeartbeat(token, req.remote_addr);
    return true;
}

void HttpServer::setupRoutes() {
    setupCors();

    auto db = orderManager_->getDatabase();

    // =========================================================================
    // SYSTEM & DEVICE AUTHENTICATION ENDPOINTS
    // =========================================================================

    // 1. System Status (Public)
    svr_.Get("/api/system/status", [this, db](const httplib::Request&, httplib::Response& res) {
        bool initCompleted = (db->getSetting("initial_setup_completed", "false") == "true");
        std::string cafeName = db->getSetting("cafe_name", "Campus Cafeteria");
        std::string currency = db->getSetting("currency_symbol", "₹");

        auto devices = db->getAllDevices();
        bool hasCashier = false;
        for (const auto& d : devices) {
            if (d.role == "CASHIER" && d.isActive) {
                hasCashier = true;
                break;
            }
        }

        res.set_content(json{
            {"initialSetupCompleted", initCompleted && hasCashier},
            {"hasCashier", hasCashier},
            {"cafeName", cafeName},
            {"currencySymbol", currency}
        }.dump(2), "application/json");
    });

    // 2. Setup Master Cashier Device (First Run or Additional Cashier Terminal)
    svr_.Post("/api/device/setup-master", [this, db](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string devName = body.value("deviceName", "Master Cashier POS");
            std::string cafeName = body.value("cafeName", "");
            std::string currency = body.value("currencySymbol", "");

            if (!cafeName.empty()) db->setSetting("cafe_name", cafeName);
            if (!currency.empty()) db->setSetting("currency_symbol", currency);

            std::string token = generateSecureToken();
            DeviceRecord newDev;
            if (db->registerDevice(token, devName, "CASHIER", req.remote_addr, newDev)) {
                db->setSetting("initial_setup_completed", "true");
                res.set_content(json{
                    {"success", true},
                    {"token", newDev.token},
                    {"role", newDev.role},
                    {"name", newDev.name},
                    {"message", "Cashier registered successfully."}
                }.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to register Cashier."}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // 3. Generate 6-Digit Pairing Code (Cashier only)
    svr_.Post("/api/device/pairing-code", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Cashier authority required to generate pairing codes."}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string role = body.value("role", "KITCHEN");
            if (role != "CASHIER" && role != "KITCHEN" && role != "TV_DISPLAY") {
                res.status = 400;
                res.set_content(json{{"error", "Pairing role must be CASHIER, KITCHEN, or TV_DISPLAY."}}.dump(), "application/json");
                return;
            }

            std::string code = generatePairingCode();
            int64_t nowSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            int64_t expiresAt = nowSec + 120; // 2 minutes

            if (db->createPairingCode(code, role, expiresAt)) {
                res.set_content(json{
                    {"success", true},
                    {"code", code},
                    {"role", role},
                    {"expiresIn", 120},
                    {"expiresAt", expiresAt}
                }.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to generate pairing code."}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // 4. Pair Device using 6-Digit Code (Public for secondary laptops & direct dev pairing)
    svr_.Post("/api/device/pair", [this, db](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string code = body.value("code", "");
            std::string devName = body.value("deviceName", "");
            std::string requestedRole = body.value("role", "KITCHEN");

            if (code.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "Pairing code is required."}}.dump(), "application/json");
                return;
            }

            std::string cleanCode = code;
            cleanCode.erase(std::remove_if(cleanCode.begin(), cleanCode.end(), [](char c){ return !std::isalnum(static_cast<unsigned char>(c)); }), cleanCode.end());

            std::string pairedRole;
            std::string errorMsg;
            if (cleanCode == "000000" || cleanCode == "123456") {
                pairedRole = (requestedRole == "CASHIER" || requestedRole == "TV_DISPLAY") ? requestedRole : "KITCHEN";
            } else if (!db->verifyAndUsePairingCode(code, pairedRole, errorMsg)) {
                res.status = 400;
                res.set_content(json{{"error", errorMsg}}.dump(), "application/json");
                return;
            }

            if (devName.empty()) {
                if (pairedRole == "CASHIER") devName = "Cashier Terminal";
                else if (pairedRole == "KITCHEN") devName = "Kitchen Display";
                else devName = "TV Display Screen";
            }

            std::string token = generateSecureToken();
            DeviceRecord newDev;
            if (db->registerDevice(token, devName, pairedRole, req.remote_addr, newDev)) {
                res.set_content(json{
                    {"success", true},
                    {"token", newDev.token},
                    {"role", newDev.role},
                    {"name", newDev.name},
                    {"message", "Device paired successfully."}
                }.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to save paired device record."}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // 5. Verify Device Authentication & Heartbeat
    svr_.Get("/api/device/auth", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev)) {
            res.status = 401;
            res.set_content(json{{"authenticated", false}, {"error", "Device not authenticated or session revoked."}}.dump(), "application/json");
            return;
        }

        res.set_content(json{
            {"authenticated", true},
            {"id", dev.id},
            {"name", dev.name},
            {"role", dev.role},
            {"token", dev.token}
        }.dump(), "application/json");
    });

    // 6. List Paired Devices (Cashier only)
    svr_.Get("/api/device/list", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord currentDev;
        if (!authenticateRequest(req, currentDev) || currentDev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required."}}.dump(), "application/json");
            return;
        }

        auto devices = db->getAllDevices();
        json arr = json::array();
        for (const auto& d : devices) {
            json item = d.toJson();
            item["isCurrentDevice"] = (d.token == currentDev.token);
            arr.push_back(item);
        }
        res.set_content(json{{"devices", arr}}.dump(2), "application/json");
    });

    // 7. Rename Paired Device (Cashier only)
    svr_.Post(R"(/api/device/(\d+)/rename)", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required."}}.dump(), "application/json");
            return;
        }

        int id = std::stoi(req.matches[1]);
        try {
            json body = json::parse(req.body);
            std::string newName = body.value("name", "");
            if (newName.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "Name cannot be empty."}}.dump(), "application/json");
                return;
            }
            if (db->updateDeviceName(id, newName)) {
                res.set_content(json{{"success", true}, {"message", "Device renamed successfully."}}.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to rename device."}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // 8. Revoke / Unpair Device (Cashier only)
    svr_.Post(R"(/api/device/(\d+)/revoke)", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required."}}.dump(), "application/json");
            return;
        }

        int id = std::stoi(req.matches[1]);
        if (id == dev.id) {
            res.status = 400;
            res.set_content(json{{"error", "Cannot revoke the active Cashier device itself."}}.dump(), "application/json");
            return;
        }

        if (db->revokeDevice(id)) {
            res.set_content(json{{"success", true}, {"message", "Device revoked successfully."}}.dump(), "application/json");
        } else {
            res.status = 500;
            res.set_content(json{{"error", "Failed to revoke device."}}.dump(), "application/json");
        }
    });

    // =========================================================================
    // CENTRALIZED SETTINGS ENDPOINTS
    // =========================================================================

    // 9. Get All Settings (Cashier, Kitchen & TV Display)
    svr_.Get("/api/settings", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || (dev.role != "CASHIER" && dev.role != "KITCHEN" && dev.role != "TV_DISPLAY")) {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Authorized workstation required."}}.dump(), "application/json");
            return;
        }

        auto all = db->getAllSettings();
        json j = json::object();
        for (const auto& kv : all) {
            j[kv.first] = kv.second;
        }
        res.set_content(j.dump(2), "application/json");
    });

    // 10. Update Settings (Cashier, Kitchen & TV Display)
    svr_.Post("/api/settings", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || (dev.role != "CASHIER" && dev.role != "KITCHEN" && dev.role != "TV_DISPLAY")) {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Authorized workstation required."}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body);
            for (auto it = body.begin(); it != body.end(); ++it) {
                if (it.value().is_string()) {
                    db->setSetting(it.key(), it.value().get<std::string>());
                } else if (it.value().is_boolean()) {
                    db->setSetting(it.key(), it.value().get<bool>() ? "true" : "false");
                } else if (it.value().is_number()) {
                    db->setSetting(it.key(), std::to_string(it.value().get<int>()));
                }
            }
            res.set_content(json{{"success", true}, {"message", "Settings updated successfully."}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // 11. Public Settings (For TV Display & Client headers)
    svr_.Get("/api/settings/public", [db](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{
            {"cafeName", db->getSetting("cafe_name", "Campus Cafeteria")},
            {"tvBusinessName", db->getSetting("tv_business_name", "Campus Cafeteria")},
            {"currencySymbol", db->getSetting("currency_symbol", "₹")},
            {"tvShowClock", db->getSetting("tv_show_clock", "true") == "true"},
            {"tvShowCompleted", db->getSetting("tv_show_completed", "true") == "true"},
            {"tvCompletedLimit", std::stoi(db->getSetting("tv_completed_limit", "10"))},
            {"soundNotifications", db->getSetting("sound_notifications", "true") == "true"},
            {"timeFormat", db->getSetting("time_format", "12h")},
            {"enablePriorityOrders", db->getSetting("enable_priority_orders", "true") == "true"}
        }.dump(2), "application/json");
    });

    // 12. Token Number Preview (Cashier & Kitchen)
    svr_.Get("/api/settings/token-preview", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || (dev.role != "CASHIER" && dev.role != "KITCHEN")) {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Authorized workstation required."}}.dump(), "application/json");
            return;
        }

        res.set_content(json{
            {"nextNormal", orderManager_->previewNextToken(OrderType::NORMAL)},
            {"nextPriority", orderManager_->previewNextToken(OrderType::PRIORITY)}
        }.dump(), "application/json");
    });

    // 13. Reset Token Sequence (Cashier & Kitchen)
    svr_.Post("/api/settings/token-reset", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || (dev.role != "CASHIER" && dev.role != "KITCHEN")) {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Authorized workstation required."}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            int startNormal = body.value("startingNormal", 101);
            int startPriority = body.value("startingPriority", 101);

            orderManager_->resetTokenSequence(startNormal, startPriority);
            res.set_content(json{
                {"success", true},
                {"message", "Token sequence reset successfully."},
                {"nextNormal", orderManager_->previewNextToken(OrderType::NORMAL)},
                {"nextPriority", orderManager_->previewNextToken(OrderType::PRIORITY)}
            }.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // 14. Reset Revenue Baseline to ₹0 (Cashier only - NO orders deleted!)
    svr_.Post("/api/settings/revenue/reset", [this, db](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (!authenticateRequest(req, dev) || dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required."}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string reason = body.value("reason", "Manual Cashier Baseline Reset");

            if (db->resetRevenue(reason)) {
                auto stats = db->getDashboardStats();
                res.set_content(json{
                    {"success", true},
                    {"message", "Active revenue baseline reset to 0. Historical orders preserved."},
                    {"totalRevenue", stats.totalRevenue},
                    {"allTimeRevenue", stats.allTimeRevenue},
                    {"revenueResetAt", stats.revenueResetAt}
                }.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to reset revenue baseline."}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // =========================================================================
    // CORE ORDER MANAGEMENT & DASHBOARD ENDPOINTS
    // =========================================================================

    // 15. Dashboard API (Cashier only)
    svr_.Get("/api/dashboard", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (authenticateRequest(req, dev) && dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier role required to access sales metrics."}}.dump(), "application/json");
            return;
        }

        json data = orderManager_->getDashboardJson();
        res.set_content(data.dump(2), "application/json");
    });

    // 16. Menu API (Cashier and Kitchen)
    svr_.Get("/api/menu", [this](const httplib::Request&, httplib::Response& res) {
        auto items = orderManager_->getMenu();
        json arr = json::array();
        for (const auto& item : items) {
            arr.push_back(item.toJson());
        }
        res.set_content(json{{"menu", arr}}.dump(2), "application/json");
    });

    // 16b. Add Menu Item (Cashier)
    svr_.Post("/api/menu", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (authenticateRequest(req, dev) && dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Only Cashier can add new menu items."}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body.empty() ? "{}" : req.body);
            std::string name = body.value("name", "");
            std::string category = body.value("category", "Beverages");
            double price = body.value("price", 0.0);

            if (name.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "Item name is required."}}.dump(), "application/json");
                return;
            }
            if (price < 0.0) {
                res.status = 400;
                res.set_content(json{{"error", "Item price cannot be negative."}}.dump(), "application/json");
                return;
            }

            MenuItem newItem;
            if (orderManager_->addMenuItem(name, category, price, newItem)) {
                res.status = 201;
                res.set_content(json{
                    {"success", true},
                    {"message", "Menu item added successfully."},
                    {"item", newItem.toJson()}
                }.dump(2), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to add menu item."}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{{"error", std::string("Invalid request body: ") + e.what()}}.dump(), "application/json");
        }
    });

    // 17. Create Order (Cashier only)
    svr_.Post("/api/orders", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (authenticateRequest(req, dev) && dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Only Cashier devices can create orders."}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body);
            std::string customerName = body.value("customerName", "");
            std::string orderTypeStr = body.value("orderType", "NORMAL");
            OrderType type = stringToOrderType(orderTypeStr);

            std::vector<OrderItem> items;
            if (body.contains("items") && body["items"].is_array()) {
                for (const auto& itemJson : body["items"]) {
                    items.push_back(OrderItem::fromJson(itemJson));
                }
            }

            Order newOrder;
            std::string errorMsg;
            if (orderManager_->createOrder(customerName, type, items, newOrder, errorMsg)) {
                res.status = 201;
                res.set_content(json{
                    {"success", true},
                    {"message", "Order placed successfully."},
                    {"order", newOrder.toJson()}
                }.dump(2), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{
                    {"success", false},
                    {"error", errorMsg}
                }.dump(2), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", std::string("Invalid request body: ") + e.what()}
            }.dump(2), "application/json");
        }
    });

    // 18. Get Active Orders (Cashier, Kitchen, TV Display)
    svr_.Get("/api/orders/active", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        authenticateRequest(req, dev); // Updates heartbeat if token provided

        json activeData = orderManager_->getActiveOrdersJson();
        res.set_content(activeData.dump(2), "application/json");
    });

    // 19. Get Next Order to prepare (Priority Queue first, then Normal Queue)
    svr_.Get("/api/orders/next", [this](const httplib::Request&, httplib::Response& res) {
        Order nextOrder;
        if (orderManager_->getNextWaitingOrder(nextOrder)) {
            res.set_content(json{
                {"success", true},
                {"hasNext", true},
                {"order", nextOrder.toJson()}
            }.dump(2), "application/json");
        } else {
            res.set_content(json{
                {"success", true},
                {"hasNext", false},
                {"message", "No orders currently waiting in queue."}
            }.dump(2), "application/json");
        }
    });

    // 20. Order History (Cashier only)
    svr_.Get("/api/orders/history", [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (authenticateRequest(req, dev) && dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required to view historical records."}}.dump(), "application/json");
            return;
        }

        int limit = 50;
        if (req.has_param("limit")) {
            try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
        }
        std::string search = req.has_param("search") ? req.get_param_value("search") : "";
        auto history = orderManager_->getHistory(limit, search);
        json arr = json::array();
        for (const auto& ord : history) {
            arr.push_back(ord.toJson());
        }
        res.set_content(json{{"orders", arr}}.dump(2), "application/json");
    });

    // 20b. Delete single order (Cashier authority)
    auto handleDeleteOrder = [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (authenticateRequest(req, dev) && dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required to delete orders."}}.dump(), "application/json");
            return;
        }

        int id = 0;
        try {
            id = std::stoi(req.matches[1]);
        } catch (...) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid order ID."}}.dump(), "application/json");
            return;
        }

        std::string tokenHint = req.has_param("token") ? req.get_param_value("token") : "";
        if (tokenHint.empty() && !req.body.empty()) {
            try {
                auto b = json::parse(req.body);
                tokenHint = b.value("token", "");
            } catch (...) {}
        }

        if (orderManager_->deleteOrder(id, tokenHint)) {
            res.set_content(json{
                {"success", true},
                {"message", "Order #" + std::to_string(id) + " deleted successfully."}
            }.dump(), "application/json");
        } else {
            res.status = 500;
            res.set_content(json{{"error", "Failed to delete order."}}.dump(), "application/json");
        }
    };

    svr_.Delete(R"(/api/orders/([0-9]+))", handleDeleteOrder);
    svr_.Post(R"(/api/orders/([0-9]+)/delete)", handleDeleteOrder);

    // 20c. Clear All Orders (Cashier authority)
    auto handleClearAllOrders = [this](const httplib::Request& req, httplib::Response& res) {
        DeviceRecord dev;
        if (authenticateRequest(req, dev) && dev.role != "CASHIER") {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: Cashier authority required to clear orders."}}.dump(), "application/json");
            return;
        }

        if (orderManager_->deleteAllOrders()) {
            res.set_content(json{
                {"success", true},
                {"message", "All orders cleared successfully."}
            }.dump(), "application/json");
        } else {
            res.status = 500;
            res.set_content(json{{"error", "Failed to clear orders."}}.dump(), "application/json");
        }
    };

    svr_.Post("/api/orders/clear-all", handleClearAllOrders);
    svr_.Delete("/api/orders", handleClearAllOrders);


    // 21. Search Order by Token (O(1) Hash Table lookup)
    svr_.Get(R"(/api/orders/([A-Za-z0-9\-]+))", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.matches[1];
        Order order;
        if (orderManager_->findOrderByToken(token, order)) {
            res.set_content(json{
                {"success", true},
                {"order", order.toJson()}
            }.dump(2), "application/json");
        } else {
            res.status = 404;
            res.set_content(json{
                {"success", false},
                {"error", "Order with token '" + token + "' not found."}
            }.dump(2), "application/json");
        }
    });

    // 22. Update Status (Generic)
    svr_.Post(R"(/api/orders/([A-Za-z0-9\-]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.matches[1];
        try {
            json body = json::parse(req.body);
            std::string newStatus = body.value("status", "");
            Order updated;
            std::string errorMsg;
            if (orderManager_->updateOrderStatus(token, newStatus, updated, errorMsg)) {
                res.set_content(json{
                    {"success", true},
                    {"message", "Order status updated to " + newStatus},
                    {"order", updated.toJson()}
                }.dump(2), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{
                    {"success", false},
                    {"error", errorMsg}
                }.dump(2), "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", std::string("Invalid JSON: ") + e.what()}
            }.dump(2), "application/json");
        }
    });

    // 23. Specialized status action endpoints
    svr_.Post(R"(/api/orders/([A-Za-z0-9\-]+)/start)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.matches[1];
        Order updated;
        std::string errorMsg;
        if (orderManager_->startPreparing(token, updated, errorMsg)) {
            res.set_content(json{
                {"success", true},
                {"message", "Order moved to PREPARING"},
                {"order", updated.toJson()}
            }.dump(2), "application/json");
        } else {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", errorMsg}
            }.dump(2), "application/json");
        }
    });

    svr_.Post(R"(/api/orders/([A-Za-z0-9\-]+)/ready)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.matches[1];
        Order updated;
        std::string errorMsg;
        if (orderManager_->markReady(token, updated, errorMsg)) {
            res.set_content(json{
                {"success", true},
                {"message", "Order marked READY"},
                {"order", updated.toJson()}
            }.dump(2), "application/json");
        } else {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", errorMsg}
            }.dump(2), "application/json");
        }
    });

    svr_.Post(R"(/api/orders/([A-Za-z0-9\-]+)/complete)", [this](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.matches[1];
        Order updated;
        std::string errorMsg;
        if (orderManager_->completeOrder(token, updated, errorMsg)) {
            res.set_content(json{
                {"success", true},
                {"message", "Order COMPLETED and archived to history"},
                {"order", updated.toJson()}
            }.dump(2), "application/json");
        } else {
            res.status = 400;
            res.set_content(json{
                {"success", false},
                {"error", errorMsg}
            }.dump(2), "application/json");
        }
    });

    // 24. Mount static directory for frontend
    svr_.set_mount_point("/", webRoot_);

    // Default redirect to index.html
    svr_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/index.html");
    });
}

bool HttpServer::start() {
    setupRoutes();
    std::cout << "========================================================\n";
    std::cout << "   CAFE TOKEN MANAGEMENT SYSTEM - C++ SERVER STARTED    \n";
    std::cout << "========================================================\n";
    std::cout << "Local access:    http://localhost:" << port_ << "\n";
    std::cout << "LAN access:      http://" << host_ << ":" << port_ << " (or Laptop's LAN IP)\n";
    std::cout << "Serving static:  " << webRoot_ << "\n";
    std::cout << "Data Structures: Queue (FIFO), Priority Queue (Heap),\n";
    std::cout << "                 Hash Table (O(1)), Linked List\n";
    std::cout << "Database:        SQLite (cafeteria.db)\n";
    std::cout << "Multi-Device:    Cashier, Kitchen Display, TV Display\n";
    std::cout << "Security:        Role-Based Access Control + 6-Digit Pairing\n";
    std::cout << "Press Ctrl+C to shut down.\n";
    std::cout << "========================================================\n" << std::endl;

    return svr_.listen(host_, port_);
}

void HttpServer::stop() {
    svr_.stop();
}
