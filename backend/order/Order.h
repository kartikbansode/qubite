#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../third_party/json.hpp"

using json = nlohmann::json;

enum class OrderType {
    NORMAL,
    PRIORITY
};

inline std::string orderTypeToString(OrderType type) {
    return (type == OrderType::PRIORITY) ? "PRIORITY" : "NORMAL";
}

inline OrderType stringToOrderType(const std::string& str) {
    if (str == "PRIORITY" || str == "priority") {
        return OrderType::PRIORITY;
    }
    return OrderType::NORMAL;
}

enum class OrderStatus {
    WAITING,
    PREPARING,
    READY,
    COMPLETED
};

inline std::string orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::WAITING: return "WAITING";
        case OrderStatus::PREPARING: return "PREPARING";
        case OrderStatus::READY: return "READY";
        case OrderStatus::COMPLETED: return "COMPLETED";
        default: return "WAITING";
    }
}

inline OrderStatus stringToOrderStatus(const std::string& str) {
    if (str == "PREPARING" || str == "preparing") return OrderStatus::PREPARING;
    if (str == "READY" || str == "ready") return OrderStatus::READY;
    if (str == "COMPLETED" || str == "completed") return OrderStatus::COMPLETED;
    return OrderStatus::WAITING;
}

struct OrderItem {
    int itemId;
    std::string itemName;
    int quantity;
    double unitPrice;
    double subtotal;

    json toJson() const {
        return json{
            {"itemId", itemId},
            {"itemName", itemName},
            {"quantity", quantity},
            {"unitPrice", unitPrice},
            {"subtotal", subtotal}
        };
    }

    static OrderItem fromJson(const json& j) {
        OrderItem item;
        item.itemId = j.value("itemId", 0);
        item.itemName = j.value("itemName", "");
        item.quantity = j.value("quantity", 1);
        item.unitPrice = j.value("unitPrice", 0.0);
        item.subtotal = item.quantity * item.unitPrice;
        return item;
    }
};

struct Order {
    int id{0};
    std::string token;
    std::string customerName;
    OrderType type{OrderType::NORMAL};
    OrderStatus status{OrderStatus::WAITING};
    std::vector<OrderItem> items;
    double totalAmount{0.0};
    uint64_t sequenceNumber{0}; // For FIFO tie-breaking in priority queue
    std::string createdAt;
    std::string updatedAt;

    bool operator==(const Order& other) const {
        return (!token.empty() && token == other.token) || (id != 0 && id == other.id);
    }

    json toJson() const {
        json itemsJson = json::array();
        for (const auto& it : items) {
            itemsJson.push_back(it.toJson());
        }

        return json{
            {"id", id},
            {"token", token},
            {"customerName", customerName},
            {"orderType", orderTypeToString(type)},
            {"status", orderStatusToString(status)},
            {"items", itemsJson},
            {"totalAmount", totalAmount},
            {"sequenceNumber", sequenceNumber},
            {"createdAt", createdAt},
            {"updatedAt", updatedAt}
        };
    }
};

/**
 * @brief Comparator for Priority Queue (Binary Heap).
 * Orders with higher priority come first.
 * For orders with the same priority, smaller sequenceNumber comes first (FIFO).
 */
struct OrderPriorityComparator {
    bool operator()(const Order& a, const Order& b) const {
        bool aIsPriority = (a.type == OrderType::PRIORITY);
        bool bIsPriority = (b.type == OrderType::PRIORITY);

        if (aIsPriority != bIsPriority) {
            return aIsPriority > bIsPriority; // true if 'a' is priority and 'b' is normal
        }
        // Same priority level: FIFO order based on creation sequence
        return a.sequenceNumber < b.sequenceNumber;
    }
};

inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmNow{};
#ifdef _WIN32
    localtime_s(&tmNow, &timeT);
#else
    localtime_r(&timeT, &tmNow);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

#endif // ORDER_H
