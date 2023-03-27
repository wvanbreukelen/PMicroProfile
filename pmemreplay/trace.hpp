#pragma once

#include <iostream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <fstream>
#include <optional>

enum class TraceOperation {
    READ,
    WRITE
};

struct TraceEntry {
public:
    TraceEntry(const TraceOperation op, const size_t op_size, const double timestamp_sec, const unsigned long addr) :
        op(op),
        op_size(op_size),
        timestamp_sec(timestamp_sec),
        addr(addr),
        data(0),
        data_size(0)
    {};

    const TraceOperation op;
    const size_t op_size;
    const double timestamp_sec;
    const unsigned long addr;
    const uint8_t* data;
    const size_t data_size;
};

class TraceFile : protected std::vector<TraceEntry> {
public:
    // Define a nested iterator class
    class ConstIterator {
    public:
        // Define the iterator typedefs required by the STL
        using iterator_category = std::input_iterator_tag;
        using value_type = TraceEntry;
        using difference_type = ptrdiff_t;
        using pointer = const TraceEntry*;
        using reference = const TraceEntry&;

        // Constructor takes a pointer to the underlying vector and an index
        ConstIterator(const std::vector<TraceEntry>* vec, size_t index)
            : m_vec(vec), m_index(index)
        {}

        // Prefix increment operator (++it)
        ConstIterator& operator++() {
            ++m_index;
            return *this;
        }

        // Postfix increment operator (it++)
        ConstIterator operator++(int) {
            ConstIterator temp(*this);
            ++m_index;
            return temp;
        }

        // Dereference operator
        reference operator*() const {
            return (*m_vec)[m_index];
        }

        // Member access operator
        pointer operator->() const {
            return &(*m_vec)[m_index];
        }

        // Equality operator
        bool operator==(const ConstIterator& other) const {
            return m_vec == other.m_vec && m_index == other.m_index;
        }

        // Inequality operator
        bool operator!=(const ConstIterator& other) const {
            return !(*this == other);
        }

    private:
        const std::vector<TraceEntry>* m_vec;
        size_t m_index;
    };



    ConstIterator begin() const {
        return ConstIterator(this, 0);
    }

    ConstIterator end() const {
        return ConstIterator(this, size());
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        std::vector<TraceEntry>::emplace_back(std::forward<Args>(args)...);
    }

    size_t get_total(const TraceOperation &op) const
    {
        size_t total = 0;

        for (const auto &entry : *(this)) {
            if (entry.op == op)
                total += entry.op_size;
        }

        return total;
    }
};



std::optional<TraceFile> parse_trace(const std::string& filename);