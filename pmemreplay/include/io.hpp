#pragma once

#include <immintrin.h>
#include <emmintrin.h>

#include "bench_suite.hpp"

template<typename T>
inline void read_value(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    volatile T temp_var;

    #ifdef ENABLE_DCOLLECTION
    if (unlikely(is_sampling)) {
        const unsigned long long start_ticks = __builtin_ia32_rdtsc();
        temp_var = *(static_cast<T*>(entry.dax_addr));
        cur_sample->read_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
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
    if (unlikely(is_sampling)) {
        const unsigned long long start_ticks = __builtin_ia32_rdtsc();
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        *(static_cast<uint8_t*>(entry.dax_addr)) = static_cast<uint8_t>(entry.data);

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif

        cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
    } else {
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    }
    #else
        *(static_cast<unsigned char*>(entry.dax_addr)) = static_cast<unsigned char>(entry.data);
    #endif
}

inline void write_movnti_32(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
    if (unlikely(is_sampling)) {
        const unsigned long long start_ticks = __builtin_ia32_rdtsc();
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        //_mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);
        _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(entry.dax_addr));
        _mm_sfence();
        #endif

        cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
    } else {
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        //_mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);
        _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(entry.dax_addr));
        _mm_sfence();
        #endif
    }
    #else
        #ifdef STRICT_CONSISTENCY
            _mm_sfence();
            #endif

            //_mm_stream_pi((__m64*) entry.dax_addr, (__m64) entry.data);
            _mm_stream_si32(static_cast<int*>(entry.dax_addr), static_cast<int>(entry.data));

            #ifdef STRICT_CONSISTENCY
            _mm_clflushopt(static_cast<void*>(dev_addr));
            _mm_sfence();
            #endif
    #endif
}

inline void write_movntq_64(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    const __m64 v = _mm_set_pi64x(entry.data);

    #ifdef ENABLE_DCOLLECTION
    if (unlikely(is_sampling)) {
        const unsigned long long start_ticks = __builtin_ia32_rdtsc();
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
        

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif

        cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
    } else {
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
        

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    }
    #else
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_pi(static_cast<__m64*>(entry.dax_addr), v);
        
        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    #endif
}

inline void write_movntqd_128(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{   
    const __m128i _stream_data = _mm_set_epi64x(0, entry.data);

    #ifdef ENABLE_DCOLLECTION
    if (unlikely(is_sampling)) {
        const unsigned long long start_ticks = __builtin_ia32_rdtsc();
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
        

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif

        cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
    } else {
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
        

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    }
    #else
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_si128(static_cast<__m128i*>(entry.dax_addr), _stream_data);
        
        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    #endif
}

inline void write_movntps_128(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    const __m128d _stream_data = _mm_set_pd(0, entry.data);

    #ifdef ENABLE_DCOLLECTION
    if (unlikely(is_sampling)) {
        const unsigned long long start_ticks = __builtin_ia32_rdtsc();
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
        

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif

        cur_sample->write_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
    } else {
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

       _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
        

        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    }
    #else
        #ifdef STRICT_CONSISTENCY
        _mm_sfence();
        #endif

        _mm_stream_pd(static_cast<double*>(entry.dax_addr), _stream_data);
        
        #ifdef STRICT_CONSISTENCY
        _mm_clflushopt(static_cast<void*>(dev_addr));
        _mm_sfence();
        #endif
    #endif
}

inline void flush_clflush(const TraceEntry& entry, const bool is_sampling, struct io_sample *const cur_sample)
{
    #ifdef ENABLE_DCOLLECTION
        if (is_sampling) {
            unsigned long long start_ticks = __builtin_ia32_rdtsc();
            _mm_clflushopt(entry.dax_addr);
            cur_sample->flush_inst_cycles += (__builtin_ia32_rdtsc() - start_ticks);
            ++(cur_sample->num_flushes);
        } else {
            _mm_clflushopt(entry.dax_addr);
        }
    #else
    _mm_clflushopt(entry.dax_addr);
    #endif
}