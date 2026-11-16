#ifndef ORDER_MANAGER_H
#define ORDER_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include "Order.h"
#include "../structures/Queue.h"
#include "../structures/PriorityQueue.h"
#include "../structures/HashTable.h"
#include "../structures/LinkedList.h"
#include "../database/Database.h"

class OrderManager {
private:
    std::shared_ptr<Database> db_;
    std::mutex mutex_;

    // Core Custom C++ Data Structures
    Queue<Order> normalQueue_;                                           // FIFO queue for Normal WAITING orders
    PriorityQueue<Order, OrderPriorityComparator> priorityQueue_;       // Binary heap for Priority WAITING orders
    HashTable<std::string, Order> activeOrdersHash_;                     // O(1) lookup table for active orders
    LinkedList<Order> completedOrdersList_;                              // Doubly linked list for completed orders

    std::atomic<uint64_t> sequenceCounter_{0};
    std::atomic<int> nextNormalTokenNum_{101};
    std::atomic<int> nextPriorityTokenNum_{101};

    std::string generateToken(OrderType type);

public:
    explicit OrderManager(std::shared_ptr<Database> db);

    bool init();

    // Core Order actions
    bool createOrder(const std::string& customerName, OrderType type, const std::vector<OrderItem>& items, Order& outOrder, std::string& errorMsg);
    bool getNextWaitingOrder(Order& outOrder);
    bool startPreparing(const std::string& token, Order& outOrder, std::string& errorMsg);
    bool markReady(const std::string& token, Order& outOrder, std::string& errorMsg);
    bool completeOrder(const std::string& token, Order& outOrder, std::string& errorMsg);
    bool updateOrderStatus(const std::string& token, const std::string& newStatusStr, Order& outOrder, std::string& errorMsg);

    // Lookups & Deletions
    bool findOrderByToken(const std::string& token, Order& outOrder);
    bool deleteOrder(int orderId, const std::string& token = "");
    bool deleteAllOrders();

    // Summaries & Stats
    json getActiveOrdersJson();
    json getDashboardJson();
    std::vector<MenuItem> getMenu();
    bool addMenuItem(const std::string& name, const std::string& category, double price, MenuItem& outItem);
    std::vector<Order> getHistory(int limit = 50, const std::string& search = "");

    // Token sequencing & configuration
    std::string previewNextToken(OrderType type);
    void resetTokenSequence(int startingNormal = 101, int startingPriority = 101);
    std::shared_ptr<Database> getDatabase() const { return db_; }
};

#endif // ORDER_MANAGER_H
