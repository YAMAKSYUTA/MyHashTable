#include <functional>
#include <cstddef>
#include <vector>
#include <iostream>

template<class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
private:
    Hash hasher;
    static const size_t default_size = 8;
    constexpr static const double rehash_size = 0.75;
    size_t sz, buffer_size, size_all_non_nullptr;
    std::vector<bool> used;
    std::vector<std::pair<const KeyType, ValueType>*> arr;

    void resize() {
        size_t past_buffer_size = buffer_size;
        buffer_size *= 2;
        size_all_non_nullptr = 0;
        sz = 0;
        std::vector<bool> used2 = used;
        used.resize(buffer_size, false);
        std::vector<std::pair<const KeyType, ValueType>*> arr2(buffer_size);
        for (size_t i = 0; i < buffer_size; ++i) {
            arr2[i] = nullptr;
        }
        std::swap(arr, arr2);
        for (size_t i = 0; i < past_buffer_size; ++i) {
            if (arr2[i] && !used2[i])
                insert(*arr2[i]);
        }
        for (size_t i = 0; i < past_buffer_size; ++i) {
            if (arr2[i]) {
                delete arr2[i];
            }
        }
    }

    void rehash() {
        size_all_non_nullptr = 0;
        sz = 0;
        std::vector<bool> used2 = used;
        std::fill(used.begin(), used.end(), false);
        std::vector<std::pair<const KeyType, ValueType>*> arr2(buffer_size);
        for (size_t i = 0; i < buffer_size; ++i) {
            arr2[i] = nullptr;
        }
        std::swap(arr, arr2);
        for (size_t i = 0; i < buffer_size; ++i) {
            if (arr2[i] && !used2[i]) {
                insert(*arr2[i]);
            }
        }
        for (size_t i = 0; i < buffer_size; ++i) {
            if (arr2[i]) {
                delete arr2[i];
            }
        }
    }

    void init() {
        buffer_size = default_size;
        sz = 0;
        size_all_non_nullptr = 0;
        arr = std::vector<std::pair<const KeyType, ValueType>*>(buffer_size);
        for (size_t i = 0; i < buffer_size; ++i) {
            arr[i] = nullptr;
        }
        used.clear();
        used.resize(default_size, false);
    }

public:
    class iterator {
    private:
        size_t pos;
        std::vector<std::pair<const KeyType, ValueType>*>* ptr;
        std::vector<bool>* check;
        void go() {
            while (pos < ptr->size() && ((*ptr)[pos] == nullptr || (*check)[pos])) {
                ++pos;
            }
        }
    public:
        iterator() : pos(0), ptr(nullptr), check(nullptr) {}

        iterator(size_t pos_, std::vector<std::pair<const KeyType, ValueType>*>& ar, 
            std::vector<bool>& used_) : pos(pos_), ptr(&ar), check(&used_) {
            go();
        }

        iterator operator++() {
            ++pos;
            go();
            return (*this);
        }

        iterator operator++(int) {
            iterator res = *this;
            ++pos;
            go();
            return res;
        }

        bool operator==(iterator oth) const {
            return pos == oth.pos && ptr == oth.ptr;
        }

        bool operator!=(iterator oth) const {
            return !((*this) == oth);
        }

        std::pair<const KeyType, ValueType>* operator->() {
            return (*ptr)[pos];
        }

        std::pair<KeyType, ValueType> operator*() {
            return *(*ptr)[pos];
        }
    };

    class const_iterator {
    private:
        size_t pos;
        const std::vector<std::pair<const KeyType, ValueType>*>* ptr;
        const std::vector<bool>* check;
        void go() {
            while (pos < ptr->size() && ((*ptr)[pos] == nullptr || (*check)[pos])) {
                ++pos;
            }
        }
    public:
        const_iterator() : pos(0), ptr(nullptr), check(nullptr) {}

        const_iterator(size_t pos_, const std::vector<std::pair<const KeyType, ValueType>*>& ar, 
            const std::vector<bool>& used_) : pos(pos_), ptr(&ar), check(&used_) {
            go();
        }

        const_iterator operator++() {
            ++pos;
            go();
            return (*this);
        }

        const_iterator operator++(int) {
            const_iterator res = *this;
            ++pos;
            go();
            return res;
        }

        bool operator==(const_iterator oth) const {
            return pos == oth.pos && ptr == oth.ptr;
        }

        bool operator!=(const_iterator oth) const {
            return !((*this) == oth);
        }

        std::pair<const KeyType, ValueType>* operator->() {
            return (*ptr)[pos];
        }

        std::pair<KeyType, ValueType> operator*() {
            return *(*ptr)[pos];
        }
    };

    HashMap(Hash hasher_ = Hash()) : hasher(hasher_) {
        init();
    }

    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> list, Hash hasher_ = Hash()) {
        hasher = hasher_;
        init();
        for (auto item : list) {
            insert(item);
        }
    }

    HashMap(const HashMap& oth) : hasher(oth.hasher) {
        init();
        for (auto it = oth.begin(); it != oth.end(); ++it) {
            insert(*it);
        }
    }

    template<typename It>
    HashMap(It begin, It end, Hash hasher_ = Hash()) : hasher(hasher_) {
        init();
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }

    HashMap& operator=(const HashMap &oth) {
        if (oth.arr != arr) {
            clear();
            for (auto item : oth) {
                insert(item);
            }
        }
        return (*this);
    }

    ~HashMap() {
        for (size_t i = 0; i < buffer_size; ++i) {
            if (arr[i]) {
                delete arr[i];
            }
        }
    }

    void clear() {
        for (size_t i = 0; i < buffer_size; ++i) {
            if (arr[i]) {
                delete arr[i];
            }
        }
        init();
    }

    void insert(const std::pair<KeyType, ValueType>& item) {
        if (sz + 1 > size_t(rehash_size * buffer_size)) {
            resize();
        }
        else if (size_all_non_nullptr > 2 * sz) {
            rehash();
        }
        size_t h1 = hasher(item.first) % buffer_size;
        size_t i = 0;
        int first_deleted = -1;
        while (arr[h1] != nullptr && i < buffer_size) {
            if (arr[h1]->first == item.first && !used[h1])
                return;
            if (used[h1] && first_deleted == -1) {
                first_deleted = h1;
            }
            h1 = (h1 + 1) % buffer_size;
            ++i;
        }
        if (first_deleted == -1) {
            arr[h1] = new std::pair<const KeyType, ValueType>(item.first, item.second);
            ++size_all_non_nullptr;
        }
        else {
            delete arr[first_deleted];
            arr[first_deleted] = new std::pair<const KeyType, ValueType>(item.first, item.second);
            used[first_deleted] = false;
        }
        ++sz;
    }

    void erase(const KeyType& key) {
        size_t h1 = hasher(key) % buffer_size;
        size_t i = 0;
        while (arr[h1] != nullptr && i < buffer_size) {
            if (arr[h1]->first == key && !used[h1]) {
                used[h1] = true;
                --sz;
                return;
            }
            h1 = (h1 + 1) % buffer_size;
            ++i;
        }
    }

    size_t size() const {
        return sz;
    }

    bool empty() const {
        return sz == 0;
    }

    Hash hash_function() const {
        return hasher;
    }

    ValueType& operator[](const KeyType& key) {
        size_t h1 = hasher(key) % buffer_size;
        size_t i = 0;
        while (arr[h1] != nullptr && i < buffer_size) {
            if (arr[h1]->first == key && !used[h1]) {
                return arr[h1]->second;
            }
            h1 = (h1 + 1) % buffer_size;
            ++i;
        }
        insert({ key, ValueType() });
        return find(key)->second;
    }

    const ValueType& at(const KeyType& key) const {
        size_t h1 = hasher(key) % buffer_size;
        size_t i = 0;
        while (arr[h1] != nullptr && i < buffer_size) {
            if (arr[h1]->first == key && !used[h1]) {
                return arr[h1]->second;
            }
            h1 = (h1 + 1) % buffer_size;
            ++i;
        }
        throw std::out_of_range("out of range");
    }

    iterator begin() {
        return iterator(0, arr, used);
    }

    iterator end() {
        return iterator(buffer_size, arr, used);
    }

    const_iterator begin() const {
        return const_iterator(0, arr, used);
    }

    const_iterator end() const {
        return const_iterator(buffer_size, arr, used);
    }

    iterator find(const KeyType& key) {
        size_t h1 = hasher(key) % buffer_size;
        size_t i = 0;
        while (arr[h1] != nullptr && i < buffer_size) {
            if (arr[h1]->first == key && !used[h1]) {
                return iterator(h1, arr, used);
            }
            h1 = (h1 + 1) % buffer_size;
            ++i;
        }
        return end();
    }

    const_iterator find(const KeyType& key) const {
        size_t h1 = hasher(key) % buffer_size;
        size_t i = 0;
        while (arr[h1] != nullptr && i < buffer_size) {
            if (arr[h1]->first == key && !used[h1]) {
                return const_iterator(h1, arr, used);
            }
            h1 = (h1 + 1) % buffer_size;
            ++i;
        }
        return end();
    }
};
