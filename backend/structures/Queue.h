#ifndef QUEUE_H
#define QUEUE_H

#include <iostream>
#include <stdexcept>
#include <vector>

/**
 * @brief Custom FIFO Queue implementation using a Singly Linked List.
 * Follows First-In, First-Out (FIFO) semantics for Normal Cafeteria Orders.
 */
template <typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& val) : data(val), next(nullptr) {}
    };

    Node* head_;
    Node* tail_;
    size_t size_;

public:
    Queue() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~Queue() {
        clear();
    }

    // Disable copy to prevent double-free issues
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    // Move constructor
    Queue(Queue&& other) noexcept : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }

    Queue& operator=(Queue&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = nullptr;
            other.tail_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Adds an element to the rear of the queue (FIFO).
     * Time Complexity: O(1)
     */
    void enqueue(const T& item) {
        Node* newNode = new Node(item);
        if (isEmpty()) {
            head_ = tail_ = newNode;
        } else {
            tail_->next = newNode;
            tail_ = newNode;
        }
        size_++;
    }

    /**
     * @brief Removes the element from the front of the queue.
     * Time Complexity: O(1)
     */
    void dequeue() {
        if (isEmpty()) {
            throw std::runtime_error("Queue Underflow: Queue is empty");
        }
        Node* temp = head_;
        head_ = head_->next;
        if (head_ == nullptr) {
            tail_ = nullptr;
        }
        delete temp;
        size_--;
    }

    /**
     * @brief Returns a reference to the front element.
     * Time Complexity: O(1)
     */
    T& front() {
        if (isEmpty()) {
            throw std::runtime_error("Queue is empty: no front element");
        }
        return head_->data;
    }

    const T& front() const {
        if (isEmpty()) {
            throw std::runtime_error("Queue is empty: no front element");
        }
        return head_->data;
    }

    /**
     * @brief Checks if the queue contains no elements.
     */
    bool isEmpty() const {
        return head_ == nullptr;
    }

    /**
     * @brief Returns current element count.
     */
    size_t size() const {
        return size_;
    }

    /**
     * @brief Displays queue elements to standard output (for debugging / demonstration).
     */
    void display() const {
        std::cout << "[Queue Front] -> ";
        Node* curr = head_;
        while (curr != nullptr) {
            std::cout << curr->data << " -> ";
            curr = curr->next;
        }
        std::cout << "[Rear] (Size: " << size_ << ")\n";
    }

    /**
     * @brief Exports items in FIFO order as std::vector for JSON/API responses.
     */
    std::vector<T> toVector() const {
        std::vector<T> result;
        result.reserve(size_);
        Node* curr = head_;
        while (curr != nullptr) {
            result.push_back(curr->data);
            curr = curr->next;
        }
        return result;
    }

    /**
     * @brief Removes a specific element matching value.
     */
    bool remove(const T& val) {
        if (isEmpty()) return false;

        if (head_->data == val) {
            dequeue();
            return true;
        }

        Node* prev = head_;
        Node* curr = head_->next;
        while (curr != nullptr) {
            if (curr->data == val) {
                prev->next = curr->next;
                if (curr == tail_) {
                    tail_ = prev;
                }
                delete curr;
                size_--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    void clear() {
        while (!isEmpty()) {
            dequeue();
        }
    }
};

#endif // QUEUE_H
