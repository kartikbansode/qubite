#include <iostream>
#include <cassert>
#include <string>
#include "../backend/structures/Queue.h"
#include "../backend/structures/PriorityQueue.h"
#include "../backend/structures/HashTable.h"
#include "../backend/structures/LinkedList.h"
#include "../backend/order/Order.h"

void testQueue() {
    std::cout << "[Test] Running Queue (FIFO) tests...\n";
    Queue<std::string> q;
    assert(q.isEmpty());
    assert(q.size() == 0);

    q.enqueue("Order-1");
    q.enqueue("Order-2");
    q.enqueue("Order-3");

    assert(!q.isEmpty());
    assert(q.size() == 3);
    assert(q.front() == "Order-1");

    q.dequeue();
    assert(q.front() == "Order-2");
    assert(q.size() == 2);

    bool removed = q.remove("Order-3");
    assert(removed);
    assert(q.size() == 1);
    assert(q.front() == "Order-2");

    q.dequeue();
    assert(q.isEmpty());
    std::cout << "  -> Queue tests PASSED.\n";
}

void testPriorityQueue() {
    std::cout << "[Test] Running PriorityQueue (Binary Heap) tests...\n";
    PriorityQueue<Order, OrderPriorityComparator> pq;
    assert(pq.isEmpty());

    Order o1; o1.token = "N-101"; o1.type = OrderType::NORMAL; o1.sequenceNumber = 1;
    Order o2; o2.token = "P-101"; o2.type = OrderType::PRIORITY; o2.sequenceNumber = 2;
    Order o3; o3.token = "P-102"; o3.type = OrderType::PRIORITY; o3.sequenceNumber = 3;
    Order o4; o4.token = "N-102"; o4.type = OrderType::NORMAL; o4.sequenceNumber = 4;

    // Insert normal first, then priority
    pq.insert(o1);
    pq.insert(o2);
    pq.insert(o3);
    pq.insert(o4);

    assert(pq.size() == 4);

    // First extracted must be P-101 (Priority with sequence 2)
    Order top1 = pq.extractTop();
    assert(top1.token == "P-101");

    // Second extracted must be P-102 (Priority with sequence 3)
    Order top2 = pq.extractTop();
    assert(top2.token == "P-102");

    // Third extracted must be N-101 (Normal with sequence 1)
    Order top3 = pq.extractTop();
    assert(top3.token == "N-101");

    // Fourth extracted must be N-102 (Normal with sequence 4)
    Order top4 = pq.extractTop();
    assert(top4.token == "N-102");

    assert(pq.isEmpty());
    std::cout << "  -> PriorityQueue tests PASSED.\n";
}

void testHashTable() {
    std::cout << "[Test] Running HashTable (Separate Chaining) tests...\n";
    HashTable<std::string, std::string> ht;
    assert(ht.isEmpty());

    ht.insert("N-101", "Burger + Fries");
    ht.insert("P-102", "Cappuccino");
    ht.insert("N-103", "Cold Coffee");

    assert(ht.size() == 3);
    assert(ht.contains("N-101"));
    assert(ht.contains("P-102"));
    assert(!ht.contains("X-999"));

    std::string* val = ht.search("P-102");
    assert(val != nullptr && *val == "Cappuccino");

    // Update value
    ht.insert("P-102", "Caramel Cappuccino");
    val = ht.search("P-102");
    assert(val != nullptr && *val == "Caramel Cappuccino");
    assert(ht.size() == 3);

    // Remove
    bool rem = ht.remove("N-101");
    assert(rem);
    assert(!ht.contains("N-101"));
    assert(ht.size() == 2);

    std::cout << "  -> HashTable tests PASSED.\n";
}

void testLinkedList() {
    std::cout << "[Test] Running LinkedList (Doubly Linked) tests...\n";
    LinkedList<std::string> list;
    assert(list.isEmpty());

    list.append("Order A");
    list.append("Order B");
    list.prepend("Order 0");

    assert(list.size() == 3);
    auto vec = list.toVector();
    assert(vec.size() == 3);
    assert(vec[0] == "Order 0");
    assert(vec[1] == "Order A");
    assert(vec[2] == "Order B");

    list.remove("Order A");
    assert(list.size() == 2);

    std::cout << "  -> LinkedList tests PASSED.\n";
}

int main() {
    std::cout << "=== Running Manual Data Structure Unit Tests ===\n";
    testQueue();
    testPriorityQueue();
    testHashTable();
    testLinkedList();
    std::cout << "=== ALL DATA STRUCTURE TESTS PASSED! ===\n";
    return 0;
}
