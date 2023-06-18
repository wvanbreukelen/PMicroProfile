#pragma once

#include <immintrin.h>
#include <emmintrin.h>

#include "bench_suite.hpp"

constexpr uint64_t lat_sample_mask = ((1u << 8) - 1);
static uint64_t num_io = 1;

template<typename T>
inline void read_value(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    volatile T temp_var;

    #ifdef ENABLE_DCOLLECTION
    if (is_sampling) {
        const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);
        
        if (do_lat_measurement) {
            const auto op_start = std::chrono::steady_clock::now();
            temp_var = *(static_cast<T*>(entry.dax_addr));
            const auto op_end = std::chrono::steady_clock::now();

            cur_sample->read_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            cur_sample->read_inst_cycles_samples++;
        } else {
            temp_var = *(static_cast<T*>(entry.dax_addr));
        }
        ++(cur_sample->num_classic_rw);
        //cur_sample->read_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
    } else {
        temp_var = *(static_cast<T*>(entry.dax_addr));
    }
    #else
        temp_var = *(static_cast<T*>(entry.dax_addr));
    #endif

    (void) temp_var;
}

inline void write_mov_8(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
    if (is_sampling) {
        const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);
        
        if (do_lat_measurement) {
            const auto op_start = std::chrono::steady_clock::now();
            *(static_cast<uint8_t*>(entry.dax_addr)) = static_cast<uint8_t>(entry.data);
            const auto op_end = std::chrono::steady_clock::now();

            cur_sample->write_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            cur_sample->write_inst_cycles_samples++;
        } else {
            *(static_cast<uint8_t*>(entry.dax_addr)) = static_cast<uint8_t>(entry.data);
        }
        ++(cur_sample->num_classic_rw);
    } else {
        *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);
    }
    #else
        *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);
    #endif
}

inline void write_movnti_32(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
    if (is_sampling) {
        const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);

        if (do_lat_measurement) {
            const auto op_start = std::chrono::steady_clock::now();
            _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));
            const auto op_end = std::chrono::steady_clock::now();

            cur_sample->write_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            cur_sample->write_inst_cycles_samples++;
        } else {
            _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));
        }
        ++(cur_sample->num_movnti);
    } else {
        _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));
    }
    #else
        _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));
    #endif
}

inline void write_movntq_64(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    const __m64 v = _mm_set_pi64x(entry.data);

    #ifdef ENABLE_DCOLLECTION
    if (is_sampling) {
        const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);

        if (do_lat_measurement) {
            const auto op_start = std::chrono::steady_clock::now();
            _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
            const auto op_end = std::chrono::steady_clock::now();

            cur_sample->write_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            cur_sample->write_inst_cycles_samples++;
        } else {
            _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
        }
        ++(cur_sample->num_movntq);
        
    } else {
        _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
    }
    #else
        _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
    #endif
}

inline void write_movntqd_128(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{   
    const __m128i _stream_data = _mm_set_epi64x(0, entry.data);

    #ifdef ENABLE_DCOLLECTION
    if (is_sampling) {
        const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);

        if (do_lat_measurement) {
            const auto op_start = std::chrono::steady_clock::now();
            _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
            const auto op_end = std::chrono::steady_clock::now();

            cur_sample->write_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            cur_sample->write_inst_cycles_samples++;
        } else {
           _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
        }

        ++(cur_sample->num_movntqd);
    } else {
        _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
    }
    #else
        _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
    #endif
}

inline void write_movntps_128(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    const __m128d _stream_data = _mm_set_pd(0, entry.data);

    #ifdef ENABLE_DCOLLECTION
    if (is_sampling) {
        const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);

        if (do_lat_measurement) {
            const auto op_start = std::chrono::steady_clock::now();
            _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
            const auto op_end = std::chrono::steady_clock::now();

            cur_sample->write_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
            cur_sample->write_inst_cycles_samples++;
        } else {
           _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
        }

        ++(cur_sample->num_movntps);
    } else {
       _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
    }
    #else
        _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
    #endif
}

inline void flush_clflush(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
        if (is_sampling) {
            const bool do_lat_measurement = (((num_io++) & lat_sample_mask) == 0);

            if (do_lat_measurement) {
                const auto op_start = std::chrono::steady_clock::now();
                _mm_clflushopt(entry.dax_addr);
                const auto op_end = std::chrono::steady_clock::now();

                cur_sample->flush_inst_cycles += std::chrono::duration_cast<std::chrono::nanoseconds>(op_end - op_start).count();
                cur_sample->flush_inst_cycles_samples++;
            } else {
                _mm_clflushopt(entry.dax_addr);
            }

            cur_sample->num_flushes++;
        } else {
            _mm_clflushopt(entry.dax_addr);
        }
    #else
    _mm_clflushopt(entry.dax_addr);
    #endif
}

inline void barrier_mfence(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
        if (is_sampling) {
            _mm_mfence();
            ++(cur_sample->num_mfence);
        } else {
            _mm_mfence();
        }
    #else
    _mm_mfence();
    #endif
}

inline void barrier_sfence(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
        if (is_sampling) {
            _mm_sfence();
            ++(cur_sample->num_sfence);
        } else {
            _mm_sfence();
        }
    #else
    _mm_sfence();
    #endif
}

inline void barrier_lfence(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
        if (is_sampling) {
            _mm_lfence();
            ++(cur_sample->num_lfence);
        } else {
            _mm_lfence();
        }
    #else
    _mm_lfence();
    #endif
}
