// base128-ascii (The Unlicense)

#include <stdint.h>
#include <stddef.h>

#if defined(_MSC_VER) && !defined(__clang__)
#include <stdlib.h>
#include <string.h>
#include <intrin.h>
#ifndef __builtin_bswap64
#define __builtin_bswap64 _byteswap_uint64
#endif
#ifndef __builtin_memcpy
#define __builtin_memcpy memcpy
#endif
#ifndef __builtin_ctz
static inline unsigned int __msvc_ctz(unsigned int x) {
    unsigned long idx;
    if (_BitScanForward(&idx, x)) return (unsigned int)idx;
    return 32;
}
#define __builtin_ctz __msvc_ctz
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define WASM_EXPORT extern "C" __attribute__((visibility("default")))

extern "C" __attribute__((no_builtin("memcpy"))) void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n >= 8) {
        uint64_t v;
        __builtin_memcpy(&v, s, 8);
        __builtin_memcpy(d, &v, 8);
        d += 8;
        s += 8;
        n -= 8;
    }
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

extern "C" __attribute__((no_builtin("memset"))) void* memset(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    while (n--) {
        *d++ = (uint8_t)c;
    }
    return dest;
}
#else
#define WASM_EXPORT extern "C"
#endif

#if defined(__INTELLISENSE__) || (defined(_MSC_VER) && !defined(__clang__))
#ifndef __wasm_simd128__
#define __wasm_simd128__ 1
#endif
typedef struct { uint8_t b[16]; } v128_t;
static inline v128_t wasm_v128_load(const void* p) { return *(const v128_t*)p; }
static inline v128_t wasm_i8x16_splat(int8_t v) { (void)v; v128_t r{}; return r; }
static inline v128_t wasm_i8x16_eq(v128_t a, v128_t b) { (void)a; (void)b; v128_t r{}; return r; }
static inline v128_t wasm_v128_or(v128_t a, v128_t b) { (void)a; (void)b; v128_t r{}; return r; }
static inline int wasm_i8x16_bitmask(v128_t a) { (void)a; return 0; }
#else
#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif
#endif

// ============================================================================
// Base128 Encode / Decode (64-bit SWAR & SIMD)
// ============================================================================

static inline uint64_t ToBE64(uint64_t v) {
    return __builtin_bswap64(v);
}

static inline void Encode7to8_SWAR_FromVal(uint64_t in_val, uint8_t* dst) {
    uint64_t x = ToBE64(in_val) >> 8;
    x = ((x & 0x00FFFFFFF0000000ULL) << 4) | (x & 0x000000000FFFFFFFULL);
    x = ((x & 0x0FFFC0000FFFC000ULL) << 2) | (x & 0x00003FFF00003FFFULL);
    x = ((x & 0x3F803F803F803F80ULL) << 1) | (x & 0x007F007F007F007FULL);
    uint64_t out_val = ToBE64(x);
    __builtin_memcpy(dst, &out_val, 8);
}

WASM_EXPORT double base128wasm_encode(const uint8_t* in, size_t in_len, uint8_t* out) {
    if (in_len == 0) return 0.0;

    uint64_t in_len_64 = in_len;
    uint64_t full_chunks = in_len_64 / 7;
    uint64_t rem = in_len_64 - (full_chunks * 7);
    uint64_t out_len = in_len_64 + full_chunks + (rem != 0);

    uint64_t c = 0;
    for (; c + 8 < full_chunks; c += 8, in += 56, out += 64) {
        uint64_t in0, in1, in2, in3, in4, in5, in6, in7;
        __builtin_memcpy(&in0, in, 8);
        __builtin_memcpy(&in1, in + 7, 8);
        __builtin_memcpy(&in2, in + 14, 8);
        __builtin_memcpy(&in3, in + 21, 8);
        __builtin_memcpy(&in4, in + 28, 8);
        __builtin_memcpy(&in5, in + 35, 8);
        __builtin_memcpy(&in6, in + 42, 8);
        __builtin_memcpy(&in7, in + 49, 8);

        Encode7to8_SWAR_FromVal(in0, out);
        Encode7to8_SWAR_FromVal(in1, out + 8);
        Encode7to8_SWAR_FromVal(in2, out + 16);
        Encode7to8_SWAR_FromVal(in3, out + 24);
        Encode7to8_SWAR_FromVal(in4, out + 32);
        Encode7to8_SWAR_FromVal(in5, out + 40);
        Encode7to8_SWAR_FromVal(in6, out + 48);
        Encode7to8_SWAR_FromVal(in7, out + 56);
    }
    for (; c + 2 < full_chunks; c += 2, in += 14, out += 16) {
        uint64_t in0, in1;
        __builtin_memcpy(&in0, in, 8);
        __builtin_memcpy(&in1, in + 7, 8);

        Encode7to8_SWAR_FromVal(in0, out);
        Encode7to8_SWAR_FromVal(in1, out + 8);
    }
    for (; c + 1 < full_chunks; c++, in += 7, out += 8) {
        uint64_t in_val;
        __builtin_memcpy(&in_val, in, 8);
        Encode7to8_SWAR_FromVal(in_val, out);
    }
    if (c < full_chunks) {
        uint64_t in_val = 0;
        if (rem > 0) {
            __builtin_memcpy(&in_val, in, 8);
        } else {
            __builtin_memcpy(&in_val, in, 7);
        }
        Encode7to8_SWAR_FromVal(in_val, out);
        in += 7;
        out += 8;
    }

    if (rem) {
        uint8_t prev = in[0];
        out[0] = prev >> 1;
        for (uint64_t r = 1; r < rem; r++) {
            uint8_t curr = in[r];
            out[r] = 127 & ((prev << (7 - r)) | (curr >> (1 + r)));
            prev = curr;
        }
        out[rem] = 127 & (prev << (7 - rem));
    }

    return static_cast<double>(out_len);
}

static inline void Decode8to7_SWAR_FromVal(uint64_t v, uint8_t* out, bool can_write_8) {
    uint64_t x = ToBE64(v);
    x = ((x & 0x7F007F007F007F00ULL) >> 1) | (x & 0x007F007F007F007FULL);
    x = ((x & 0x3FFF00003FFF0000ULL) >> 2) | (x & 0x00003FFF00003FFFULL);
    uint64_t src_56 = ((x & 0x0FFFFFFF00000000ULL) >> 4) | (x & 0x000000000FFFFFFFULL);
    uint64_t out_val = ToBE64(src_56 << 8);
    __builtin_memcpy(out, &out_val, can_write_8 ? 8 : 7);
}

WASM_EXPORT double base128wasm_decode(const uint8_t* in, size_t in_len, uint8_t* out) {
    if (in_len == 0) return 0.0;

    uint64_t in_len_64 = in_len;
    uint64_t rem = in_len_64 & 7;
    if (rem == 1) return -2.0;

    uint64_t full_chunks = in_len_64 / 8;
    uint64_t out_len = in_len_64 - full_chunks - (rem != 0);

    uint64_t c = 0;
#if defined(__wasm_simd128__)
    for (; c + 8 < full_chunks; c += 8, in += 64, out += 56) {
        v128_t v0 = wasm_v128_load(in);
        v128_t v1 = wasm_v128_load(in + 16);
        v128_t v2 = wasm_v128_load(in + 32);
        v128_t v3 = wasm_v128_load(in + 48);
        v128_t any = wasm_v128_or(wasm_v128_or(v0, v1), wasm_v128_or(v2, v3));
        if (wasm_i8x16_bitmask(any) != 0) {
            return -1.0;
        }

        uint64_t d0, d1, d2, d3, d4, d5, d6, d7;
        __builtin_memcpy(&d0, in, 8);
        __builtin_memcpy(&d1, in + 8, 8);
        __builtin_memcpy(&d2, in + 16, 8);
        __builtin_memcpy(&d3, in + 24, 8);
        __builtin_memcpy(&d4, in + 32, 8);
        __builtin_memcpy(&d5, in + 40, 8);
        __builtin_memcpy(&d6, in + 48, 8);
        __builtin_memcpy(&d7, in + 56, 8);

        Decode8to7_SWAR_FromVal(d0, out, true);
        Decode8to7_SWAR_FromVal(d1, out + 7, true);
        Decode8to7_SWAR_FromVal(d2, out + 14, true);
        Decode8to7_SWAR_FromVal(d3, out + 21, true);
        Decode8to7_SWAR_FromVal(d4, out + 28, true);
        Decode8to7_SWAR_FromVal(d5, out + 35, true);
        Decode8to7_SWAR_FromVal(d6, out + 42, true);
        Decode8to7_SWAR_FromVal(d7, out + 49, true);
    }
#else
    for (; c + 4 < full_chunks; c += 4, in += 32, out += 28) {
        uint64_t d0, d1, d2, d3;
        __builtin_memcpy(&d0, in, 8);
        __builtin_memcpy(&d1, in + 8, 8);
        __builtin_memcpy(&d2, in + 16, 8);
        __builtin_memcpy(&d3, in + 24, 8);
        if ((d0 | d1 | d2 | d3) & 0x8080808080808080ULL) return -1.0;

        Decode8to7_SWAR_FromVal(d0, out, true);
        Decode8to7_SWAR_FromVal(d1, out + 7, true);
        Decode8to7_SWAR_FromVal(d2, out + 14, true);
        Decode8to7_SWAR_FromVal(d3, out + 21, true);
    }
#endif
    for (; c + 1 < full_chunks; c++, in += 8, out += 7) {
        uint64_t v;
        __builtin_memcpy(&v, in, 8);
        if (v & 0x8080808080808080ULL) return -1.0;
        Decode8to7_SWAR_FromVal(v, out, true);
    }
    if (c < full_chunks) {
        uint64_t v;
        __builtin_memcpy(&v, in, 8);
        if (v & 0x8080808080808080ULL) return -1.0;
        bool can_write_8 = (rem > 0);
        Decode8to7_SWAR_FromVal(v, out, can_write_8);
        in += 8;
        out += 7;
    }

    if (rem) {
        uint8_t c0 = in[0];
        if (c0 > 127) return -1.0;
        for (uint64_t r = 1; r < rem; r++) {
            uint8_t c1 = in[r];
            if (c1 > 127) return -1.0;
            out[r - 1] = (c0 << r) | (c1 >> (7 - r));
            c0 = c1;
        }
    }

    return static_cast<double>(out_len);
}

// ============================================================================
// toJSTemplateLiterals (EncodeResult)
// ============================================================================

template <typename T, size_t N>
struct Array {
    T data[N];
    constexpr const T& operator[](size_t idx) const { return data[idx]; }
    constexpr T& operator[](size_t idx) { return data[idx]; }
};

static constexpr auto kIsSpecialChar = []() constexpr {
    Array<bool, 256> table{};
    table['\0'] = true;
    table['\r'] = true;
    table['$']  = true;
    table['<']  = true;
    table['\\'] = true;
    table['`']  = true;
    return table;
}();

static inline bool IsCloseScriptTag(const uint8_t* src, uint64_t i, uint64_t len) {
    if (i + 7 >= len) return false;
    static const char kTarget[8] = {'<', '/', 's', 'c', 'r', 'i', 'p', 't'};
    static const char kMask[8]   = {0, 0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
    uint64_t target, mask;
    __builtin_memcpy(&target, kTarget, 8);
    __builtin_memcpy(&mask, kMask, 8);

    uint64_t val;
    __builtin_memcpy(&val, src + i, 8);
    return (val | mask) == target;
}

WASM_EXPORT double base128wasm_to_js_template_literals(const uint8_t* in, size_t in_len, uint8_t* out) {
    uint64_t in_len_64 = in_len;
    uint64_t out_len = 0;
    out[out_len++] = '`';

    uint64_t start = 0;
    uint64_t i = 0;

    auto emit_plain = [&](uint64_t pos) {
        uint64_t plain = pos - start;
        if (plain > 0) {
            __builtin_memcpy(out + out_len, in + start, plain);
            out_len += plain;
        }
    };

    auto handle_escape = [&](uint64_t pos, uint8_t c) {
        if (c == '$') {
            if (pos + 1 < in_len_64 && in[pos + 1] == '{') {
                emit_plain(pos);
                out[out_len++] = '\\';
                out[out_len++] = '$';
                out[out_len++] = '{';
                start = pos + 2;
            }
        } else if (c == '<') {
            if (IsCloseScriptTag(in, pos, in_len_64)) {
                emit_plain(pos);
                out[out_len++] = '<';
                out[out_len++] = '\\';
                out[out_len++] = '/';
                for (uint64_t k = 2; k <= 7; k++) {
                    out[out_len++] = in[pos + k];
                }
                start = pos + 8;
            }
        } else {
            emit_plain(pos);
            if (c == '\r') {
                out[out_len++] = '\\';
                out[out_len++] = 'r';
                start = pos + 1;
            } else if (c == '\\') {
                out[out_len++] = '\\';
                out[out_len++] = '\\';
                start = pos + 1;
            } else if (c == '`') {
                out[out_len++] = '\\';
                out[out_len++] = '`';
                start = pos + 1;
            } else /* c == '\0' */ {
                if (pos + 1 < in_len_64 && in[pos + 1] >= '0' && in[pos + 1] <= '9') {
                    out[out_len++] = '\\';
                    out[out_len++] = 'x';
                    out[out_len++] = '0';
                    out[out_len++] = '0';
                    out[out_len++] = in[pos + 1];
                    start = pos + 2;
                } else {
                    out[out_len++] = '\\';
                    out[out_len++] = '0';
                    start = pos + 1;
                }
            }
        }
    };

#if defined(__wasm_simd128__)
    const v128_t v0  = wasm_i8x16_splat(0);
    const v128_t v13 = wasm_i8x16_splat(13);
    const v128_t v36 = wasm_i8x16_splat(36);
    const v128_t v60 = wasm_i8x16_splat(60);
    const v128_t v92 = wasm_i8x16_splat(92);
    const v128_t v96 = wasm_i8x16_splat(96);

    while (i + 16 <= in_len_64) {
        v128_t v = wasm_v128_load(in + i);
        v128_t m0  = wasm_i8x16_eq(v, v0);
        v128_t m13 = wasm_i8x16_eq(v, v13);
        v128_t m36 = wasm_i8x16_eq(v, v36);
        v128_t m60 = wasm_i8x16_eq(v, v60);
        v128_t m92 = wasm_i8x16_eq(v, v92);
        v128_t m96 = wasm_i8x16_eq(v, v96);
        v128_t any = wasm_v128_or(wasm_v128_or(m0, m13),
                     wasm_v128_or(wasm_v128_or(m36, m60), wasm_v128_or(m92, m96)));
        int mask = wasm_i8x16_bitmask(any);
        while (mask != 0) {
            unsigned int offset = __builtin_ctz(mask);
            uint64_t pos = i + offset;
            if (pos >= start) {
                handle_escape(pos, in[pos]);
            }
            mask &= mask - 1;
        }
        i += 16;
    }
#endif

    while (i < in_len_64) {
        if (i >= start) {
            uint8_t c = in[i];
            if (kIsSpecialChar[c]) {
                handle_escape(i, c);
            }
        }
        i++;
    }

    emit_plain(in_len_64);
    out[out_len++] = '`';
    return static_cast<double>(out_len);
}

// ============================================================================
// parseJSTemplateLiterals
// ============================================================================

static inline uint64_t GetWhitespaceLengthUtf8(const uint8_t* input, uint64_t pos, uint64_t end) {
    uint8_t b0 = input[pos];
    if (b0 == ' ' || (b0 >= '\t' && b0 <= '\r')) return 1;
    if (b0 == 0xC2 && pos + 1 <= end && input[pos + 1] == 0xA0) return 2;
    if (pos + 2 <= end) {
        uint8_t b1 = input[pos + 1], b2 = input[pos + 2];
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) return 3;
        if (b0 == 0xE1 && b1 == 0x9A && b2 == 0x80) return 3;
        if (b0 == 0xE3 && b1 == 0x80 && b2 == 0x80) return 3;
        if (b0 == 0xE2) {
            if (b1 == 0x81 && b2 == 0x9F) return 3;
            if (b1 == 0x80) {
                if ((b2 >= 0x80 && b2 <= 0x8A) || b2 == 0xA8 || b2 == 0xA9 || b2 == 0xAF) return 3;
            }
        }
    }
    return 0;
}

static inline uint64_t GetTrailingWhitespaceLengthUtf8(const uint8_t* input, uint64_t start, uint64_t end) {
    uint8_t b = input[end];
    if (b == ' ' || (b >= '\t' && b <= '\r')) return 1;
    if (b == 0xA0 && end >= start + 1 && input[end - 1] == 0xC2) return 2;
    if (end >= start + 2) {
        uint8_t b0 = input[end - 2], b1 = input[end - 1];
        if (b0 == 0xEF && b1 == 0xBB && b == 0xBF) return 3;
        if (b0 == 0xE1 && b1 == 0x9A && b == 0x80) return 3;
        if (b0 == 0xE3 && b1 == 0x80 && b == 0x80) return 3;
        if (b0 == 0xE2) {
            if (b1 == 0x81 && b == 0x9F) return 3;
            if (b1 == 0x80) {
                if ((b >= 0x80 && b <= 0x8A) || b == 0xA8 || b == 0xA9 || b == 0xAF) return 3;
            }
        }
    }
    return 0;
}

static inline int HexVal(uint8_t c) {
    if (c >= '0' && c <= '9') return c - '0';
    c |= 32;
    if (c >= 'a' && c <= 'f') return c - ('a' - 10);
    return -1;
}

static inline uint64_t AppendCodePointUtf8(uint32_t cp, uint8_t* out) {
    if (cp <= 0x7F) {
        out[0] = (uint8_t)cp;
        return 1;
    }
    if (cp <= 0x7FF) {
        out[0] = (uint8_t)(0xC0 | (cp >> 6));
        out[1] = (uint8_t)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        out[0] = 0xEF;
        out[1] = 0xBF;
        out[2] = 0xBD;
        return 3;
    }
    if (cp <= 0xFFFF) {
        out[0] = (uint8_t)(0xE0 | (cp >> 12));
        out[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (cp & 0x3F));
        return 3;
    }
    if (cp <= 0x10FFFF) {
        out[0] = (uint8_t)(0xF0 | (cp >> 18));
        out[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (uint8_t)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

WASM_EXPORT double base128wasm_parse_js_template_literals(const uint8_t* in, size_t in_len, uint8_t* out) {
    uint64_t in_len_64 = in_len;
    if (in_len_64 < 2) return -1.0;

    uint64_t start = 0;
    uint64_t end = in_len_64 - 1;

    while (start <= end) {
        uint64_t ws = GetWhitespaceLengthUtf8(in, start, end);
        if (ws > 0) {
            start += ws;
            continue;
        }
        if (in[start] == '`') break;
        return -1.0;
    }

    while (end >= start) {
        uint64_t ws = GetTrailingWhitespaceLengthUtf8(in, start, end);
        if (ws > 0) {
            end -= ws;
            continue;
        }
        if (in[end] == '`') break;
        return -1.0;
    }

    if (start >= end) return -1.0;

    uint64_t i = start + 1;
    uint64_t out_len = 0;

#if defined(__wasm_simd128__)
    const v128_t v_slash  = wasm_i8x16_splat('\\');
    const v128_t v_grave  = wasm_i8x16_splat('`');
    const v128_t v_dollar = wasm_i8x16_splat('$');
    const v128_t v_cr     = wasm_i8x16_splat('\r');
#endif

    while (i < end) {
        uint64_t chunkStart = i;

#if defined(__wasm_simd128__)
        while (i + 16 <= end) {
            v128_t v = wasm_v128_load(in + i);
            v128_t m_slash  = wasm_i8x16_eq(v, v_slash);
            v128_t m_grave  = wasm_i8x16_eq(v, v_grave);
            v128_t m_dollar = wasm_i8x16_eq(v, v_dollar);
            v128_t m_cr     = wasm_i8x16_eq(v, v_cr);
            v128_t any = wasm_v128_or(wasm_v128_or(m_slash, m_grave),
                         wasm_v128_or(m_dollar, m_cr));
            int mask = wasm_i8x16_bitmask(any);
            if (mask != 0) {
                bool stop = false;
                while (mask != 0) {
                    unsigned int offset = __builtin_ctz(mask);
                    uint64_t pos = i + offset;
                    uint8_t c = in[pos];
                    if (c == '$') {
                        if (pos + 1 < end && in[pos + 1] == '{') {
                            i = pos;
                            stop = true;
                            break;
                        }
                    } else {
                        i = pos;
                        stop = true;
                        break;
                    }
                    mask &= mask - 1;
                }
                if (stop) break;
            }
            i += 16;
        }
#endif

        while (i < end) {
            uint8_t c = in[i];
            if (c == '\\' || c == '`' || c == '\r') break;
            if (c == '$' && i + 1 < end && in[i + 1] == '{') break;
            i++;
        }

        uint64_t chunkLen = i - chunkStart;
        if (chunkLen > 0) {
            __builtin_memcpy(out + out_len, in + chunkStart, chunkLen);
            out_len += chunkLen;
        }

        if (i >= end) break;

        uint8_t c = in[i];
        if (c == '`') {
            return -1.0;
        }
        if (c == '$') {
            return -1.0;
        }
        if (c == '\r') {
            out[out_len++] = '\n';
            i++;
            if (i < end && in[i] == '\n') i++;
            continue;
        }
        if (c == '\\') {
            i++;
            if (i >= end) return -1.0;
            uint8_t next = in[i++];
            switch (next) {
                case 'r': out[out_len++] = '\r'; break;
                case 'n': out[out_len++] = '\n'; break;
                case 't': out[out_len++] = '\t'; break;
                case 'b': out[out_len++] = '\b'; break;
                case 'f': out[out_len++] = '\f'; break;
                case 'v': out[out_len++] = '\v'; break;
                case '0':
                    if (i < end && in[i] >= '0' && in[i] <= '9') {
                        return -1.0;
                    }
                    out[out_len++] = '\0';
                    break;
                case 'x': {
                    if (i + 2 > end) return -1.0;
                    int h1 = HexVal(in[i++]);
                    int h2 = HexVal(in[i++]);
                    if (h1 == -1 || h2 == -1) return -1.0;
                    uint32_t val = (uint32_t)((h1 << 4) | h2);
                    out_len += AppendCodePointUtf8(val, out + out_len);
                    break;
                }
                case 'u': {
                    if (i < end && in[i] == '{') {
                        uint64_t startHex = ++i;
                        uint32_t cp = 0;
                        bool foundEnd = false;
                        for (; i < end; i++) {
                            uint8_t ch = in[i];
                            if (ch == '}') {
                                if (i == startHex) return -1.0;
                                foundEnd = true;
                                i++;
                                break;
                            }
                            int hv = HexVal(ch);
                            if (hv == -1) return -1.0;
                            cp = (cp << 4) | (uint32_t)hv;
                            if (cp > 0x10FFFF) return -1.0;
                        }
                        if (!foundEnd) return -1.0;
                        out_len += AppendCodePointUtf8(cp, out + out_len);
                        break;
                    }
                    if (i + 4 > end) return -1.0;
                    int h1 = HexVal(in[i++]);
                    int h2 = HexVal(in[i++]);
                    int h3 = HexVal(in[i++]);
                    int h4 = HexVal(in[i++]);
                    if (h1 == -1 || h2 == -1 || h3 == -1 || h4 == -1) return -1.0;
                    uint32_t val = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                    out_len += AppendCodePointUtf8(val, out + out_len);
                    break;
                }
                case '\r':
                    if (i < end && in[i] == '\n') i++;
                    break;
                case '\n':
                    break;
                case 0xE2:
                    if (i + 1 < end && in[i] == 0x80 && (in[i + 1] == 0xA8 || in[i + 1] == 0xA9)) {
                        i += 2;
                        break;
                    }
                    [[fallthrough]];
                default: {
                    if (next >= '1' && next <= '9') {
                        return -1.0;
                    }
                    out[out_len++] = next;
                    if (next >= 0xC0) {
                        int extra = 0;
                        if ((next & 0xE0) == 0xC0) extra = 1;
                        else if ((next & 0xF0) == 0xE0) extra = 2;
                        else if ((next & 0xF8) == 0xF0) extra = 3;
                        while (extra-- > 0 && i < end) {
                            out[out_len++] = in[i++];
                        }
                    }
                    break;
                }
            }
        }
    }

    return static_cast<double>(out_len);
}
