#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Robix::StdLib {

// ============================================================================
// 1. FORMAL ZERO-COST ITERATOR PROTOCOL (Iterator<T> & EnumerateItem<T>)
// ============================================================================
template <typename T>
struct EnumerateItem {
    std::size_t index = 0;
    T           value;
};

template <typename T>
class VectorIterator {
public:
    explicit VectorIterator(const std::vector<T>& data) noexcept
        : m_data(&data), m_index(0) {}

    [[nodiscard]] std::optional<T> Next() {
        if (!m_data || m_index >= m_data->size()) {
            return std::nullopt;
        }
        return (*m_data)[m_index++];
    }

    [[nodiscard]] bool HasNext() const noexcept {
        return m_data && m_index < m_data->size();
    }

private:
    const std::vector<T>* m_data = nullptr;
    std::size_t           m_index = 0;
};

// ============================================================================
// 2. LIST<T> WITH FUNCTIONAL PIPELINES & ITERATOR PROTOCOL
// ============================================================================
template <typename T>
class List {
public:
    List() = default;
    explicit List(std::vector<T> items) : m_items(std::move(items)) {}

    void Add(T item) { m_items.push_back(std::move(item)); }

    bool Remove(const T& item) {
        auto it = std::find(m_items.begin(), m_items.end(), item);
        if (it == m_items.end()) return false;
        m_items.erase(it);
        return true;
    }

    [[nodiscard]] const T& Get(std::size_t index) const {
        if (index >= m_items.size()) {
            throw std::out_of_range("Robix List<T>::Get out of bounds");
        }
        return m_items[index];
    }

    void Set(std::size_t index, T value) {
        if (index >= m_items.size()) {
            throw std::out_of_range("Robix List<T>::Set out of bounds");
        }
        m_items[index] = std::move(value);
    }

    [[nodiscard]] bool Contains(const T& item) const {
        return std::find(m_items.begin(), m_items.end(), item) != m_items.end();
    }

    [[nodiscard]] std::optional<T> Find(const std::function<bool(const T&)>& predicate) const {
        for (const auto& el : m_items) {
            if (predicate(el)) return el;
        }
        return std::nullopt;
    }

    [[nodiscard]] List<T> Filter(const std::function<bool(const T&)>& predicate) const {
        List<T> out;
        for (const auto& el : m_items) {
            if (predicate(el)) {
                out.Add(el);
            }
        }
        return out;
    }

    template <typename U>
    [[nodiscard]] List<U> Map(const std::function<U(const T&)>& transform) const {
        List<U> out;
        out.m_items.reserve(m_items.size());
        for (const auto& el : m_items) {
            out.Add(transform(el));
        }
        return out;
    }

    template <typename Acc>
    [[nodiscard]] Acc Reduce(Acc initial, const std::function<Acc(const Acc&, const T&)>& folder) const {
        Acc acc = std::move(initial);
        for (const auto& el : m_items) {
            acc = folder(acc, el);
        }
        return acc;
    }

    void Sort() { std::sort(m_items.begin(), m_items.end()); }
    void Reverse() { std::reverse(m_items.begin(), m_items.end()); }
    void Clear() noexcept { m_items.clear(); }

    [[nodiscard]] std::size_t Count() const noexcept { return m_items.size(); }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_items.empty(); }

    [[nodiscard]] std::vector<EnumerateItem<T>> Enumerate() const {
        std::vector<EnumerateItem<T>> out;
        out.reserve(m_items.size());
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            out.push_back(EnumerateItem<T>{ i, m_items[i] });
        }
        return out;
    }

    [[nodiscard]] VectorIterator<T> GetIterator() const noexcept {
        return VectorIterator<T>(m_items);
    }

    [[nodiscard]] const std::vector<T>& Raw() const noexcept { return m_items; }

    template <typename U>
    friend class List;

private:
    std::vector<T> m_items;
};

// ============================================================================
// 3. DICTIONARY<K, V> / HASHMAP<K, V> & HASHSET<T>
// ============================================================================
template <typename K, typename V>
class Dictionary {
public:
    void Set(K key, V value) { m_map[std::move(key)] = std::move(value); }

    [[nodiscard]] std::optional<V> Get(const K& key) const {
        auto it = m_map.find(key);
        if (it == m_map.end()) return std::nullopt;
        return it->second;
    }

    [[nodiscard]] bool Contains(const K& key) const {
        return m_map.find(key) != m_map.end();
    }

    bool Remove(const K& key) {
        return m_map.erase(key) > 0;
    }

    void Clear() noexcept { m_map.clear(); }
    [[nodiscard]] std::size_t Count() const noexcept { return m_map.size(); }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_map.empty(); }

private:
    std::unordered_map<K, V> m_map;
};

template <typename K, typename V>
using HashMap = Dictionary<K, V>;

template <typename T>
class HashSet {
public:
    bool Add(T item) { return m_set.insert(std::move(item)).second; }
    bool Remove(const T& item) { return m_set.erase(item) > 0; }
    [[nodiscard]] bool Contains(const T& item) const { return m_set.find(item) != m_set.end(); }
    void Clear() noexcept { m_set.clear(); }
    [[nodiscard]] std::size_t Count() const noexcept { return m_set.size(); }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_set.empty(); }

private:
    std::unordered_set<T> m_set;
};

// ============================================================================
// 4. STACK<T>, QUEUE<T> & PRIORITYQUEUE<T>
// ============================================================================
template <typename T>
class Stack {
public:
    void Push(T item) { m_data.push_back(std::move(item)); }
    [[nodiscard]] std::optional<T> Pop() {
        if (m_data.empty()) return std::nullopt;
        T v = std::move(m_data.back());
        m_data.pop_back();
        return v;
    }
    [[nodiscard]] std::optional<T> Peek() const {
        if (m_data.empty()) return std::nullopt;
        return m_data.back();
    }
    [[nodiscard]] std::size_t Count() const noexcept { return m_data.size(); }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_data.empty(); }

private:
    std::vector<T> m_data;
};

template <typename T>
class Queue {
public:
    void Enqueue(T item) { m_deque.push_back(std::move(item)); }
    [[nodiscard]] std::optional<T> Dequeue() {
        if (m_deque.empty()) return std::nullopt;
        T v = std::move(m_deque.front());
        m_deque.pop_front();
        return v;
    }
    [[nodiscard]] std::size_t Count() const noexcept { return m_deque.size(); }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_deque.empty(); }

private:
    std::deque<T> m_deque;
};

template <typename T>
class PriorityQueue {
public:
    void Enqueue(T item) { m_pq.push(std::move(item)); }
    [[nodiscard]] std::optional<T> Dequeue() {
        if (m_pq.empty()) return std::nullopt;
        T v = m_pq.top();
        m_pq.pop();
        return v;
    }
    [[nodiscard]] std::size_t Count() const noexcept { return m_pq.size(); }
    [[nodiscard]] bool IsEmpty() const noexcept { return m_pq.empty(); }

private:
    std::priority_queue<T, std::vector<T>, std::greater<T>> m_pq; // Min-priority queue
};

// ============================================================================
// 5. GRAPH<T> (DIRECTED ADJACENCY GRAPH + BFS SHORTEST PATH)
// ============================================================================
template <typename T>
class Graph {
public:
    void AddEdge(const T& from, const T& to) {
        m_adj[from].push_back(to);
    }

    [[nodiscard]] bool HasPathBFS(const T& start, const T& goal) const {
        if (start == goal) return true;
        std::unordered_set<T> visited;
        std::deque<T> q;
        visited.insert(start);
        q.push_back(start);

        while (!q.empty()) {
            T cur = q.front();
            q.pop_front();
            auto it = m_adj.find(cur);
            if (it == m_adj.end()) continue;
            for (const auto& nxt : it->second) {
                if (nxt == goal) return true;
                if (visited.insert(nxt).second) {
                    q.push_back(nxt);
                }
            }
        }
        return false;
    }

private:
    std::unordered_map<T, std::vector<T>> m_adj;
};

// ============================================================================
// 6. MATRIX<T> & TENSOR<T> (CONTIGUOUS ROW-MAJOR SCIENTIFIC COMPUTING)
// ============================================================================
template <typename T>
class Matrix {
public:
    Matrix(std::size_t rows, std::size_t cols, T initialValue = T{})
        : m_rows(rows), m_cols(cols), m_data(rows * cols, initialValue) {}

    [[nodiscard]] std::size_t Rows() const noexcept { return m_rows; }
    [[nodiscard]] std::size_t Cols() const noexcept { return m_cols; }

    T& At(std::size_t r, std::size_t c) {
        if (r >= m_rows || c >= m_cols) throw std::out_of_range("Matrix::At out of bounds");
        return m_data[r * m_cols + c];
    }

    [[nodiscard]] const T& At(std::size_t r, std::size_t c) const {
        if (r >= m_rows || c >= m_cols) throw std::out_of_range("Matrix::At out of bounds");
        return m_data[r * m_cols + c];
    }

    [[nodiscard]] Matrix<T> Multiply(const Matrix<T>& rhs) const {
        if (m_cols != rhs.m_rows) {
            throw std::invalid_argument("Matrix::Multiply dimension mismatch");
        }
        Matrix<T> out(m_rows, rhs.m_cols, T{});
        for (std::size_t r = 0; r < m_rows; ++r) {
            for (std::size_t k = 0; k < m_cols; ++k) {
                const T lhsVal = At(r, k);
                for (std::size_t c = 0; c < rhs.m_cols; ++c) {
                    out.At(r, c) += lhsVal * rhs.At(k, c);
                }
            }
        }
        return out;
    }

    [[nodiscard]] Matrix<T> Transpose() const {
        Matrix<T> out(m_cols, m_rows, T{});
        for (std::size_t r = 0; r < m_rows; ++r) {
            for (std::size_t c = 0; c < m_cols; ++c) {
                out.At(c, r) = At(r, c);
            }
        }
        return out;
    }

private:
    std::size_t    m_rows = 0;
    std::size_t    m_cols = 0;
    std::vector<T> m_data;
};

template <typename T>
class Tensor {
public:
    explicit Tensor(std::vector<std::size_t> shape, T initialValue = T{})
        : m_shape(std::move(shape)) {
        std::size_t total = m_shape.empty() ? 0 : 1;
        for (std::size_t d : m_shape) total *= d;
        m_data.assign(total, initialValue);
    }

    [[nodiscard]] const std::vector<std::size_t>& Shape() const noexcept { return m_shape; }
    [[nodiscard]] std::size_t ElementCount() const noexcept { return m_data.size(); }
    [[nodiscard]] std::vector<T>& Data() noexcept { return m_data; }
    [[nodiscard]] const std::vector<T>& Data() const noexcept { return m_data; }

private:
    std::vector<std::size_t> m_shape;
    std::vector<T>           m_data;
};

} // namespace Robix::StdLib

