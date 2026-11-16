#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cstdint>
#include "../third_party/sqlite3.h"
#include "../order/Order.h"

struct MenuItem {
    int id;
    std::string name;
    std::string category;
    double price;
    bool isAvailable;

    json toJson() const {
        return json{
            {"id", id},
            {"name", name},
            {"category", category},
            {"price", price},
            {"isAvailable", isAvailable}
        };
    }
};

struct DashboardStats {
    int totalOrders{0};
    int waitingOrders{0};
    int preparingOrders{0};
    int readyOrders{0};
    int completedOrders{0};
    double totalRevenue{0.0};
    double allTimeRevenue{0.0};
    std::string revenueResetAt{""};

    json toJson() const {
        return json{
            {"totalOrders", totalOrders},
            {"waitingOrders", waitingOrders},
            {"preparingOrders", preparingOrders},
            {"readyOrders", readyOrders},
            {"completedOrders", completedOrders},
            {"totalRevenue", totalRevenue},
            {"allTimeRevenue", allTimeRevenue},
            {"revenueResetAt", revenueResetAt}
        };
    }
};

struct DeviceRecord {
    int id{0};
    std::string token;
    std::string name;
    std::string role; // "CASHIER", "KITCHEN", "TV_DISPLAY"
    std::string ipAddress;
    std::string createdAt;
    std::string lastConnected;
    bool isActive{true};
    bool isOnline{false};

    json toJson() const {
        return json{
            {"id", id},
            {"token", token},
            {"name", name},
            {"role", role},
            {"ipAddress", ipAddress},
            {"createdAt", createdAt},
            {"lastConnected", lastConnected},
            {"isActive", isActive},
            {"isOnline", isOnline}
        };
    }
};

struct PairingCodeRecord {
    int id{0};
    std::string code;
    std::string role;
    int64_t expiresAt{0};
    bool isUsed{false};
    int attempts{0};
    std::string createdAt;
};

class Database {
private:
    sqlite3* db_{nullptr};
    std::string dbPath_;

    bool execute(const std::string& sql);
    void seedMenuItemsIfEmpty();
    void seedSettingsIfEmpty();

public:
    explicit Database(const std::string& dbPath);
    ~Database();

    bool init();

    // Menu operations
    std::vector<MenuItem> getMenuItems();
    bool addMenuItem(const std::string& name, const std::string& category, double price, MenuItem& outItem);

    // Order operations
    bool saveOrder(Order& order);
    bool updateOrderStatus(const std::string& token, OrderStatus status, const std::string& updatedAt);
    bool getOrder(const std::string& token, Order& outOrder);
    std::vector<Order> getActiveOrders();
    std::vector<Order> getOrderHistory(int limit = 50, const std::string& search = "");
    DashboardStats getDashboardStats();

    bool deleteOrderById(int id);
    bool deleteAllOrders();

    uint64_t getMaxSequenceNumber();
    int getLastTokenNumber(OrderType type);

    // Settings operations
    std::string getSetting(const std::string& key, const std::string& defaultVal = "");
    bool setSetting(const std::string& key, const std::string& val);
    std::map<std::string, std::string> getAllSettings();
    bool resetRevenue(const std::string& reason = "");
    std::string getLatestRevenueResetTime();

    // Device operations
    bool registerDevice(const std::string& token, const std::string& name, const std::string& role, const std::string& ipAddress, DeviceRecord& outDevice);
    bool getDeviceByToken(const std::string& token, DeviceRecord& outDevice);
    std::vector<DeviceRecord> getAllDevices();
    bool updateDeviceName(int id, const std::string& name);
    bool revokeDevice(int id);
    bool updateDeviceHeartbeat(const std::string& token, const std::string& ipAddress = "");

    // Pairing code operations
    bool createPairingCode(const std::string& code, const std::string& role, int64_t expiresAt);
    bool verifyAndUsePairingCode(const std::string& code, std::string& outRole, std::string& errorMsg);
    void cleanupExpiredPairingCodes();
};

#endif // DATABASE_H
