#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <iostream>
#include <stdexcept>
#include <vector>
#include <functional>

/**
 * @brief Custom Priority Queue using a Binary Heap.
 * 
 * Supports custom comparator Compare: Compare(a, b) returns true if 'a' has higher priority than 'b'.
 * For equal priority items, the comparator should break ties using sequenceNumber (FIFO).
 */
template <typename T, typename Compare = std::less<T>>
class PriorityQueue {
private:
    T* data_;
    size_t capacity_;
    size_t size_;
    Compare comp_;

    void resize(size_t newCap) {
        T* newData = new T[newCap];
        for (size_t i = 0; i < size_; ++i) {
            newData[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_ = newData;
        capacity_ = newCap;
    }

    size_t parent(size_t i) const { return (i - 1) / 2; }
    size_t leftChild(size_t i) const { return 2 * i + 1; }
    size_t rightChild(size_t i) const { return 2 * i + 2; }

    void swapElements(size_t i, size_t j) {
        T temp = std::move(data_[i]);
        data_[i] = std::move(data_[j]);
        data_[j] = std::move(temp);
    }

    void siftUp(size_t i) {
        while (i > 0 && comp_(data_[i], data_[parent(i)])) {
            swapElements(i, parent(i));
            i = parent(i);
        }
    }

    void siftDown(size_t i) {
        size_t best = i;
        size_t left = leftChild(i);
        size_t right = rightChild(i);

        if (left < size_ && comp_(data_[left], data_[best])) {
            best = left;
        }
        if (right < size_ && comp_(data_[right], data_[best])) {
            best = right;
        }

        if (best != i) {
            swapElements(i, best);
            siftDown(best);
        }
    }

public:
    explicit PriorityQueue(size_t initialCap = 16, Compare comp = Compare())
        : capacity_(initialCap < 4 ? 4 : initialCap), size_(0), comp_(comp) {
        data_ = new T[capacity_];
    }

    ~PriorityQueue() {
        delete[] data_;
    }

    // Disable copy
    PriorityQueue(const PriorityQueue&) = delete;
    PriorityQueue& operator=(const PriorityQueue&) = delete;

    // Move constructor
    PriorityQueue(PriorityQueue&& other) noexcept
        : data_(other.data_), capacity_(other.capacity_), size_(other.size_), comp_(std::move(other.comp_)) {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    PriorityQueue& operator=(PriorityQueue&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            capacity_ = other.capacity_;
            size_ = other.size_;
            comp_ = std::move(other.comp_);
            other.data_ = nullptr;
            other.capacity_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Inserts an element into the binary heap and restores the heap property.
     * Time Complexity: O(log N)
     */
    void insert(const T& val) {
        if (size_ == capacity_) {
            resize(capacity_ * 2);
        }
        data_[size_] = val;
        siftUp(size_);
        size_++;
    }

    /**
     * @brief Extracts and returns the highest priority element from the heap.
     * Time Complexity: O(log N)
     */
    T extractTop() {
        if (isEmpty()) {
            throw std::runtime_error("PriorityQueue Underflow: Queue is empty");
        }
        T topItem = std::move(data_[0]);
        size_--;
        if (size_ > 0) {
            data_[0] = std::move(data_[size_]);
            siftDown(0);
        }
        return topItem;
    }

    /**
     * @brief Peeks at the highest priority element without removing it.
     * Time Complexity: O(1)
     */
    const T& peek() const {
        if (isEmpty()) {
            throw std::runtime_error("PriorityQueue is empty: no top element");
        }
        return data_[0];
    }

    T& peek() {
        if (isEmpty()) {
            throw std::runtime_error("PriorityQueue is empty: no top element");
        }
        return data_[0];
    }

    bool isEmpty() const {
        return size_ == 0;
    }

    size_t size() const {
        return size_;
    }

    /**
     * @brief Displays the heap elements in array order (for debugging / demonstration).
     */
    void display() const {
        std::cout << "[PriorityQueue (Heap)]: ";
        for (size_t i = 0; i < size_; ++i) {
            std::cout << "[" << i << "]: " << data_[i] << " | ";
        }
        std::cout << " (Total: " << size_ << ")\n";
    }

    /**
     * @brief Removes a specific element matching val from the heap, then reheapifies.
     */
    bool remove(const T& val) {
        size_t idx = size_;
        for (size_t i = 0; i < size_; ++i) {
            if (data_[i] == val) {
                idx = i;
                break;
            }
        }
        if (idx == size_) return false;

        size_--;
        if (idx < size_) {
            data_[idx] = std::move(data_[size_]);
            siftDown(idx);
            siftUp(idx);
        }
        return true;
    }

    /**
     * @brief Returns copy of elements in sorted order of priority without modifying the queue.
     */
    std::vector<T> toSortedVector() const {
        // Clone internal array into a temporary heap and extract top one by one
        PriorityQueue<T, Compare> temp(capacity_, comp_);
        for (size_t i = 0; i < size_; ++i) {
            temp.insert(data_[i]);
        }
        std::vector<T> result;
        result.reserve(size_);
        while (!temp.isEmpty()) {
            result.push_back(temp.extractTop());
        }
        return result;
    }

    void clear() {
        size_ = 0;
    }
};

#endif // PRIORITY_QUEUE_H
