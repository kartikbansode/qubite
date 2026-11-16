#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <iostream>
#include <vector>
#include <functional>

/**
 * @brief Custom Doubly Linked List for maintaining order history and sequences.
 */
template <typename T>
class LinkedList {
private:
    struct Node {
        T data;
        Node* prev;
        Node* next;
        Node(const T& val) : data(val), prev(nullptr), next(nullptr) {}
    };

    Node* head_;
    Node* tail_;
    size_t size_;

public:
    LinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~LinkedList() {
        clear();
    }

    // Disable copy
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    // Move constructor
    LinkedList(LinkedList&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = nullptr;
        other.tail_ = nullptr;
        other.size_ = 0;
    }

    LinkedList& operator=(LinkedList&& other) noexcept {
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
     * @brief Appends item to tail of list (O(1)).
     */
    void append(const T& val) {
        Node* newNode = new Node(val);
        if (tail_ == nullptr) {
            head_ = tail_ = newNode;
        } else {
            tail_->next = newNode;
            newNode->prev = tail_;
            tail_ = newNode;
        }
        size_++;
    }

    /**
     * @brief Prepends item to head of list (O(1)).
     */
    void prepend(const T& val) {
        Node* newNode = new Node(val);
        if (head_ == nullptr) {
            head_ = tail_ = newNode;
        } else {
            newNode->next = head_;
            head_->prev = newNode;
            head_ = newNode;
        }
        size_++;
    }

    /**
     * @brief Removes the first occurrence of an item.
     */
    bool remove(const T& val) {
        Node* curr = head_;
        while (curr != nullptr) {
            if (curr->data == val) {
                if (curr->prev != nullptr) {
                    curr->prev->next = curr->next;
                } else {
                    head_ = curr->next;
                }
                if (curr->next != nullptr) {
                    curr->next->prev = curr->prev;
                } else {
                    tail_ = curr->prev;
                }
                delete curr;
                size_--;
                return true;
            }
            curr = curr->next;
        }
        return false;
    }

    size_t size() const {
        return size_;
    }

    bool isEmpty() const {
        return head_ == nullptr;
    }

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

    void display() const {
        std::cout << "[LinkedList]: ";
        Node* curr = head_;
        while (curr != nullptr) {
            std::cout << curr->data << " <-> ";
            curr = curr->next;
        }
        std::cout << "END (Size: " << size_ << ")\n";
    }

    void clear() {
        Node* curr = head_;
        while (curr != nullptr) {
            Node* nextNode = curr->next;
            delete curr;
            curr = nextNode;
        }
        head_ = tail_ = nullptr;
        size_ = 0;
    }
};

#endif // LINKED_LIST_H
