#include "OrderManager.h"
#include <iostream>
#include <algorithm>

OrderManager::OrderManager(std::shared_ptr<Database> db) : db_(db) {}

bool OrderManager::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Recover sequence counter
    uint64_t maxSeq = db_->getMaxSequenceNumber();
    sequenceCounter_ = maxSeq;

    // 2. Recover token numbers
    std::string savedNorm = db_->getSetting("token_next_normal", "");
    if (!savedNorm.empty()) {
        try { nextNormalTokenNum_ = std::stoi(savedNorm); } catch(...) {}
    } else {
        nextNormalTokenNum_ = db_->getLastTokenNumber(OrderType::NORMAL) + 1;
    }

    std::string savedPrio = db_->getSetting("token_next_priority", "");
    if (!savedPrio.empty()) {
        try { nextPriorityTokenNum_ = std::stoi(savedPrio); } catch(...) {}
    } else {
        nextPriorityTokenNum_ = db_->getLastTokenNumber(OrderType::PRIORITY) + 1;
    }

    // 3. Reconstruct in-memory data structures from persistent SQLite storage
    std::vector<Order> activeOrders = db_->getActiveOrders();
    std::cout << "[OrderManager] Restoring " << activeOrders.size() << " active orders into C++ data structures...\n";

    for (const auto& ord : activeOrders) {
        // Index every active order into the custom Hash Table
        activeOrdersHash_.insert(ord.token, ord);

        // If order is WAITING, re-enqueue into custom Priority Queue or Queue
        if (ord.status == OrderStatus::WAITING) {
            if (ord.type == OrderType::PRIORITY) {
                priorityQueue_.insert(ord);
            } else {
                normalQueue_.enqueue(ord);
            }
        }
    }

    // 4. Preload recent completed orders into custom Linked List
    std::vector<Order> completedOrders = db_->getOrderHistory(20);
    for (const auto& ord : completedOrders) {
        if (ord.status == OrderStatus::COMPLETED) {
            completedOrdersList_.append(ord);
        }
    }

    std::cout << "[OrderManager] Data structures restored: "
              << "Priority Queue: " << priorityQueue_.size() << ", "
              << "Normal Queue: " << normalQueue_.size() << ", "
              << "Hash Table active: " << activeOrdersHash_.size() << ", "
              << "Linked List history: " << completedOrdersList_.size() << "\n";

    return true;
}

static std::string formatTokenNumber(const std::string& prefix, int num, const std::string& format) {
    if (format == "three_digit") {
        std::ostringstream oss;
        oss << prefix << std::setw(3) << std::setfill('0') << num;
        return oss.str();
    }
    return prefix + std::to_string(num);
}

std::string OrderManager::generateToken(OrderType type) {
    std::string prefix = (type == OrderType::PRIORITY)
        ? db_->getSetting("token_prefix_priority", "P-")
        : db_->getSetting("token_prefix_normal", "N-");
    std::string fmt = db_->getSetting("token_format", "from_101");

    std::string token;
    while (true) {
        int num = (type == OrderType::PRIORITY) ? nextPriorityTokenNum_++ : nextNormalTokenNum_++;
        token = formatTokenNumber(prefix, num, fmt);
        if (!activeOrdersHash_.contains(token)) {
            break;
        }
    }

    if (type == OrderType::PRIORITY) {
        db_->setSetting("token_next_priority", std::to_string(nextPriorityTokenNum_.load()));
    } else {
        db_->setSetting("token_next_normal", std::to_string(nextNormalTokenNum_.load()));
    }

    return token;
}

std::string OrderManager::previewNextToken(OrderType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string prefix = (type == OrderType::PRIORITY)
        ? db_->getSetting("token_prefix_priority", "P-")
        : db_->getSetting("token_prefix_normal", "N-");
    std::string fmt = db_->getSetting("token_format", "from_101");
    int num = (type == OrderType::PRIORITY) ? nextPriorityTokenNum_.load() : nextNormalTokenNum_.load();
    return formatTokenNumber(prefix, num, fmt);
}

void OrderManager::resetTokenSequence(int startingNormal, int startingPriority) {
    std::lock_guard<std::mutex> lock(mutex_);
    nextNormalTokenNum_ = startingNormal;
    nextPriorityTokenNum_ = startingPriority;
    db_->setSetting("token_next_normal", std::to_string(startingNormal));
    db_->setSetting("token_next_priority", std::to_string(startingPriority));
}

bool OrderManager::createOrder(const std::string& customerName, OrderType type, const std::vector<OrderItem>& items, Order& outOrder, std::string& errorMsg) {
    if (items.empty()) {
        errorMsg = "Order must contain at least one item.";
        return false;
    }

    if (type == OrderType::PRIORITY && db_->getSetting("enable_priority_orders", "true") == "false") {
        errorMsg = "Priority orders are currently disabled in settings.";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    double total = 0.0;
    for (const auto& it : items) {
        if (it.quantity <= 0) {
            errorMsg = "Item quantity must be greater than 0.";
            return false;
        }
        total += it.subtotal;
    }

    std::string now = getCurrentTimestamp();
    std::string defStatus = db_->getSetting("default_order_status", "WAITING");
    OrderStatus initialStatus = (defStatus == "PREPARING") ? OrderStatus::PREPARING : OrderStatus::WAITING;

    Order order;
    order.token = generateToken(type);
    order.customerName = customerName.empty() ? "Guest" : customerName;
    order.type = type;
    order.status = initialStatus;
    order.items = items;
    order.totalAmount = total;
    order.sequenceNumber = ++sequenceCounter_;
    order.createdAt = now;
    order.updatedAt = now;

    // 1. Persist to SQLite
    if (!db_->saveOrder(order)) {
        errorMsg = "Failed to save order to database.";
        return false;
    }

    // 2. Insert into custom Hash Table for O(1) lookup
    activeOrdersHash_.insert(order.token, order);

    // 3. Enqueue into appropriate custom Queue structure if status is WAITING
    if (order.status == OrderStatus::WAITING) {
        if (order.type == OrderType::PRIORITY) {
            priorityQueue_.insert(order);
        } else {
            normalQueue_.enqueue(order);
        }
    }

    outOrder = order;
    return true;
}

bool OrderManager::getNextWaitingOrder(Order& outOrder) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Rule: Priority orders are processed before normal orders.
    // If priority queue is not empty, take the highest priority order.
    if (!priorityQueue_.isEmpty()) {
        outOrder = priorityQueue_.peek();
        return true;
    }

    // If no priority orders, take from normal FIFO queue.
    if (!normalQueue_.isEmpty()) {
        outOrder = normalQueue_.front();
        return true;
    }

    return false;
}

bool OrderManager::startPreparing(const std::string& token, Order& outOrder, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(mutex_);

    Order* existing = activeOrdersHash_.search(token);
    if (!existing) {
        errorMsg = "Order not found in active orders.";
        return false;
    }

    if (existing->status != OrderStatus::WAITING) {
        errorMsg = "Only WAITING orders can be started. Current status: " + orderStatusToString(existing->status);
        return false;
    }

    // Remove from waiting queue (Priority or Normal)
    if (existing->type == OrderType::PRIORITY) {
        priorityQueue_.remove(*existing);
    } else {
        normalQueue_.remove(*existing);
    }

    // Transition status to PREPARING
    std::string now = getCurrentTimestamp();
    existing->status = OrderStatus::PREPARING;
    existing->updatedAt = now;

    // Update in Database
    db_->updateOrderStatus(token, OrderStatus::PREPARING, now);

    outOrder = *existing;
    return true;
}

bool OrderManager::markReady(const std::string& token, Order& outOrder, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(mutex_);

    Order* existing = activeOrdersHash_.search(token);
    if (!existing) {
        errorMsg = "Order not found in active orders.";
        return false;
    }

    if (existing->status != OrderStatus::PREPARING) {
        errorMsg = "Only PREPARING orders can be marked READY. Current status: " + orderStatusToString(existing->status);
        return false;
    }

    std::string now = getCurrentTimestamp();
    existing->status = OrderStatus::READY;
    existing->updatedAt = now;

    db_->updateOrderStatus(token, OrderStatus::READY, now);

    outOrder = *existing;
    return true;
}

bool OrderManager::completeOrder(const std::string& token, Order& outOrder, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(mutex_);

    Order* existing = activeOrdersHash_.search(token);
    if (!existing) {
        errorMsg = "Order not found in active orders.";
        return false;
    }

    if (existing->status != OrderStatus::READY && existing->status != OrderStatus::PREPARING) {
        errorMsg = "Order cannot be completed from current status: " + orderStatusToString(existing->status);
        return false;
    }

    std::string now = getCurrentTimestamp();
    existing->status = OrderStatus::COMPLETED;
    existing->updatedAt = now;

    // Update in database
    db_->updateOrderStatus(token, OrderStatus::COMPLETED, now);

    // Save copy to linked list of completed history (newest at head)
    outOrder = *existing;
    completedOrdersList_.prepend(outOrder);

    // Remove from active hash table
    activeOrdersHash_.remove(token);

    return true;
}

bool OrderManager::updateOrderStatus(const std::string& token, const std::string& newStatusStr, Order& outOrder, std::string& errorMsg) {
    OrderStatus target = stringToOrderStatus(newStatusStr);
    switch (target) {
        case OrderStatus::PREPARING:
            return startPreparing(token, outOrder, errorMsg);
        case OrderStatus::READY:
            return markReady(token, outOrder, errorMsg);
        case OrderStatus::COMPLETED:
            return completeOrder(token, outOrder, errorMsg);
        default:
            errorMsg = "Invalid target status: " + newStatusStr;
            return false;
    }
}

bool OrderManager::findOrderByToken(const std::string& token, Order& outOrder) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Fast O(1) in-memory lookup via custom Hash Table
    Order* active = activeOrdersHash_.search(token);
    if (active != nullptr) {
        outOrder = *active;
        return true;
    }

    // 2. If not found in active hash table, query SQLite (completed or historic order)
    return db_->getOrder(token, outOrder);
}

json OrderManager::getActiveOrdersJson() {
    std::lock_guard<std::mutex> lock(mutex_);

    json waitingList = json::array();
    // 1. Waiting Priority Orders (sorted by heap priority)
    std::vector<Order> priorityOrders = priorityQueue_.toSortedVector();
    for (const auto& ord : priorityOrders) {
        waitingList.push_back(ord.toJson());
    }

    // 2. Waiting Normal Orders (FIFO order)
    std::vector<Order> normalOrders = normalQueue_.toVector();
    for (const auto& ord : normalOrders) {
        waitingList.push_back(ord.toJson());
    }

    // 3. Preparing & Ready from active hash table
    json preparingList = json::array();
    json readyList = json::array();

    std::vector<Order> allActive = activeOrdersHash_.getAllValues();
    // Sort allActive by sequenceNumber for consistent display
    std::sort(allActive.begin(), allActive.end(), [](const Order& a, const Order& b) {
        return a.sequenceNumber < b.sequenceNumber;
    });

    for (const auto& ord : allActive) {
        if (ord.status == OrderStatus::PREPARING) {
            preparingList.push_back(ord.toJson());
        } else if (ord.status == OrderStatus::READY) {
            readyList.push_back(ord.toJson());
        }
    }

    json completedList = json::array();
    std::vector<Order> recentCompleted = completedOrdersList_.toVector();
    for (const auto& ord : recentCompleted) {
        completedList.push_back(json{
            {"id", ord.id},
            {"token", ord.token},
            {"type", orderTypeToString(ord.type)},
            {"status", "COMPLETED"},
            {"updatedAt", ord.updatedAt}
        });
    }

    return json{
        {"waiting", waitingList},
        {"preparing", preparingList},
        {"ready", readyList},
        {"completed", completedList},
        {"counts", {
            {"priorityWaiting", priorityQueue_.size()},
            {"normalWaiting", normalQueue_.size()},
            {"totalWaiting", priorityQueue_.size() + normalQueue_.size()},
            {"preparing", preparingList.size()},
            {"ready", readyList.size()},
            {"completed", completedList.size()},
            {"totalActive", activeOrdersHash_.size()}
        }}
    };
}

json OrderManager::getDashboardJson() {
    // Collect SQLite aggregated metrics
    DashboardStats stats = db_->getDashboardStats();

    std::lock_guard<std::mutex> lock(mutex_);

    // Recent orders from database
    std::vector<Order> recent = db_->getOrderHistory(8);
    json recentJson = json::array();
    for (const auto& ord : recent) {
        recentJson.push_back(ord.toJson());
    }

    // Queue snapshot
    json queueOverview = json::array();
    std::vector<Order> priorityOrders = priorityQueue_.toSortedVector();
    for (size_t i = 0; i < priorityOrders.size() && queueOverview.size() < 10; ++i) {
        queueOverview.push_back(priorityOrders[i].toJson());
    }
    std::vector<Order> normalOrders = normalQueue_.toVector();
    for (size_t i = 0; i < normalOrders.size() && queueOverview.size() < 10; ++i) {
        queueOverview.push_back(normalOrders[i].toJson());
    }

    return json{
        {"stats", stats.toJson()},
        {"queueCounts", {
            {"priorityQueueSize", priorityQueue_.size()},
            {"normalQueueSize", normalQueue_.size()},
            {"activeHashTableSize", activeOrdersHash_.size()}
        }},
        {"recentOrders", recentJson},
        {"queueOverview", queueOverview}
    };
}

std::vector<MenuItem> OrderManager::getMenu() {
    return db_->getMenuItems();
}

bool OrderManager::addMenuItem(const std::string& name, const std::string& category, double price, MenuItem& outItem) {
    return db_->addMenuItem(name, category, price, outItem);
}

std::vector<Order> OrderManager::getHistory(int limit, const std::string& search) {
    return db_->getOrderHistory(limit, search);
}

bool OrderManager::deleteOrder(int orderId, const std::string& tokenHint) {
    std::string tok = tokenHint;
    if (tok.empty()) {
        auto history = db_->getOrderHistory(100, "");
        for (const auto& o : history) {
            if (o.id == orderId) {
                tok = o.token;
                break;
            }
        }
    }

    if (!tok.empty()) {
        activeOrdersHash_.remove(tok);
    }

    Order dummy;
    dummy.id = orderId;
    if (!tok.empty()) dummy.token = tok;
    completedOrdersList_.remove(dummy);

    return db_->deleteOrderById(orderId);
}

bool OrderManager::deleteAllOrders() {
    bool ok = db_->deleteAllOrders();
    activeOrdersHash_.clear();
    completedOrdersList_.clear();
    normalQueue_.clear();
    priorityQueue_.clear();
    return ok;
}

