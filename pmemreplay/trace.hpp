#pragma once

#include <iostream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <fstream>
#include <optional>

enum class TraceOperation {
    READ = 0,
    WRITE = 1,
    CLFLUSH = 2,
    UNKNOWN = 3,
};

struct TraceEntry {
public:
    TraceEntry(const TraceOperation op, const size_t op_size, const unsigned int opcode, const double timestamp_sec, const unsigned long abs_addr, const unsigned long rel_addr, const uint64_t data) :
        op(op),
        op_size(op_size),
        opcode(opcode),
        timestamp_sec(timestamp_sec),
        abs_addr(abs_addr),
        rel_addr(rel_addr),
        dax_addr(nullptr),
        data(data)
    {};

    TraceEntry() {
        reset();
    }

    void reset() {
        this->op = TraceOperation::UNKNOWN;
        this->opcode = 0x0000;
        this->op_size = 0;
        this->timestamp_sec = 0;
        this->abs_addr = 0;
        this->rel_addr = 0;
        this->dax_addr = nullptr;
        this->data = 0;
    }

    /*
        parquet::schema::PrimitiveNode::Make("timestamp", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
        parquet::schema::PrimitiveNode::Make("op", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
        parquet::schema::PrimitiveNode::Make("opcode", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
        parquet::schema::PrimitiveNode::Make("op_size", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
        parquet::schema::PrimitiveNode::Make("abs_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
        parquet::schema::PrimitiveNode::Make("rel_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
        parquet::schema::PrimitiveNode::Make("data", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
    */

    double timestamp_sec;
    TraceOperation op;
    unsigned int opcode;
    size_t op_size;
    unsigned long abs_addr;
    unsigned long rel_addr;
    uint64_t data;
    void* dax_addr;
    //const std::vector<uint8_t> data;
};

class TraceFile : public std::vector<TraceEntry> {
public:
    // Define a nested iterator class
    class Iterator {
    public:
        // Define the iterator typedefs required by the STL
        using iterator_category = std::input_iterator_tag;
        using value_type = TraceEntry;
        using difference_type = ptrdiff_t;
        using pointer = TraceEntry*;
        using reference = TraceEntry&;

        // Constructor takes a pointer to the underlying vector and an index
        Iterator(std::vector<TraceEntry>* vec, size_t index)
            : m_vec(vec), m_index(index)
        {}

        // Prefix increment operator (++it)
        Iterator& operator++() {
            ++m_index;
            return *this;
        }

        // Postfix increment operator (it++)
        Iterator operator++(int) {
            Iterator temp(*this);
            ++m_index;
            return temp;
        }

        // Dereference operator
        reference operator*() {
            return (*m_vec)[m_index];
        }

        // Member access operator
        pointer operator->() {
            return &(*m_vec)[m_index];
        }

        // Equality operator
        bool operator==(const Iterator& other) const {
            return m_vec == other.m_vec && m_index == other.m_index;
        }

        // Inequality operator
        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        std::vector<TraceEntry>* m_vec;
        size_t m_index;
    };



    Iterator begin() {
        return Iterator(this, 0);
    }

    Iterator end() {
        return Iterator(this, size());
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        std::vector<TraceEntry>::emplace_back(std::forward<Args>(args)...);
    }

    size_t get_total(const TraceOperation &op)
    {
        size_t total = 0;

        for (const auto &entry : *(this)) {
            if (entry.op == op)
                total += entry.op_size;
        }

        return total;
    }

    size_t get_total_all_ops()
    {
        size_t total = 0;

        total += this->get_total(TraceOperation::READ);
        total += this->get_total(TraceOperation::WRITE);

        return total;
    }

    size_t get_size()
    {
        return size();
    }
};



bool parse_trace(const std::filesystem::path &path, TraceFile &trace);
