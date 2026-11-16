#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

/**
 * @brief Default hash functor for std::string using the djb2 algorithm.
 */
struct StringHash {
    size_t operator()(const std::string& key) const {
        unsigned long hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + static_cast<unsigned char>(c); // hash * 33 + c
        }
        return static_cast<size_t>(hash);
    }
};

/**
 * @brief Custom Hash Table implementation using Separate Chaining.
 * Used for O(1) average-time order lookups by unique Token string.
 */
template <typename Key, typename Value, typename Hash = StringHash>
class HashTable {
private:
    struct HashNode {
        Key key;
        Value value;
        HashNode* next;
        HashNode(const Key& k, const Value& v) : key(k), value(v), next(nullptr) {}
    };

    HashNode** buckets_;
    size_t bucketCount_;
    size_t size_;
    Hash hasher_;
    float maxLoadFactor_;

    size_t getBucketIndex(const Key& key) const {
        return hasher_(key) % bucketCount_;
    }

    void rehash(size_t newBucketCount) {
        HashNode** newBuckets = new HashNode*[newBucketCount]();
        for (size_t i = 0; i < bucketCount_; ++i) {
            HashNode* curr = buckets_[i];
            while (curr != nullptr) {
                HashNode* nextNode = curr->next;
                size_t newIdx = hasher_(curr->key) % newBucketCount;
                curr->next = newBuckets[newIdx];
                newBuckets[newIdx] = curr;
                curr = nextNode;
            }
        }
        delete[] buckets_;
        buckets_ = newBuckets;
        bucketCount_ = newBucketCount;
    }

public:
    explicit HashTable(size_t initialBuckets = 53, float maxLoadFactor = 0.75f)
        : bucketCount_(initialBuckets < 11 ? 11 : initialBuckets),
          size_(0),
          maxLoadFactor_(maxLoadFactor) {
        buckets_ = new HashNode*[bucketCount_]();
    }

    ~HashTable() {
        clear();
        delete[] buckets_;
    }

    // Disable copy
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    // Move constructor
    HashTable(HashTable&& other) noexcept
        : buckets_(other.buckets_), bucketCount_(other.bucketCount_),
          size_(other.size_), hasher_(std::move(other.hasher_)),
          maxLoadFactor_(other.maxLoadFactor_) {
        other.buckets_ = nullptr;
        other.bucketCount_ = 0;
        other.size_ = 0;
    }

    HashTable& operator=(HashTable&& other) noexcept {
        if (this != &other) {
            clear();
            delete[] buckets_;
            buckets_ = other.buckets_;
            bucketCount_ = other.bucketCount_;
            size_ = other.size_;
            hasher_ = std::move(other.hasher_);
            maxLoadFactor_ = other.maxLoadFactor_;
            other.buckets_ = nullptr;
            other.bucketCount_ = 0;
            other.size_ = 0;
        }
        return *this;
    }

    /**
     * @brief Inserts or updates a key-value pair.
     * Time Complexity: O(1) average, O(N) worst-case.
     */
    void insert(const Key& key, const Value& value) {
        if (static_cast<float>(size_ + 1) / bucketCount_ > maxLoadFactor_) {
            rehash(bucketCount_ * 2 + 1);
        }

        size_t idx = getBucketIndex(key);
        HashNode* curr = buckets_[idx];
        while (curr != nullptr) {
            if (curr->key == key) {
                curr->value = value; // Key already exists: update value
                return;
            }
            curr = curr->next;
        }

        // Key not found: insert at head of bucket chain
        HashNode* newNode = new HashNode(key, value);
        newNode->next = buckets_[idx];
        buckets_[idx] = newNode;
        size_++;
    }

    /**
     * @brief Searches for a key in the hash table.
     * @return Pointer to Value if found, nullptr otherwise.
     * Time Complexity: O(1) average.
     */
    Value* search(const Key& key) {
        size_t idx = getBucketIndex(key);
        HashNode* curr = buckets_[idx];
        while (curr != nullptr) {
            if (curr->key == key) {
                return &(curr->value);
            }
            curr = curr->next;
        }
        return nullptr;
    }

    const Value* search(const Key& key) const {
        size_t idx = getBucketIndex(key);
        HashNode* curr = buckets_[idx];
        while (curr != nullptr) {
            if (curr->key == key) {
                return &(curr->value);
            }
            curr = curr->next;
        }
        return nullptr;
    }

    bool contains(const Key& key) const {
        return search(key) != nullptr;
    }

    /**
     * @brief Removes a key-value pair from the hash table.
     * Time Complexity: O(1) average.
     */
    bool remove(const Key& key) {
        size_t idx = getBucketIndex(key);
        HashNode* curr = buckets_[idx];
        HashNode* prev = nullptr;

        while (curr != nullptr) {
            if (curr->key == key) {
                if (prev == nullptr) {
                    buckets_[idx] = curr->next;
                } else {
                    prev->next = curr->next;
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

    size_t size() const {
        return size_;
    }

    bool isEmpty() const {
        return size_ == 0;
    }

    /**
     * @brief Returns all stored values in the hash table.
     */
    std::vector<Value> getAllValues() const {
        std::vector<Value> values;
        values.reserve(size_);
        for (size_t i = 0; i < bucketCount_; ++i) {
            HashNode* curr = buckets_[i];
            while (curr != nullptr) {
                values.push_back(curr->value);
                curr = curr->next;
            }
        }
        return values;
    }

    void display() const {
        std::cout << "--- Hash Table (Buckets: " << bucketCount_ << ", Size: " << size_ << ") ---\n";
        for (size_t i = 0; i < bucketCount_; ++i) {
            if (buckets_[i] != nullptr) {
                std::cout << "Bucket [" << i << "]: ";
                HashNode* curr = buckets_[i];
                while (curr != nullptr) {
                    std::cout << "{" << curr->key << "} -> ";
                    curr = curr->next;
                }
                std::cout << "null\n";
            }
        }
    }

    void clear() {
        for (size_t i = 0; i < bucketCount_; ++i) {
            HashNode* curr = buckets_[i];
            while (curr != nullptr) {
                HashNode* temp = curr;
                curr = curr->next;
                delete temp;
            }
            buckets_[i] = nullptr;
        }
        size_ = 0;
    }
};

#endif // HASH_TABLE_H
