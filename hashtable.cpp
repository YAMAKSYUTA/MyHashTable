#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>


// Hash table with open addressing, linear probing and lazy deletion.
// Using dynamic rehashing with doubling and halving size.
// https://en.wikipedia.org/wiki/Open_addressing
template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
 public:
    // default_size - size of hash table
    // when first initialized or cleared.
    constexpr static size_t default_size = 8;
    constexpr static size_t decreasing_size = 2;
    constexpr static size_t increasing_size = 2;
    constexpr static size_t allowed_deleted_elements = 2;
    constexpr static double overload_size = 0.75;

    // Iterator allows to iterate over elements in table and work with them.
    class iterator {
     public:
        // Default constructor.
        iterator() : pos(0), ptr(nullptr), check(nullptr),
            is_empty(nullptr) {}

        
        // Constructor with the given position.
        iterator(size_t pos_,
            std::vector<std::pair<KeyType, ValueType>>& ar,
            std::vector<bool>& used_, std::vector<bool>& is_empty_) :
            pos(pos_),
            ptr(reinterpret_cast<std::vector<std::pair<const KeyType, ValueType>>*>(&ar)),
            check(&used_),
            is_empty(&is_empty_) {
            go();
        }
        // Pre-increment iterator in O(1) amortized.
        iterator operator++() {
            ++pos;
            go();
            return (*this);
        }

        // Post-increment iterator in O(1) amortized.
        iterator operator++(int) {
            iterator res = *this;
            ++pos;
            go();
            return res;
        }

        // Return true if iterators are the same in O(1).
        bool operator==(iterator oth) const {
            return pos == oth.pos && ptr == oth.ptr;
        }

        // Return true if iterators are different in O(1).
        bool operator!=(iterator oth) const {
            return !((*this) == oth);
        }

        // Returns item reference in O(1) time.
        std::pair<const KeyType, ValueType>* operator->() {
            return &((*ptr)[pos]);
        }

        // Returns item object in O(1) time.
        std::pair<const KeyType, ValueType> operator*() {
            return (*ptr)[pos];
        }

     private:
        size_t pos;
        std::vector<std::pair<const KeyType, ValueType>>* ptr;
        std::vector<bool>* check;
        std::vector<bool>* is_empty;
        // Finds the next element in O(1) amortized.
        void go() {
            while (pos < ptr->size() &&
                ((*is_empty)[pos] || (*check)[pos])) {
                ++pos;
            }
        }
    };

    // Const iterator allows to iterate over the elements in table and get
    // their values but doesn't allow to change them.
    class const_iterator {
     public:
        // Default constructor.
        const_iterator() : pos(0), ptr(nullptr), check(nullptr),
            is_empty(nullptr) {}

        // Constructor with the given position.
        const_iterator(size_t pos_,
            const std::vector<std::pair<KeyType, ValueType>>& ar,
            const std::vector<bool>& used_,
            const std::vector<bool>& is_empty_) : pos(pos_),
            ptr(reinterpret_cast<const std::vector<std::pair<const KeyType, ValueType>>*>(&ar)),
            check(&used_), is_empty(&is_empty_) {
            go();
        }

        // Pre-increment iterator in O(1) amortized.
        const_iterator operator++() {
            ++pos;
            go();
            return (*this);
        }

        // Post-increment iterator in O(1) amortized.
        const_iterator operator++(int) {
            const_iterator res = *this;
            ++pos;
            go();
            return res;
        }

        // Return true if iterators are the same in O(1).
        bool operator==(const_iterator oth) const {
            return pos == oth.pos && ptr == oth.ptr;
        }

        // Return true if iterators are different in O(1).
        bool operator!=(const_iterator oth) const {
            return !((*this) == oth);
        }

        // Returns constant item reference in O(1) time.
        std::pair<const KeyType, ValueType>* operator->() {
            return const_cast<std::pair<const KeyType, ValueType>*>(&((*ptr)[pos]));
        }

        // Returns constant item object in O(1) time.
        std::pair<const KeyType, ValueType> operator*() {
            return (*ptr)[pos];
        }

     private:
        size_t pos;
        const std::vector<std::pair<const KeyType, ValueType>>* ptr;
        const std::vector<bool>* check;
        const std::vector<bool>* is_empty;

        // Finds the next element in O(1) amortized.
        void go() {
            while (pos < ptr->size() &&
                ((*is_empty)[pos] || (*check)[pos])) {
                ++pos;
            }
        }
    };

    // Default constructor with given hash function.
    HashMap(Hash hasher_ = Hash()) : hasher(hasher_) {
        init();
    }

    // Constructor for initializer list with given hash function.
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> list,
        Hash hasher_ = Hash()) :
        HashMap(list.begin(), list.end(), hasher_) {}

    // Copy constructor.
    HashMap(const HashMap& oth) : hasher(oth.hasher) {
        init();
        for (auto it = oth.begin(); it != oth.end(); ++it) {
            insert(*it);
        }
    }

    // Constructor for given begin and end iterator.
    template<typename It>
    HashMap(It begin, It end, Hash hasher_ = Hash()) : hasher(hasher_) {
        init();
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }

    // Copies other hash table.
    HashMap& operator=(const HashMap &oth) {
        if (&oth != this) {
            hasher = oth.hasher;
            clear();
            for (auto it : oth) {
                insert(it);
            }
        }
        return (*this);
    }

    // Deletes all elements in table in O(size)
    // and resets table to its beginning conditions.
    void clear() {
        arr.clear();
        init();
    }

    // Calls increase_size() method of a table if needed.
    void check_density() {
        if (sz + 1 > size_t(overload_size * buffer_size)) {
            increase_size();
        }
    }

    // Calls decrease_size() method of a table if needed.
    void check_deleted_elements() {
        if (size_all_non_nullptr > allowed_deleted_elements * sz) {
            decrease_size();
        }
    }

    // Adds a new pair of key and value to the table in O(1) amortized.
    // Does nothing if key already exists.
    void insert(const std::pair<KeyType, ValueType>& item) {
        check_density();
        check_deleted_elements();
        
        // Finds a position for element or checks if key already exists.
        size_t hash = hasher(item.first) % buffer_size;
        size_t i = 0;
        // found == true if we want to insert an element to a position
        // where we have a deleted element.
        bool found = false;
        size_t first_deleted = 0;
        while (!is_nullptr[hash] && i < buffer_size) {
            if (arr[hash].first == item.first && !used[hash]) {
                return;
            }
            if (used[hash] && !found) {
                first_deleted = hash;
                found = true;
                break;
            }
            hash = (hash + 1) % buffer_size;
            ++i;
        }

        // Adds an element to a found position.
        if (!found) {
            is_nullptr[hash] = false;
            arr[hash] = item;
            ++size_all_non_nullptr;
        } else {
            is_nullptr[first_deleted] = false;
            arr[first_deleted] = item;
            used[first_deleted] = false;
        }
        ++sz;
    }

    // Delete an element with the given key in O(1) amortized.
    // Does nothing if key doesn't exist in table.
    void erase(const KeyType& key) {
        size_t hash = hasher(key) % buffer_size;
        size_t i = 0;
        while (!is_nullptr[hash] && i < buffer_size) {
            if (arr[hash].first == key && !used[hash]) {
                used[hash] = true;
                --sz;
                check_deleted_elements();
                return;
            }
            hash = (hash + 1) % buffer_size;
            ++i;
        }
    }

    // Returns amount of elements in table in O(1).
    size_t size() const {
        return sz;
    }

    // Returns true if there are no elements in table in O(1).
    bool empty() const {
        return sz == 0;
    }

    // Returns hash function of table in O(1).
    Hash hash_function() const {
        return hasher;
    }

    // Returns reference to an object with the given key
    // in O(1) amortized time.
    // If key doesn't exist adds a new pair and
    // retuns reference to it.
    ValueType& operator[](const KeyType& key) {
        size_t hash = hasher(key) % buffer_size;
        size_t i = 0;
        while (!is_nullptr[hash] && i < buffer_size) {
            if (arr[hash].first == key && !used[hash]) {
                return arr[hash].second;
            }
            hash = (hash + 1) % buffer_size;
            ++i;
        }
        insert({ key, ValueType() });
        return find(key)->second;
    }

    // Returns reference to an object with the given key
    // in O(1) amortized time.
    // If key doesn't exist throws an exception.
    const ValueType& at(const KeyType& key) const {
        size_t hash = hasher(key) % buffer_size;
        size_t i = 0;
        while (!is_nullptr[hash] && i < buffer_size) {
            if (arr[hash].first == key && !used[hash]) {
                return arr[hash].second;
            }
            hash = (hash + 1) % buffer_size;
            ++i;
        }
        throw std::out_of_range("out of range");
    }

    // Returns iterator to the first element in O(1) amortized.
    iterator begin() {
        return iterator(0, arr, used, is_nullptr);
    }

    // Returns iterator to the end of the table in O(1).
    iterator end() {
        return iterator(buffer_size, arr, used, is_nullptr);
    }

    // Returns const_iterator to the first element in O(1) amortized.
    const_iterator begin() const {
        return const_iterator(0, arr, used, is_nullptr);
    }

    // Returns const_iterator to the end of the table in O(1).
    const_iterator end() const {
        return const_iterator(buffer_size, arr, used, is_nullptr);
    }

    // Returns iterator to the pair with the given key
    // or end() if key doesn't exist in O(1) amortized.
    iterator find(const KeyType& key) {
        size_t hash = hasher(key) % buffer_size;
        size_t i = 0;
        while (!is_nullptr[hash] && i < buffer_size) {
            if (arr[hash].first == key && !used[hash]) {
                return iterator(hash, arr, used, is_nullptr);
            }
            hash = (hash + 1) % buffer_size;
            ++i;
        }
        return end();
    }

    // Returns const_iterator to the pair with the given key
    // or end() if key doesn't exist in O(1) amortized.
    const_iterator find(const KeyType& key) const {
        size_t hash = hasher(key) % buffer_size;
        size_t i = 0;
        while (!is_nullptr[hash] && i < buffer_size) {
            if (arr[hash].first == key && !used[hash]) {
                return const_iterator(hash, arr, used, is_nullptr);
            }
            hash = (hash + 1) % buffer_size;
            ++i;
        }
        return end();
    }

 private:
    Hash hasher;
    // buffer_size - size of current state of hash table.
    size_t sz, buffer_size, size_all_non_nullptr;
    std::vector<bool> used, is_nullptr;
    std::vector<std::pair<KeyType, ValueType>> arr;

    // Initializes table.
    void init() {
        buffer_size = default_size;
        sz = 0;
        size_all_non_nullptr = 0;
        arr = std::vector<std::pair<KeyType, ValueType>>(buffer_size);
        used.clear();
        used.resize(default_size, false);
        is_nullptr.clear();
        is_nullptr.resize(default_size, true);
    }

    // Increases the size of a table and reinserts all elements in O(size) time.
    void increase_size() {
        size_t past_buffer_size = buffer_size;
        buffer_size *= increasing_size;
        size_all_non_nullptr = 0;
        sz = 0;
        std::vector<bool> prev_used = used;
        std::vector<bool> prev_nullptr = is_nullptr;
        used.clear();
        used.resize(buffer_size, false);
        is_nullptr.clear();
        is_nullptr.resize(buffer_size, true);
        std::vector<std::pair<KeyType, ValueType>> prev_arr(buffer_size);
        std::swap(arr, prev_arr);
        for (size_t i = 0; i < past_buffer_size; ++i) {
            if (!prev_nullptr[i] && !prev_used[i]) {
                insert(prev_arr[i]);
            }
        }
    }

    // Decreases the size of a table and reinserts all elements in O(size) time.
    void decrease_size() {
        size_t past_buffer_size = buffer_size;
        buffer_size /= decreasing_size;
        size_all_non_nullptr = 0;
        sz = 0;
        std::vector<bool> prev_used = used;
        std::vector<bool> prev_nullptr = is_nullptr;
        used.clear();
        used.resize(buffer_size, false);
        is_nullptr.clear();
        is_nullptr.resize(buffer_size, true);
        std::vector<std::pair<KeyType, ValueType>> prev_arr(buffer_size);
        std::swap(arr, prev_arr);
        for (size_t i = 0; i < past_buffer_size; ++i) {
            if (!prev_nullptr[i] && !prev_used[i]) {
                insert(prev_arr[i]);
            }
        }
    }
};
