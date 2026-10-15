// base128-ascii (The Unlicense)

#ifndef NAPI_VERSION
#define NAPI_VERSION 10
#endif
#include <napi.h>
#ifdef USE_V8_ACCELERATION
#include <v8.h>
#include <v8-primitive.h>
#endif
#include <array>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <type_traits>


#if defined(_MSC_VER)
#include <intrin.h>
#endif
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#if !defined(_MSC_VER)
#include <cpuid.h>
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#elif (defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && defined(__ALTIVEC__)
#include <altivec.h>
#undef bool
#undef vector
#undef pixel
#endif


// ============================================================================
// 1. Common Types, Concepts & Utilities
// ============================================================================

struct AddonData {
    Napi::FunctionReference encodeResultConstructor;
    ~AddonData() {
        encodeResultConstructor.Reset();
    }
};

static void CleanupAddonData(Napi::Env /*env*/, AddonData* data) {
    delete data;
}

struct ParsedStringResult {
    void* buf = nullptr;
    size_t len = 0;
    bool is_one_byte = true;
    bool error = false;
    bool oom = false;
};

template <typename T>
concept AnyChar = std::is_same_v<T, char> ||
                  std::is_same_v<T, uint8_t> ||
                  std::is_same_v<T, char16_t>;

template <typename T>
concept UnsignedChar = std::is_same_v<T, uint8_t> ||
                       std::is_same_v<T, char16_t>;

static inline uint64_t BSwap64(uint64_t v) {
#if defined(_MSC_VER)
    return _byteswap_uint64(v);
#else
    return __builtin_bswap64(v);
#endif
}

static inline uint64_t ToBE64(uint64_t v) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    return v;
#else
    return BSwap64(v);
#endif
}

#if defined(__x86_64__) || defined(_M_X64)
static inline bool CpuHasFastBMI2() {
    static const bool has_fast_bmi2 = []() {
        int cpu_info[4] = {0};
#if defined(_MSC_VER)
        __cpuid(cpu_info, 0);
#else
        __cpuid(0, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
        int max_ids = cpu_info[0];
        if (max_ids < 7) return false;

        bool is_amd = (cpu_info[1] == 0x68747541 && cpu_info[3] == 0x69746e65 && cpu_info[2] == 0x444d4163); // "AuthenticAMD"

#if defined(_MSC_VER)
        __cpuidex(cpu_info, 7, 0);
#else
        __cpuid_count(7, 0, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#endif
        bool bmi2 = (cpu_info[1] & (1 << 8)) != 0;
        if (!bmi2) return false;

        if (is_amd) {
            int ver_info[4] = {0};
#if defined(_MSC_VER)
            __cpuid(ver_info, 1);
#else
            __cpuid(1, ver_info[0], ver_info[1], ver_info[2], ver_info[3]);
#endif
            int base = (ver_info[0] >> 8) & 0xF;
            int family = (base == 0xF) ? (base + ((ver_info[0] >> 20) & 0xFF)) : base;
            if (family < 0x19) return false; // Zen 1 and Zen 2 have slow microcoded PDEP/PEXT
        }
        return true;
    }();
    return has_fast_bmi2;
}

static inline bool IsUint8Array(const Napi::Value& val) {
    if (!val.IsTypedArray()) return false;
    napi_typedarray_type type = val.As<Napi::TypedArray>().TypedArrayType();
    return type == napi_uint8_array || type == napi_uint8_clamped_array;
}

static inline void Widen8To16(const uint8_t* src, char16_t* dst, size_t len) {
#if defined(__x86_64__) || defined(_M_X64)
    size_t i = 0;
    const __m128i zero = _mm_setzero_si128();
    for (; i + 32 <= len; i += 32) {
        __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i + 16));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), _mm_unpacklo_epi8(v0, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i + 8), _mm_unpackhi_epi8(v0, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i + 16), _mm_unpacklo_epi8(v1, zero));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i + 24), _mm_unpackhi_epi8(v1, zero));
    }
    for (; i < len; i++) {
        dst[i] = static_cast<char16_t>(src[i]);
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        uint16x8_t lo = vmovl_u8(vget_low_u8(v));
        uint16x8_t hi = vmovl_u8(vget_high_u8(v));
        vst1q_u16(reinterpret_cast<uint16_t*>(dst + i), lo);
        vst1q_u16(reinterpret_cast<uint16_t*>(dst + i + 8), hi);
    }
    for (; i < len; i++) {
        dst[i] = static_cast<char16_t>(src[i]);
    }
#elif (defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && defined(__ALTIVEC__)
    size_t i = 0;
    const __vector unsigned char zero = vec_splats(static_cast<unsigned char>(0));
    for (; i + 16 <= len; i += 16) {
        __vector unsigned char v = vec_xl(0, const_cast<unsigned char*>(src + i));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        __vector unsigned char lo = vec_mergeh(v, zero);
        __vector unsigned char hi = vec_mergel(v, zero);
#else
        __vector unsigned char lo = vec_mergeh(zero, v);
        __vector unsigned char hi = vec_mergel(zero, v);
#endif
        vec_xst(lo, 0, reinterpret_cast<unsigned char*>(dst + i));
        vec_xst(hi, 0, reinterpret_cast<unsigned char*>(dst + i + 8));
    }
    for (; i < len; i++) {
        dst[i] = static_cast<char16_t>(src[i]);
    }
#else
    for (size_t i = 0; i < len; i++) {
        dst[i] = static_cast<char16_t>(src[i]);
    }
#endif
}

static Napi::Value MakeLatin1String(napi_env env, char* buf, size_t len) {
#if NAPI_VERSION >= 10
    if (len >= 1024) {
        char* nb = static_cast<char*>(std::realloc(buf, len));
        if (nb) buf = nb;

        int64_t byte_size = static_cast<int64_t>(len);
        napi_adjust_external_memory(env, byte_size, nullptr);

        bool copied = false;
        napi_value res;
        napi_status status = node_api_create_external_string_latin1(
            env, buf, len,
            [](node_api_basic_env env, void* data, void* hint) {
                if (env && hint) {
                    int64_t bytes = static_cast<int64_t>(reinterpret_cast<uintptr_t>(hint));
                    napi_adjust_external_memory(env, -bytes, nullptr);
                }
                std::free(data);
            },
            reinterpret_cast<void*>(static_cast<uintptr_t>(byte_size)),
            &res, &copied);
        if (status == napi_ok) {
            // buf is already freed by finalizer when copied is true, do not double-free here
            return Napi::Value(env, res);
        }
        napi_adjust_external_memory(env, -byte_size, nullptr);
    }
#endif
    napi_value res;
    napi_status status = napi_create_string_latin1(env, buf, len, &res);
    std::free(buf);
    NAPI_THROW_IF_FAILED(env, status, Napi::Value());
    return Napi::Value(env, res);
}

static Napi::Value MakeUtf16String(napi_env env, char16_t* buf, size_t len) {
#if NAPI_VERSION >= 10
    if (len >= 512) {
        char16_t* nb = static_cast<char16_t*>(std::realloc(buf, len * sizeof(char16_t)));
        if (nb) buf = nb;

        int64_t byte_size = static_cast<int64_t>(len * sizeof(char16_t));
        napi_adjust_external_memory(env, byte_size, nullptr);

        bool copied = false;
        napi_value res;
        napi_status status = node_api_create_external_string_utf16(
            env, buf, len,
            [](node_api_basic_env env, void* data, void* hint) {
                if (env && hint) {
                    int64_t bytes = static_cast<int64_t>(reinterpret_cast<uintptr_t>(hint));
                    napi_adjust_external_memory(env, -bytes, nullptr);
                }
                std::free(data);
            },
            reinterpret_cast<void*>(static_cast<uintptr_t>(byte_size)),
            &res, &copied);
        if (status == napi_ok) {
            // buf is already freed by finalizer when copied is true, do not double-free here
            return Napi::Value(env, res);
        }
        napi_adjust_external_memory(env, -byte_size, nullptr);
    }
#endif
    napi_value res;
    napi_status status = napi_create_string_utf16(env, buf, len, &res);
    std::free(buf);
    NAPI_THROW_IF_FAILED(env, status, Napi::Value());
    return Napi::Value(env, res);
}


// ============================================================================
// 2. Base128 Encode
// ============================================================================


static inline void Encode7to8_SWAR(const uint8_t* src, uint8_t* dst, bool can_read_8) {
    uint64_t in_val = 0;
    std::memcpy(&in_val, src, can_read_8 ? 8 : 7);
    uint64_t x = ToBE64(in_val) >> 8; // 56 bits in low bits
    x = ((x & 0x00FFFFFFF0000000ULL) << 4) | (x & 0x000000000FFFFFFFULL);
    x = ((x & 0x0FFFC0000FFFC000ULL) << 2) | (x & 0x00003FFF00003FFFULL);
    x = ((x & 0x3F803F803F803F80ULL) << 1) | (x & 0x007F007F007F007FULL);
    uint64_t out_val = ToBE64(x);
    std::memcpy(dst, &out_val, 8);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("bmi2")))
#endif
static void EncodeLoop_BMI2(const uint8_t* src, uint8_t* dst, size_t full_chunks) {
    size_t c = 0;
    for (; c + 4 < full_chunks; c += 4, src += 28, dst += 32) {
        uint64_t in0, in1, in2, in3;
        std::memcpy(&in0, src, 8);
        std::memcpy(&in1, src + 7, 8);
        std::memcpy(&in2, src + 14, 8);
        std::memcpy(&in3, src + 21, 8);

        uint64_t out0 = BSwap64(_pdep_u64(BSwap64(in0) >> 8, 0x7F7F7F7F7F7F7F7FULL));
        uint64_t out1 = BSwap64(_pdep_u64(BSwap64(in1) >> 8, 0x7F7F7F7F7F7F7F7FULL));
        uint64_t out2 = BSwap64(_pdep_u64(BSwap64(in2) >> 8, 0x7F7F7F7F7F7F7F7FULL));
        uint64_t out3 = BSwap64(_pdep_u64(BSwap64(in3) >> 8, 0x7F7F7F7F7F7F7F7FULL));

        std::memcpy(dst, &out0, 8);
        std::memcpy(dst + 8, &out1, 8);
        std::memcpy(dst + 16, &out2, 8);
        std::memcpy(dst + 24, &out3, 8);
    }
    for (; c + 1 < full_chunks; c++, src += 7, dst += 8) {
        uint64_t in_val;
        std::memcpy(&in_val, src, 8);
        uint64_t src_56 = BSwap64(in_val) >> 8;
        uint64_t out_val = BSwap64(_pdep_u64(src_56, 0x7F7F7F7F7F7F7F7FULL));
        std::memcpy(dst, &out_val, 8);
    }
    if (c < full_chunks) {
        uint64_t in_val = 0;
        std::memcpy(&in_val, src, 7);
        uint64_t src_56 = BSwap64(in_val) >> 8;
        uint64_t out_val = BSwap64(_pdep_u64(src_56, 0x7F7F7F7F7F7F7F7FULL));
        std::memcpy(dst, &out_val, 8);
    }
}

static void EncodeLoop_SWAR(const uint8_t* src, uint8_t* dst, size_t full_chunks) {
    size_t c = 0;
    for (; c + 1 < full_chunks; c++, src += 7, dst += 8) {
        Encode7to8_SWAR(src, dst, true);
    }
    if (c < full_chunks) {
        Encode7to8_SWAR(src, dst, false);
    }
}

static Napi::Value Encode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !IsUint8Array(info[0])) {
        Napi::TypeError::New(env, "encode: input must be a Uint8Array or Uint8ClampedArray").ThrowAsJavaScriptException();
        return env.Null();
    }
    void* in_ptr = nullptr;
    size_t il = 0;
    napi_status status = napi_get_typedarray_info(env, info[0], nullptr, &il, &in_ptr, nullptr, nullptr);
    NAPI_THROW_IF_FAILED(env, status, env.Null());
    const uint8_t* in = static_cast<const uint8_t*>(in_ptr);

    size_t full_chunks = il / 7;
    size_t rem = il - (full_chunks * 7);
    size_t out_len = il + full_chunks + (rem != 0);

    Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, out_len);
    if (env.IsExceptionPending()) return env.Null();
    uint8_t* out = reinterpret_cast<uint8_t*>(ab.Data());
#if defined(__x86_64__) || defined(_M_X64)
    if (CpuHasFastBMI2()) {
        EncodeLoop_BMI2(in, out, full_chunks);
    } else
#endif
    {
        EncodeLoop_SWAR(in, out, full_chunks);
    }

    in += full_chunks * 7;
    out += full_chunks * 8;

    if (rem) {
        uint8_t b[7] = {0};
        std::memcpy(b, in, rem);
        uint8_t tmp[8];
        Encode7to8_SWAR(b, tmp, false);
        std::memcpy(out, tmp, rem + 1);
    }

    Napi::Uint8Array out_ta = Napi::Uint8Array::New(env, out_len, ab, 0);
    if (env.IsExceptionPending()) return env.Null();
    AddonData* addonData = env.GetInstanceData<AddonData>();
    return addonData->encodeResultConstructor.New({ out_ta });
}

// ============================================================================
// 3. Base128 Decode
// ============================================================================


static inline void Decode8to7_SWAR_FromVal(uint64_t v, uint8_t* out, bool can_write_8) {
    uint64_t x = ToBE64(v);
    x = ((x & 0x7F007F007F007F00ULL) >> 1) | (x & 0x007F007F007F007FULL);
    x = ((x & 0x3FFF00003FFF0000ULL) >> 2) | (x & 0x00003FFF00003FFFULL);
    uint64_t src_56 = ((x & 0x0FFFFFFF00000000ULL) >> 4) | (x & 0x000000000FFFFFFFULL);
    uint64_t out_val = ToBE64(src_56 << 8);
    std::memcpy(out, &out_val, can_write_8 ? 8 : 7);
}

static inline bool Decode8to7_SWAR(const uint8_t* in, uint8_t* out, bool can_write_8) {
    uint64_t v;
    std::memcpy(&v, in, 8);
    if (v & 0x8080808080808080ULL) return false;
    Decode8to7_SWAR_FromVal(v, out, can_write_8);
    return true;
}

static inline bool Pack8Char16To8Uint8(const char16_t* in, uint64_t& out_v) {
    uint64_t w0, w1;
    std::memcpy(&w0, in, 8);
    std::memcpy(&w1, in + 4, 8);
    if ((w0 | w1) & 0xFF80FF80FF80FF80ULL) return false;
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    auto pack4 = [](uint64_t w) -> uint32_t {
        w = (w | (w << 8)) & 0x7F7F00007F7F0000ULL;
        return static_cast<uint32_t>((w | (w << 16)) >> 32);
    };
    uint32_t p0 = pack4(w0);
    uint32_t p1 = pack4(w1);
    out_v = (static_cast<uint64_t>(p0) << 32) | p1;
#else
    auto pack4 = [](uint64_t w) -> uint32_t {
        w = (w | (w >> 8)) & 0x00007F7F00007F7FULL;
        return static_cast<uint32_t>(w | (w >> 16));
    };
    uint32_t p0 = pack4(w0);
    uint32_t p1 = pack4(w1);
    out_v = (static_cast<uint64_t>(p1) << 32) | p0;
#endif
    return true;
}

static inline bool Decode8to7_Char16(const char16_t* in, uint8_t* out, bool can_write_8) {
    uint64_t v;
    if (!Pack8Char16To8Uint8(in, v)) return false;
    Decode8to7_SWAR_FromVal(v, out, can_write_8);
    return true;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((target("bmi2")))
#endif
static bool DecodeLoop_BMI2(const uint8_t* in, uint8_t* out, size_t full_chunks) {
    size_t c = 0;
    for (; c + 4 < full_chunks; c += 4, in += 32, out += 28) {
        uint64_t v0, v1, v2, v3;
        std::memcpy(&v0, in, 8);
        std::memcpy(&v1, in + 8, 8);
        std::memcpy(&v2, in + 16, 8);
        std::memcpy(&v3, in + 24, 8);
        if ((v0 | v1 | v2 | v3) & 0x8080808080808080ULL) return false;

        uint64_t out0 = BSwap64(_pext_u64(BSwap64(v0), 0x7F7F7F7F7F7F7F7FULL) << 8);
        uint64_t out1 = BSwap64(_pext_u64(BSwap64(v1), 0x7F7F7F7F7F7F7F7FULL) << 8);
        uint64_t out2 = BSwap64(_pext_u64(BSwap64(v2), 0x7F7F7F7F7F7F7F7FULL) << 8);
        uint64_t out3 = BSwap64(_pext_u64(BSwap64(v3), 0x7F7F7F7F7F7F7F7FULL) << 8);

        std::memcpy(out, &out0, 8);
        std::memcpy(out + 7, &out1, 8);
        std::memcpy(out + 14, &out2, 8);
        std::memcpy(out + 21, &out3, 8);
    }
    for (; c + 1 < full_chunks; c++, in += 8, out += 7) {
        uint64_t v;
        std::memcpy(&v, in, 8);
        if (v & 0x8080808080808080ULL) return false;
        uint64_t src_56 = _pext_u64(BSwap64(v), 0x7F7F7F7F7F7F7F7FULL);
        uint64_t out_val = BSwap64(src_56 << 8);
        std::memcpy(out, &out_val, 8);
    }
    if (c < full_chunks) {
        uint64_t v;
        std::memcpy(&v, in, 8);
        if (v & 0x8080808080808080ULL) return false;
        uint64_t src_56 = _pext_u64(BSwap64(v), 0x7F7F7F7F7F7F7F7FULL);
        uint64_t out_val = BSwap64(src_56 << 8);
        std::memcpy(out, &out_val, 7);
    }
    return true;
}
#endif

static bool DecodeLoop_SWAR(const uint8_t* in, uint8_t* out, size_t full_chunks) {
    size_t c = 0;
    for (; c + 1 < full_chunks; c++, in += 8, out += 7) {
        if (!Decode8to7_SWAR(in, out, true)) return false;
    }
    if (c < full_chunks) {
        if (!Decode8to7_SWAR(in, out, false)) return false;
    }
    return true;
}

template <UnsignedChar CharT>
static inline bool DecodeImpl(const CharT* in, size_t full_chunks, size_t rem, uint8_t* out) {
    if constexpr (sizeof(CharT) == 1) {
#if defined(__x86_64__) || defined(_M_X64)
        if (CpuHasFastBMI2()) {
            if (!DecodeLoop_BMI2(reinterpret_cast<const uint8_t*>(in), out, full_chunks)) {
                return false;
            }
        } else
#endif
        {
            if (!DecodeLoop_SWAR(reinterpret_cast<const uint8_t*>(in), out, full_chunks)) {
                return false;
            }
        }
    } else {
        size_t c = 0;
        const char16_t* in16 = reinterpret_cast<const char16_t*>(in);
        uint8_t* out_curr = out;
        for (; c + 1 < full_chunks; c++, in16 += 8, out_curr += 7) {
            if (!Decode8to7_Char16(in16, out_curr, true)) return false;
        }
        if (c < full_chunks) {
            if (!Decode8to7_Char16(in16, out_curr, false)) return false;
        }
    }

    if (rem) {
        const CharT* in_rem = in + (full_chunks * 8);
        CharT c[8] = {0};
        std::memcpy(c, in_rem, rem * sizeof(CharT));
        uint8_t tmp[8];
        if constexpr (sizeof(CharT) == 1) {
            if (!Decode8to7_SWAR(reinterpret_cast<const uint8_t*>(c), tmp, false)) {
                return false;
            }
        } else {
            if (!Decode8to7_Char16(reinterpret_cast<const char16_t*>(c), tmp, false)) {
                return false;
            }
        }
        if (rem > 1) {
            uint8_t* out_rem = out + (full_chunks * 7);
            std::memcpy(out_rem, tmp, rem - 1);
        }
    }
    return true;
}

static Napi::Value Decode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "decode: input must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    napi_value str_val = info[0];
    size_t utf16_len = 0;
    size_t full_chunks;
    size_t rem;
    size_t out_len;

    auto updateOutLen = [&utf16_len, &full_chunks, &rem, &out_len]() {
        full_chunks = utf16_len / 8;
        rem = utf16_len & 7;
        out_len = utf16_len - full_chunks - (rem != 0);
    };

#ifdef USE_V8_ACCELERATION
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
        v8::Local<v8::Value> v8_val = *reinterpret_cast<v8::Local<v8::Value>*>(&str_val);
        if (v8_val->IsString()) {
            v8::Local<v8::String> v8_str = v8_val.As<v8::String>();
            utf16_len = v8_str->Length();
            updateOutLen();
            Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, out_len);
            if (env.IsExceptionPending()) return env.Null();
            uint8_t* out = reinterpret_cast<uint8_t*>(ab.Data());
            bool success = false;
            {
                v8::String::ValueView view(isolate, v8_str);
                if (view.is_one_byte()) {
                    success = DecodeImpl(reinterpret_cast<const uint8_t*>(view.data8()), full_chunks, rem, out);
                } else {
                    success = DecodeImpl(reinterpret_cast<const char16_t*>(view.data16()), full_chunks, rem, out);
                }
            }
            if (!success) {
                Napi::SyntaxError::New(env, "decode: invalid input").ThrowAsJavaScriptException();
                return env.Null();
            }
            return Napi::Uint8Array::New(env, out_len, ab, 0);
        }
    }
#endif

    napi_status status = napi_get_value_string_utf16(env, str_val, nullptr, 0, &utf16_len);
    NAPI_THROW_IF_FAILED(env, status, env.Null());

    size_t utf8_len = 0;
    status = napi_get_value_string_utf8(env, str_val, nullptr, 0, &utf8_len);
    NAPI_THROW_IF_FAILED(env, status, env.Null());

    if (utf8_len != utf16_len) {
        Napi::SyntaxError::New(env, "decode: invalid input").ThrowAsJavaScriptException();
        return env.Null();
    }

    updateOutLen();
    Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, out_len);
    if (env.IsExceptionPending()) return env.Null();
    uint8_t* out = reinterpret_cast<uint8_t*>(ab.Data());

    if (utf16_len == 0) {
        return Napi::Uint8Array::New(env, 0, ab, 0);
    }

    constexpr size_t kStackCap = 1024;
    char stack_buf[kStackCap];
    char* buf = stack_buf;
    std::string heap_buf;
    if (utf16_len + 1 > kStackCap) {
        heap_buf.resize(utf16_len + 1);
        buf = heap_buf.data();
    }
    size_t copied = 0;
    status = napi_get_value_string_latin1(env, str_val, buf, utf16_len + 1, &copied);
    NAPI_THROW_IF_FAILED(env, status, env.Null());
    bool success = DecodeImpl(reinterpret_cast<const uint8_t*>(buf), full_chunks, rem, out);
    if (!success) {
        Napi::SyntaxError::New(env, "decode: invalid input").ThrowAsJavaScriptException();
        return env.Null();
    }
    return Napi::Uint8Array::New(env, out_len, ab, 0);
}

// ============================================================================
// 4. EncodeResult & Template Literal Escaping
// ============================================================================


static constexpr auto kIsSpecialChar = []() constexpr {
    std::array<bool, 256> table{};
    table['\0'] = true;
    table['\r'] = true;
    table['$']  = true;
    table['<']  = true;
    table['\\'] = true;
    table['`']  = true;
    return table;
}();
template <AnyChar CharT>
static inline bool IsCloseScriptTag(const CharT* src, size_t i, size_t len) {
    if (i + 7 >= len) return false;
    if constexpr (sizeof(CharT) == 1) {
        static const char kTarget[8] = {'<', '/', 's', 'c', 'r', 'i', 'p', 't'};
        static const char kMask[8]   = {0, 0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
        uint64_t target, mask;
        std::memcpy(&target, kTarget, 8);
        std::memcpy(&mask, kMask, 8);

        uint64_t val;
        std::memcpy(&val, src + i, 8);
        return (val | mask) == target;
    } else {
        static const char16_t kTarget0[4] = {u'<', u'/', u's', u'c'};
        static const char16_t kMask0[4]   = {0, 0, 0x20, 0x20};
        static const char16_t kTarget1[4] = {u'r', u'i', u'p', u't'};
        static constexpr uint64_t kMask1  = 0x0020002000200020ULL;

        uint64_t target0, mask0, target1;
        std::memcpy(&target0, kTarget0, 8);
        std::memcpy(&mask0, kMask0, 8);
        std::memcpy(&target1, kTarget1, 8);

        uint64_t val0;
        std::memcpy(&val0, src + i, 8);
        if ((val0 | mask0) != target0) return false;

        uint64_t val1;
        std::memcpy(&val1, src + i + 4, 8);
        return (val1 | kMask1) == target1;
    }
}

static inline char* EscapeBytesToTemplateLiterals(const char* src, size_t len, size_t* out_len) {
    size_t cap = len + (len / 4) + 128;
    char* out_buf = static_cast<char*>(std::malloc(cap));
    if (!out_buf) return nullptr;

    char* dst = out_buf;
    *dst++ = '`';

    size_t start = 0;
    size_t i = 0;

    auto emit_plain = [&](size_t pos) -> bool {
        size_t plain = pos - start;
        if (plain > 0) {
            if (static_cast<size_t>(dst - out_buf) + plain + 32 > cap) {
                size_t off = dst - out_buf;
                cap = cap * 2 + plain + 128;
                char* nb = static_cast<char*>(std::realloc(out_buf, cap));
                if (!nb) { std::free(out_buf); return false; }
                out_buf = nb;
                dst = out_buf + off;
            }
            std::memcpy(dst, src + start, plain);
            dst += plain;
        }
        return true;
    };

    auto handle_escape = [&](size_t pos, char c) -> bool {
        if (c == '$') {
            if (pos + 1 < len && src[pos + 1] == '{') {
                if (!emit_plain(pos)) return false;
                std::memcpy(dst, "\\${", 3);
                dst += 3;
                start = pos + 2;
            }
        } else if (c == '<') {
            if (IsCloseScriptTag(src, pos, len)) {
                if (!emit_plain(pos)) return false;
                std::memcpy(dst, "<\\/", 3);
                std::memcpy(dst + 3, src + pos + 2, 6);
                dst += 9;
                start = pos + 8;
            }
        } else {
            if (!emit_plain(pos)) return false;
            if (c == '\r') {
                std::memcpy(dst, "\\r", 2);
                dst += 2;
                start = pos + 1;
            } else if (c == '\\') {
                std::memcpy(dst, "\\\\", 2);
                dst += 2;
                start = pos + 1;
            } else if (c == '`') {
                std::memcpy(dst, "\\`", 2);
                dst += 2;
                start = pos + 1;
            } else /* c == '\0' */ {
                if (pos + 1 < len && src[pos + 1] >= '0' && src[pos + 1] <= '9') {
                    std::memcpy(dst, "\\x00", 4);
                    dst[4] = src[pos + 1];
                    dst += 5;
                    start = pos + 2;
                } else {
                    std::memcpy(dst, "\\0", 2);
                    dst += 2;
                    start = pos + 1;
                }
            }
        }
        return true;
    };

#if defined(__x86_64__) || defined(_M_X64)
    const __m128i v0  = _mm_set1_epi8(0);
    const __m128i v13 = _mm_set1_epi8(13);
    const __m128i v36 = _mm_set1_epi8(36);
    const __m128i v60 = _mm_set1_epi8(60);
    const __m128i v92 = _mm_set1_epi8(92);
    const __m128i v96 = _mm_set1_epi8(96);

    while (i + 16 <= len) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        __m128i m0  = _mm_cmpeq_epi8(v, v0);
        __m128i m13 = _mm_cmpeq_epi8(v, v13);
        __m128i m36 = _mm_cmpeq_epi8(v, v36);
        __m128i m60 = _mm_cmpeq_epi8(v, v60);
        __m128i m92 = _mm_cmpeq_epi8(v, v92);
        __m128i m96 = _mm_cmpeq_epi8(v, v96);
        __m128i any = _mm_or_si128(_mm_or_si128(m0, m13),
                      _mm_or_si128(_mm_or_si128(m36, m60), _mm_or_si128(m92, m96)));
        int mask = _mm_movemask_epi8(any);
        while (mask != 0) {
#if defined(_MSC_VER)
            unsigned long offset;
            _BitScanForward(&offset, mask);
#else
            unsigned long offset = __builtin_ctz(mask);
#endif
            size_t pos = i + offset;
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            mask &= mask - 1;
        }
        i += 16;
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    const uint8x16_t v0  = vdupq_n_u8(0);
    const uint8x16_t v13 = vdupq_n_u8(13);
    const uint8x16_t v36 = vdupq_n_u8(36);
    const uint8x16_t v60 = vdupq_n_u8(60);
    const uint8x16_t v92 = vdupq_n_u8(92);
    const uint8x16_t v96 = vdupq_n_u8(96);

    while (i + 16 <= len) {
        uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(src + i));
        uint8x16_t m0  = vceqq_u8(v, v0);
        uint8x16_t m13 = vceqq_u8(v, v13);
        uint8x16_t m36 = vceqq_u8(v, v36);
        uint8x16_t m60 = vceqq_u8(v, v60);
        uint8x16_t m92 = vceqq_u8(v, v92);
        uint8x16_t m96 = vceqq_u8(v, v96);
        uint8x16_t any = vorrq_u8(vorrq_u8(m0, m13),
                         vorrq_u8(vorrq_u8(m36, m60), vorrq_u8(m92, m96)));
        uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(any), 0);
        uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(any), 1);
        while (lo != 0) {
#if defined(_MSC_VER)
            unsigned long offset;
            _BitScanForward64(&offset, lo);
#else
            unsigned long offset = __builtin_ctzll(lo);
#endif
            size_t pos = i + (offset >> 3);
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            lo &= ~(0xFFULL << (offset & ~7ULL));
        }
        while (hi != 0) {
#if defined(_MSC_VER)
            unsigned long offset;
            _BitScanForward64(&offset, hi);
#else
            unsigned long offset = __builtin_ctzll(hi);
#endif
            size_t pos = i + 8 + (offset >> 3);
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            hi &= ~(0xFFULL << (offset & ~7ULL));
        }
        i += 16;
    }
#elif (defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && defined(__ALTIVEC__)
    const __vector unsigned char v0  = vec_splats(static_cast<unsigned char>(0));
    const __vector unsigned char v13 = vec_splats(static_cast<unsigned char>(13));
    const __vector unsigned char v36 = vec_splats(static_cast<unsigned char>(36));
    const __vector unsigned char v60 = vec_splats(static_cast<unsigned char>(60));
    const __vector unsigned char v92 = vec_splats(static_cast<unsigned char>(92));
    const __vector unsigned char v96 = vec_splats(static_cast<unsigned char>(96));

    while (i + 16 <= len) {
        __vector unsigned char v = vec_xl(0, reinterpret_cast<const unsigned char*>(src + i));
        auto m0  = vec_cmpeq(v, v0);
        auto m13 = vec_cmpeq(v, v13);
        auto m36 = vec_cmpeq(v, v36);
        auto m60 = vec_cmpeq(v, v60);
        auto m92 = vec_cmpeq(v, v92);
        auto m96 = vec_cmpeq(v, v96);
        auto any = vec_or(vec_or(m0, m13),
                         vec_or(vec_or(m36, m60), vec_or(m92, m96)));
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        uint64_t lo = vec_extract(((__vector unsigned long long)any), 0);
        uint64_t hi = vec_extract(((__vector unsigned long long)any), 1);
        while (lo != 0) {
            unsigned long offset = __builtin_ctzll(lo);
            size_t pos = i + (offset >> 3);
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            lo &= ~(0xFFULL << (offset & ~7ULL));
        }
        while (hi != 0) {
            unsigned long offset = __builtin_ctzll(hi);
            size_t pos = i + 8 + (offset >> 3);
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            hi &= ~(0xFFULL << (offset & ~7ULL));
        }
#else
        uint64_t hi = vec_extract(((__vector unsigned long long)any), 0);
        uint64_t lo = vec_extract(((__vector unsigned long long)any), 1);
        while (hi != 0) {
            unsigned long offset = __builtin_clzll(hi);
            size_t pos = i + (offset >> 3);
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            hi &= ~(0xFF00000000000000ULL >> (offset & ~7ULL));
        }
        while (lo != 0) {
            unsigned long offset = __builtin_clzll(lo);
            size_t pos = i + 8 + (offset >> 3);
            if (pos >= start) {
                if (!handle_escape(pos, src[pos])) return nullptr;
            }
            lo &= ~(0xFF00000000000000ULL >> (offset & ~7ULL));
        }
#endif
        i += 16;
    }
#endif

    while (i < len) {
        if (i >= start) {
            char c = src[i];
            if (kIsSpecialChar[static_cast<uint8_t>(c)]) {
                if (!handle_escape(i, c)) return nullptr;
            }
        }
        i++;
    }

    if (!emit_plain(len)) return nullptr;

    if (static_cast<size_t>(dst - out_buf) + 2 > cap) {
        size_t offset = dst - out_buf;
        cap += 16;
        char* nb = static_cast<char*>(std::realloc(out_buf, cap));
        if (!nb) { std::free(out_buf); return nullptr; }
        out_buf = nb;
        dst = out_buf + offset;
    }
    *dst++ = '`';
    *out_len = dst - out_buf;
    return out_buf;
}

template <UnsignedChar CharT>
static inline void EscapeToTemplateLiterals(const CharT* src, size_t len, ParsedStringResult& res) {
    if constexpr (sizeof(CharT) == 1) {
        size_t out_len = 0;
        char* out_buf = EscapeBytesToTemplateLiterals(reinterpret_cast<const char*>(src), len, &out_len);
        res.buf = out_buf;
        res.len = out_len;
        res.is_one_byte = true;
        res.error = (out_buf == nullptr);
        res.oom = (out_buf == nullptr);
        return;
    }

    res.error = true;
    res.oom = false;
    res.buf = nullptr;
    res.len = 0;
    res.is_one_byte = true;

    size_t cap = len + (len / 4) + 128;
    char* buf8 = static_cast<char*>(std::malloc(cap));
    if (!buf8) {
        res.oom = true;
        return;
    }

    char16_t* buf16 = nullptr;
    bool is_one_byte = true;
    size_t out_len = 0;

    auto switchToTwoByte = [&]() -> bool {
        buf16 = static_cast<char16_t*>(std::malloc(cap * sizeof(char16_t)));
        if (!buf16) {
            res.oom = true;
            std::free(buf8);
            buf8 = nullptr;
            return false;
        }
        Widen8To16(reinterpret_cast<const uint8_t*>(buf8), buf16, out_len);
        std::free(buf8);
        buf8 = nullptr;
        is_one_byte = false;
        return true;
    };

    auto ensureCap = [&](size_t needed) -> bool {
        if (out_len + needed > cap) {
            cap = cap * 2 + needed + 128;
            if (is_one_byte) {
                char* nb = static_cast<char*>(std::realloc(buf8, cap));
                if (!nb) {
                    res.oom = true;
                    std::free(buf8);
                    buf8 = nullptr;
                    return false;
                }
                buf8 = nb;
            } else {
                char16_t* nb = static_cast<char16_t*>(std::realloc(buf16, cap * sizeof(char16_t)));
                if (!nb) {
                    res.oom = true;
                    std::free(buf16);
                    buf16 = nullptr;
                    return false;
                }
                buf16 = nb;
            }
        }
        return true;
    };

    buf8[out_len++] = '`';
    size_t i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len) {
            CharT c = src[i];
            if (is_one_byte) {
                if (static_cast<uint16_t>(c) > 255) break;
                if (kIsSpecialChar[static_cast<uint8_t>(c)]) {
                    if (c == '$') {
                        if (i + 1 < len && src[i + 1] == '{') break;
                    } else if (c == '<') {
                        if (IsCloseScriptTag(src, i, len)) break;
                    } else {
                        break;
                    }
                }
            } else {
                if (c <= 255 && kIsSpecialChar[static_cast<uint8_t>(c)]) {
                    if (c == '$') {
                        if (i + 1 < len && src[i + 1] == '{') break;
                    } else if (c == '<') {
                        if (IsCloseScriptTag(src, i, len)) break;
                    } else {
                        break;
                    }
                }
            }
            i++;
        }

        size_t chunk_len = i - start;
        if (chunk_len > 0) {
            if (!ensureCap(chunk_len + 32)) return;
            if (is_one_byte) {
                if constexpr (sizeof(CharT) == 1) {
                    std::memcpy(buf8 + out_len, src + start, chunk_len);
                } else {
                    for (size_t k = 0; k < chunk_len; k++) {
                        buf8[out_len + k] = static_cast<char>(src[start + k]);
                    }
                }
            } else {
                std::memcpy(buf16 + out_len, src + start, chunk_len * sizeof(char16_t));
            }
            out_len += chunk_len;
        }
        if (i >= len) break;

        if (is_one_byte && static_cast<uint16_t>(src[i]) > 255) {
            if (!switchToTwoByte()) return;
            if (!ensureCap(16)) return;
            buf16[out_len++] = src[i];
            i++;
            continue;
        }

        if (!ensureCap(16)) return;
        CharT c = src[i];
        if (is_one_byte) {
            switch (c) {
                case '\r':
                    buf8[out_len++] = '\\'; buf8[out_len++] = 'r'; i++; break;
                case '\\':
                    buf8[out_len++] = '\\'; buf8[out_len++] = '\\'; i++; break;
                case '`':
                    buf8[out_len++] = '\\'; buf8[out_len++] = '`'; i++; break;
                case '\0':
                    if (i + 1 < len && src[i + 1] >= '0' && src[i + 1] <= '9') {
                        buf8[out_len++] = '\\'; buf8[out_len++] = 'x';
                        buf8[out_len++] = '0';  buf8[out_len++] = '0';
                        buf8[out_len++] = static_cast<char>(src[i + 1]);
                        i += 2;
                    } else {
                        buf8[out_len++] = '\\'; buf8[out_len++] = '0'; i++;
                    }
                    break;
                case '$':
                    if (i + 1 < len && src[i + 1] == '{') {
                        buf8[out_len++] = '\\'; buf8[out_len++] = '$'; buf8[out_len++] = '{';
                        i += 2;
                    } else {
                        buf8[out_len++] = '$'; i++;
                    }
                    break;
                case '<':
                    if (IsCloseScriptTag(src, i, len)) {
                        buf8[out_len++] = '<';
                        buf8[out_len++] = '\\';
                        buf8[out_len++] = '/';
                        for (size_t k = 2; k <= 7; k++) {
                            buf8[out_len++] = static_cast<char>(src[i + k]);
                        }
                        i += 8;
                    } else {
                        buf8[out_len++] = '<'; i++;
                    }
                    break;
                default:
                    buf8[out_len++] = static_cast<char>(c); i++; break;
            }
        } else {
            switch (c) {
                case '\r':
                    buf16[out_len++] = u'\\'; buf16[out_len++] = u'r'; i++; break;
                case '\\':
                    buf16[out_len++] = u'\\'; buf16[out_len++] = u'\\'; i++; break;
                case '`':
                    buf16[out_len++] = u'\\'; buf16[out_len++] = u'`'; i++; break;
                case '\0':
                    if (i + 1 < len && src[i + 1] >= '0' && src[i + 1] <= '9') {
                        buf16[out_len++] = u'\\'; buf16[out_len++] = u'x';
                        buf16[out_len++] = u'0';  buf16[out_len++] = u'0';
                        buf16[out_len++] = static_cast<char16_t>(src[i + 1]);
                        i += 2;
                    } else {
                        buf16[out_len++] = u'\\'; buf16[out_len++] = u'0'; i++;
                    }
                    break;
                case '$':
                    if (i + 1 < len && src[i + 1] == '{') {
                        buf16[out_len++] = u'\\'; buf16[out_len++] = u'$'; buf16[out_len++] = u'{';
                        i += 2;
                    } else {
                        buf16[out_len++] = u'$'; i++;
                    }
                    break;
                case '<':
                    if (IsCloseScriptTag(src, i, len)) {
                        buf16[out_len++] = u'<';
                        buf16[out_len++] = u'\\';
                        buf16[out_len++] = u'/';
                        for (size_t k = 2; k <= 7; k++) {
                            buf16[out_len++] = static_cast<char16_t>(src[i + k]);
                        }
                        i += 8;
                    } else {
                        buf16[out_len++] = u'<'; i++;
                    }
                    break;
                default:
                    buf16[out_len++] = static_cast<char16_t>(c); i++; break;
            }
        }
    }

    if (!ensureCap(2)) return;
    if (is_one_byte) {
        buf8[out_len++] = '`';
        res.buf = buf8;
    } else {
        buf16[out_len++] = u'`';
        res.buf = buf16;
    }
    res.len = out_len;
    res.is_one_byte = is_one_byte;
    res.error = false;
}

static Napi::Value EncodeResult_Constructor(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (!info.IsConstructCall()) {
        Napi::TypeError::New(env, "Class constructor EncodeResult cannot be invoked without 'new'")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::Object self = info.This().As<Napi::Object>();
    self.Set("bytes", info[0]);
    return self;
}

static Napi::Value EncodeResult_ToString(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Value self = info.This();
    if (!self.IsObject()) {
        Napi::TypeError::New(env, "EncodeResult.prototype.toString: 'this' must be an object")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::Object obj = self.As<Napi::Object>();
    Napi::Value bytesVal = obj.Get("bytes");
    if (env.IsExceptionPending()) return env.Null();
    if (!IsUint8Array(bytesVal)) {
        Napi::TypeError::New(env, "EncodeResult.prototype.toString: 'this.bytes' must be a Uint8Array or Uint8ClampedArray")
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    void* data_ptr = nullptr;
    size_t len = 0;
    napi_status status = napi_get_typedarray_info(
        env, bytesVal, nullptr, &len, &data_ptr, nullptr, nullptr);
    NAPI_THROW_IF_FAILED(env, status, env.Null());
    const char* data = static_cast<const char*>(data_ptr ? data_ptr : "");
    napi_value res;
    status = napi_create_string_latin1(env, data, len, &res);
    NAPI_THROW_IF_FAILED(env, status, Napi::Value());
    return Napi::Value(env, res);
}

static Napi::Value EncodeResult_ToJSTemplateLiterals(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Value self = info.This();

    if (self.IsObject()) {
        Napi::Value bytesVal = self.As<Napi::Object>().Get("bytes");
        if (env.IsExceptionPending()) return env.Null();
        if (IsUint8Array(bytesVal)) {
            // Read this.bytes directly to accelerate string construction (guaranteed ASCII Base128)
            void* data_ptr = nullptr;
            size_t len = 0;
            napi_status status = napi_get_typedarray_info(
                env, bytesVal, nullptr, &len, &data_ptr, nullptr, nullptr);
            if (status == napi_ok) {
                const char* data = static_cast<const char*>(data_ptr ? data_ptr : "");
                size_t out_len = 0;
                char* out_buf = EscapeBytesToTemplateLiterals(data, len, &out_len);
                if (!out_buf) {
                    Napi::Error::New(env, "Out of memory").ThrowAsJavaScriptException();
                    return env.Null();
                }
                return MakeLatin1String(env, out_buf, out_len);
            }
        }
    }

    // String path: read uniformly
    Napi::String strVal;
    if (self.IsString()) {
        strVal = self.As<Napi::String>();
    } else {
        if (self.IsUndefined()) {
            Napi::TypeError::New(env, "Cannot read properties of undefined (reading 'toString')")
                .ThrowAsJavaScriptException();
            return env.Null();
        }
        if (self.IsNull()) {
            Napi::TypeError::New(env, "Cannot read properties of null (reading 'toString')")
                .ThrowAsJavaScriptException();
            return env.Null();
        }
        strVal = self.ToString();
        if (env.IsExceptionPending()) {
            return env.Null();
        }
    }

    ParsedStringResult res;

#ifdef USE_V8_ACCELERATION
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
        napi_value str_handle = strVal;
        v8::Local<v8::Value> v8_val = *reinterpret_cast<v8::Local<v8::Value>*>(&str_handle);
        if (v8_val->IsString()) {
            v8::Local<v8::String> v8_str = v8_val.As<v8::String>();
            v8::String::ValueView view(isolate, v8_str);
            if (view.is_one_byte()) {
                EscapeToTemplateLiterals(reinterpret_cast<const uint8_t*>(view.data8()), view.length(), res);
            } else {
                EscapeToTemplateLiterals(reinterpret_cast<const char16_t*>(view.data16()), view.length(), res);
            }
            goto done;
        }
    }
#endif

    {
        napi_value str_handle = strVal;
        size_t utf16_len = 0;
        napi_status status = napi_get_value_string_utf16(env, str_handle, nullptr, 0, &utf16_len);
        NAPI_THROW_IF_FAILED(env, status, env.Null());

        size_t utf8_len = 0;
        status = napi_get_value_string_utf8(env, str_handle, nullptr, 0, &utf8_len);
        NAPI_THROW_IF_FAILED(env, status, env.Null());

        if (utf8_len == utf16_len) {
            constexpr size_t kStackCap = 1024;
            char stack_buf[kStackCap];
            char* buf = stack_buf;
            std::string heap_buf;
            if (utf16_len + 1 > kStackCap) {
                heap_buf.resize(utf16_len + 1);
                buf = heap_buf.data();
            }
            size_t copied = 0;
            status = napi_get_value_string_latin1(env, str_handle, buf, utf16_len + 1, &copied);
            NAPI_THROW_IF_FAILED(env, status, env.Null());
            EscapeToTemplateLiterals(reinterpret_cast<const uint8_t*>(buf), utf16_len, res);
        } else {
            constexpr size_t kStackCap16 = 512;
            char16_t stack_buf16[kStackCap16];
            char16_t* buf16 = stack_buf16;
            std::u16string heap_buf16;
            if (utf16_len + 1 > kStackCap16) {
                heap_buf16.resize(utf16_len + 1);
                buf16 = heap_buf16.data();
            }
            size_t copied = 0;
            status = napi_get_value_string_utf16(env, str_handle, buf16, utf16_len + 1, &copied);
            NAPI_THROW_IF_FAILED(env, status, env.Null());
            EscapeToTemplateLiterals(reinterpret_cast<const char16_t*>(buf16), utf16_len, res);
        }
    }

#ifdef USE_V8_ACCELERATION
done:
#endif
    if (res.oom) {
        Napi::Error::New(env, "Out of memory").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (res.error) return env.Null();
    if (res.is_one_byte) {
        return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
    }
    return MakeUtf16String(env, static_cast<char16_t*>(res.buf), res.len);
}


// ============================================================================
// 5. parseJSTemplateLiterals
// ============================================================================

template <UnsignedChar CharT>
static inline bool IsTemplateLiteralWhitespace(CharT c) {
    if (c == ' ' || (c >= '\t' && c <= '\r') || c == 0xA0) {
        return true;
    }
    if constexpr (sizeof(CharT) > 1) {
        if (c == 0xFEFF || c == 0x2028 || c == 0x2029 || c == 0x1680 ||
            (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000) {
            return true;
        }
    }
    return false;
}


template <AnyChar CharT>
static inline int HexVal(CharT c) {
    if (c >= '0' && c <= '9') return c - '0';
    c |= 32;
    if (c >= 'a' && c <= 'f') return c - ('a' - 10);
    return -1;
}


static inline void FastCopyBytes(char* dst, const char* src, size_t len) {
    if (len <= 8) {
        if (len >= 4) {
            uint32_t a, b;
            std::memcpy(&a, src, 4);
            std::memcpy(&b, src + len - 4, 4);
            std::memcpy(dst, &a, 4);
            std::memcpy(dst + len - 4, &b, 4);
        } else if (len > 0) {
            dst[0] = src[0];
            dst[len / 2] = src[len / 2];
            dst[len - 1] = src[len - 1];
        }
        return;
    }
    if (len <= 16) {
        uint64_t a, b;
        std::memcpy(&a, src, 8);
        std::memcpy(&b, src + len - 8, 8);
        std::memcpy(dst, &a, 8);
        std::memcpy(dst + len - 8, &b, 8);
        return;
    }
    if (len <= 32) {
#if defined(__x86_64__) || defined(_M_X64)
        __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
        __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + len - 16));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), a);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + len - 16), b);
#elif defined(__aarch64__) || defined(_M_ARM64)
        uint8x16_t a = vld1q_u8(reinterpret_cast<const uint8_t*>(src));
        uint8x16_t b = vld1q_u8(reinterpret_cast<const uint8_t*>(src + len - 16));
        vst1q_u8(reinterpret_cast<uint8_t*>(dst), a);
        vst1q_u8(reinterpret_cast<uint8_t*>(dst + len - 16), b);
#elif (defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && defined(__ALTIVEC__)
        __vector unsigned char a = vec_xl(0, reinterpret_cast<const unsigned char*>(src));
        __vector unsigned char b = vec_xl(0, reinterpret_cast<const unsigned char*>(src + len - 16));
        vec_xst(a, 0, reinterpret_cast<unsigned char*>(dst));
        vec_xst(b, 0, reinterpret_cast<unsigned char*>(dst + len - 16));
#else
        std::memcpy(dst, src, len);
#endif
        return;
    }
    std::memcpy(dst, src, len);
}

static constexpr auto kSimpleEscapeTable = []() constexpr {
    std::array<char, 256> table{};
    table['r'] = '\r';
    table['n'] = '\n';
    table['t'] = '\t';
    table['b'] = '\b';
    table['f'] = '\f';
    table['v'] = '\v';
    table['\\'] = '\\';
    table['`'] = '`';
    table['\''] = '\'';
    table['"'] = '"';
    table['$'] = '$';
    return table;
}();

template <UnsignedChar CharT>
static void ParseJSTemplateLiteralsImpl(const CharT* input, size_t len, ParsedStringResult& res) {
    res.error = true;
    res.oom = false;
    res.buf = nullptr;
    res.len = 0;

    if (len < 2) return;

    size_t start = 0;
    size_t end = len - 1;

    for (;; start++) {
        if (start == end) return;
        CharT c = input[start];
        if (c == '`') break;
        if (!IsTemplateLiteralWhitespace(c)) {
            return;
        }
    }

    for (;; end--) {
        if (end == start) return;
        CharT c = input[end];
        if (c == '`') break;
        if (!IsTemplateLiteralWhitespace(c)) {
            return;
        }
    }

    size_t i = start + 1;
    size_t cap = end - start;

    char* buf8 = static_cast<char*>(std::malloc(cap));
    if (!buf8) {
        res.oom = true;
        return;
    }
    char16_t* buf16 = nullptr;
    bool is_one_byte = true;
    size_t out_len = 0;

    auto cleanup = [&]() {
        if (is_one_byte) {
            std::free(buf8);
            buf8 = nullptr;
        } else {
            std::free(buf16);
            buf16 = nullptr;
        }
    };

    auto switchToTwoByte = [&]() -> bool {
        buf16 = static_cast<char16_t*>(std::malloc(cap * sizeof(char16_t)));
        if (!buf16) {
            res.oom = true;
            cleanup();
            return false;
        }
        Widen8To16(reinterpret_cast<const uint8_t*>(buf8), buf16, out_len);
        std::free(buf8);
        buf8 = nullptr;
        is_one_byte = false;
        return true;
    };

    auto pushChar = [&](char16_t ch) -> bool {
        if (is_one_byte) {
            if (ch <= 255) {
                buf8[out_len++] = static_cast<char>(ch);
                return true;
            }
            if (!switchToTwoByte()) return false;
        }
        buf16[out_len++] = ch;
        return true;
    };

#if defined(__x86_64__) || defined(_M_X64)
    const __m128i v_slash  = _mm_set1_epi8('\\');
    const __m128i v_grave  = _mm_set1_epi8('`');
    const __m128i v_dollar = _mm_set1_epi8('$');
    const __m128i v_cr     = _mm_set1_epi8('\r');
#elif defined(__aarch64__) || defined(_M_ARM64)
    const uint8x16_t v_slash  = vdupq_n_u8('\\');
    const uint8x16_t v_grave  = vdupq_n_u8('`');
    const uint8x16_t v_dollar = vdupq_n_u8('$');
    const uint8x16_t v_cr     = vdupq_n_u8('\r');
#elif (defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && defined(__ALTIVEC__)
    const __vector unsigned char v_slash  = vec_splats(static_cast<unsigned char>('\\'));
    const __vector unsigned char v_grave  = vec_splats(static_cast<unsigned char>('`'));
    const __vector unsigned char v_dollar = vec_splats(static_cast<unsigned char>('$'));
    const __vector unsigned char v_cr     = vec_splats(static_cast<unsigned char>('\r'));
    const __vector unsigned char zero     = vec_splats(static_cast<unsigned char>(0));
#endif

    while (i < end) {
        size_t chunkStart = i;

        if constexpr (sizeof(CharT) == 1) {
#if defined(__x86_64__) || defined(_M_X64)
            while (i + 16 <= end) {
                __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + i));
                __m128i m_slash  = _mm_cmpeq_epi8(v, v_slash);
                __m128i m_grave  = _mm_cmpeq_epi8(v, v_grave);
                __m128i m_dollar = _mm_cmpeq_epi8(v, v_dollar);
                __m128i m_cr     = _mm_cmpeq_epi8(v, v_cr);
                __m128i any = _mm_or_si128(_mm_or_si128(m_slash, m_grave), _mm_or_si128(m_dollar, m_cr));
                int mask = _mm_movemask_epi8(any);
                if (mask != 0) {
                    bool stop = false;
                    while (mask != 0) {
#if defined(_MSC_VER)
                        unsigned long offset;
                        _BitScanForward(&offset, mask);
#else
                        unsigned long offset = __builtin_ctz(mask);
#endif
                        size_t pos = i + offset;
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
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
                    if (stop) goto found_escape;
                }
                i += 16;
            }
#elif defined(__aarch64__) || defined(_M_ARM64)
            while (i + 16 <= end) {
                uint8x16_t v = vld1q_u8(reinterpret_cast<const uint8_t*>(input + i));
                uint8x16_t m_slash  = vceqq_u8(v, v_slash);
                uint8x16_t m_grave  = vceqq_u8(v, v_grave);
                uint8x16_t m_dollar = vceqq_u8(v, v_dollar);
                uint8x16_t m_cr     = vceqq_u8(v, v_cr);
                uint8x16_t any = vorrq_u8(vorrq_u8(m_slash, m_grave), vorrq_u8(m_dollar, m_cr));
                uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(any), 0);
                uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(any), 1);
                if ((lo | hi) != 0) {
                    bool stop = false;
                    while (lo != 0) {
#if defined(_MSC_VER)
                        unsigned long offset;
                        _BitScanForward64(&offset, lo);
#else
                        unsigned long offset = __builtin_ctzll(lo);
#endif
                        size_t pos = i + (offset >> 3);
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
                                i = pos;
                                stop = true;
                                break;
                            }
                        } else {
                            i = pos;
                            stop = true;
                            break;
                        }
                        lo &= ~(0xFFULL << (offset & ~7ULL));
                    }
                    if (stop) goto found_escape;

                    while (hi != 0) {
#if defined(_MSC_VER)
                        unsigned long offset;
                        _BitScanForward64(&offset, hi);
#else
                        unsigned long offset = __builtin_ctzll(hi);
#endif
                        size_t pos = i + 8 + (offset >> 3);
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
                                i = pos;
                                stop = true;
                                break;
                            }
                        } else {
                            i = pos;
                            stop = true;
                            break;
                        }
                        hi &= ~(0xFFULL << (offset & ~7ULL));
                    }
                    if (stop) goto found_escape;
                }
                i += 16;
            }
#elif (defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)) && defined(__ALTIVEC__)
            while (i + 16 <= end) {
                __vector unsigned char v = vec_xl(0, reinterpret_cast<const unsigned char*>(input + i));
                auto m_slash  = vec_cmpeq(v, v_slash);
                auto m_grave  = vec_cmpeq(v, v_grave);
                auto m_dollar = vec_cmpeq(v, v_dollar);
                auto m_cr     = vec_cmpeq(v, v_cr);
                auto any = vec_or(vec_or(m_slash, m_grave), vec_or(m_dollar, m_cr));
                if (vec_any_ne(((__vector unsigned char)any), zero)) {
                    bool stop = false;
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
                    uint64_t lo = vec_extract(((__vector unsigned long long)any), 0);
                    uint64_t hi = vec_extract(((__vector unsigned long long)any), 1);
                    while (lo != 0) {
                        unsigned long offset = __builtin_ctzll(lo);
                        size_t pos = i + (offset >> 3);
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
                                i = pos; stop = true; break;
                            }
                        } else {
                            i = pos; stop = true; break;
                        }
                        lo &= ~(0xFFULL << (offset & ~7ULL));
                    }
                    if (stop) goto found_escape;

                    while (hi != 0) {
                        unsigned long offset = __builtin_ctzll(hi);
                        size_t pos = i + 8 + (offset >> 3);
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
                                i = pos; stop = true; break;
                            }
                        } else {
                            i = pos; stop = true; break;
                        }
                        hi &= ~(0xFFULL << (offset & ~7ULL));
                    }
                    if (stop) goto found_escape;
#else
                    uint64_t hi = vec_extract(((__vector unsigned long long)any), 0);
                    uint64_t lo = vec_extract(((__vector unsigned long long)any), 1);
                    while (hi != 0) {
                        unsigned long offset = __builtin_clzll(hi);
                        size_t pos = i + (offset >> 3);
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
                                i = pos; stop = true; break;
                            }
                        } else {
                            i = pos; stop = true; break;
                        }
                        hi &= ~(0xFF00000000000000ULL >> (offset & ~7ULL));
                    }
                    if (stop) goto found_escape;

                    while (lo != 0) {
                        unsigned long offset = __builtin_clzll(lo);
                        size_t pos = i + 8 + (offset >> 3);
                        CharT c = input[pos];
                        if (c == '$') {
                            if (pos + 1 < end && input[pos + 1] == '{') {
                                i = pos; stop = true; break;
                            }
                        } else {
                            i = pos; stop = true; break;
                        }
                        lo &= ~(0xFF00000000000000ULL >> (offset & ~7ULL));
                    }
                    if (stop) goto found_escape;
#endif
                }
                i += 16;
            }
#endif
        }

        while (i < end) {
            CharT c = input[i];
            if (c == '\\' || c == '`' || c == '\r') break;
            if (c == '$' && i + 1 < end && input[i + 1] == '{') break;
            i++;
        }

found_escape:
        if (i > chunkStart) {
            size_t chunkLen = i - chunkStart;
            if (is_one_byte) {
                if constexpr (sizeof(CharT) == 1) {
                    FastCopyBytes(buf8 + out_len, reinterpret_cast<const char*>(input + chunkStart), chunkLen);
                } else {
                    bool has_wide = false;
                    for (size_t k = 0; k < chunkLen; k++) {
                        if (static_cast<uint16_t>(input[chunkStart + k]) > 255) {
                            has_wide = true;
                            break;
                        }
                    }
                    if (has_wide) {
                        if (!switchToTwoByte()) return;
                        std::memcpy(buf16 + out_len, input + chunkStart, chunkLen * sizeof(char16_t));
                    } else {
                        for (size_t k = 0; k < chunkLen; k++) {
                            buf8[out_len + k] = static_cast<char>(input[chunkStart + k]);
                        }
                    }
                }
            } else {
                if constexpr (sizeof(CharT) == 1) {
                    Widen8To16(reinterpret_cast<const uint8_t*>(input + chunkStart), buf16 + out_len, chunkLen);
                } else {
                    std::memcpy(buf16 + out_len, input + chunkStart, chunkLen * sizeof(char16_t));
                }
            }
            out_len += chunkLen;
        }
        if (i >= end) break;

        CharT c = input[i];
        if (c == '\\') {
            i++;
            if (i >= end) {
                cleanup();
                return;
            }
            uint16_t next = static_cast<uint16_t>(static_cast<std::make_unsigned_t<CharT>>(input[i]));
            if (is_one_byte) {
                if (next < 256) {
                    char esc = kSimpleEscapeTable[static_cast<uint8_t>(next)];
                    if (esc != 0) {
                        buf8[out_len++] = esc;
                        i++;
                        continue;
                    }
                    if (next == '0') {
                        if (i + 1 < end && input[i + 1] >= '0' && input[i + 1] <= '9') {
                            cleanup();
                            return;
                        }
                        buf8[out_len++] = '\0';
                        i++;
                        continue;
                    }
                }
                switch (next) {
                    case 'x': {
                        if (i + 2 >= end) { cleanup(); return; }
                        int h1 = HexVal(input[i + 1]);
                        if (h1 == -1) { cleanup(); return; }
                        int h2 = HexVal(input[i + 2]);
                        if (h2 == -1) { cleanup(); return; }
                        buf8[out_len++] = static_cast<char>((h1 << 4) | h2);
                        i += 3;
                        break;
                    }
                    case 'u': {
                        if (i + 1 < end && input[i + 1] == '{') {
                            size_t startHex = i + 2;
                            uint32_t cp = 0;
                            size_t k = startHex;
                            for (; k < end; k++) {
                                CharT ch = input[k];
                                if (ch == '}') {
                                    if (k == startHex) {
                                        cleanup();
                                        return;
                                    }
                                    if (cp <= 0xFF) {
                                        buf8[out_len++] = static_cast<char>(cp);
                                    } else if (cp <= 0xFFFF) {
                                        if (!switchToTwoByte()) return;
                                        buf16[out_len++] = static_cast<char16_t>(cp);
                                    } else {
                                        cp -= 0x10000;
                                        if (!switchToTwoByte()) return;
                                        buf16[out_len++] = static_cast<char16_t>(0xD800 + (cp >> 10));
                                        buf16[out_len++] = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
                                    }
                                    i = k + 1;
                                    break;
                                }
                                int hv = HexVal(ch);
                                if (hv == -1) {
                                    cleanup();
                                    return;
                                }
                                cp = (cp << 4) | hv;
                                if (cp > 0x10FFFF) {
                                    cleanup();
                                    return;
                                }
                            }
                            if (k >= end) {
                                cleanup();
                                return;
                            }
                            break;
                        }
                        if (i + 4 >= end) { cleanup(); return; }
                        int h1 = HexVal(input[i + 1]);
                        if (h1 == -1) { cleanup(); return; }
                        int h2 = HexVal(input[i + 2]);
                        if (h2 == -1) { cleanup(); return; }
                        int h3 = HexVal(input[i + 3]);
                        if (h3 == -1) { cleanup(); return; }
                        int h4 = HexVal(input[i + 4]);
                        if (h4 == -1) { cleanup(); return; }
                        uint32_t cp = (h1 << 12) | (h2 << 8) | (h3 << 4) | h4;
                        if (cp <= 0xFF) {
                            buf8[out_len++] = static_cast<char>(cp);
                        } else {
                            if (!switchToTwoByte()) return;
                            buf16[out_len++] = static_cast<char16_t>(cp);
                        }
                        i += 5;
                        break;
                    }
                    case '\r': {
                        if (++i < end && input[i] == '\n') i++;
                        break;
                    }
                    case '\n':
                    case 0x2028:
                    case 0x2029: {
                        i++;
                        break;
                    }
                    default: {
                        if (next >= '1' && next <= '9') {
                            cleanup();
                            return;
                        }
                        if (next <= 255) {
                            buf8[out_len++] = static_cast<char>(next);
                        } else {
                            if (!switchToTwoByte()) return;
                            buf16[out_len++] = static_cast<char16_t>(next);
                        }
                        i++;
                        break;
                    }
                }
            } else {
                if (next < 256) {
                    char esc = kSimpleEscapeTable[static_cast<uint8_t>(next)];
                    if (esc != 0) {
                        buf16[out_len++] = static_cast<char16_t>(static_cast<uint8_t>(esc));
                        i++;
                        continue;
                    }
                    if (next == '0') {
                        if (i + 1 < end && input[i + 1] >= '0' && input[i + 1] <= '9') {
                            cleanup();
                            return;
                        }
                        buf16[out_len++] = u'\0';
                        i++;
                        continue;
                    }
                }
                switch (next) {
                    case 'x': {
                        if (i + 2 >= end) { cleanup(); return; }
                        int h1 = HexVal(input[i + 1]);
                        if (h1 == -1) { cleanup(); return; }
                        int h2 = HexVal(input[i + 2]);
                        if (h2 == -1) { cleanup(); return; }
                        buf16[out_len++] = static_cast<char16_t>((h1 << 4) | h2);
                        i += 3;
                        break;
                    }
                    case 'u': {
                        if (i + 1 < end && input[i + 1] == '{') {
                            size_t startHex = i + 2;
                            uint32_t cp = 0;
                            size_t k = startHex;
                            for (; k < end; k++) {
                                CharT ch = input[k];
                                if (ch == '}') {
                                    if (k == startHex) {
                                        cleanup();
                                        return;
                                    }
                                    if (cp <= 0xFFFF) {
                                        buf16[out_len++] = static_cast<char16_t>(cp);
                                    } else {
                                        cp -= 0x10000;
                                        buf16[out_len++] = static_cast<char16_t>(0xD800 + (cp >> 10));
                                        buf16[out_len++] = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
                                    }
                                    i = k + 1;
                                    break;
                                }
                                int hv = HexVal(ch);
                                if (hv == -1) {
                                    cleanup();
                                    return;
                                }
                                cp = (cp << 4) | hv;
                                if (cp > 0x10FFFF) {
                                    cleanup();
                                    return;
                                }
                            }
                            if (k >= end) {
                                cleanup();
                                return;
                            }
                            break;
                        }
                        if (i + 4 >= end) { cleanup(); return; }
                        int h1 = HexVal(input[i + 1]);
                        if (h1 == -1) { cleanup(); return; }
                        int h2 = HexVal(input[i + 2]);
                        if (h2 == -1) { cleanup(); return; }
                        int h3 = HexVal(input[i + 3]);
                        if (h3 == -1) { cleanup(); return; }
                        int h4 = HexVal(input[i + 4]);
                        if (h4 == -1) { cleanup(); return; }
                        buf16[out_len++] = static_cast<char16_t>((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                        i += 5;
                        break;
                    }
                    case '\r': {
                        if (++i < end && input[i] == '\n') i++;
                        break;
                    }
                    case '\n':
                    case 0x2028:
                    case 0x2029: {
                        i++;
                        break;
                    }
                    default: {
                        if (next >= '1' && next <= '9') {
                            cleanup();
                            return;
                        }
                        buf16[out_len++] = static_cast<char16_t>(next);
                        i++;
                        break;
                    }
                }
            }
        } else if (c == '\r') {
            if (i + 1 < end && input[i + 1] == '\n') {
                i++;
            }
            if (is_one_byte) {
                buf8[out_len++] = '\n';
            } else {
                buf16[out_len++] = u'\n';
            }
            i++;
        } else {
            // c is '`' or '$' followed by '{'
            cleanup();
            return;
        }
    }

    res.buf = is_one_byte ? static_cast<void*>(buf8) : static_cast<void*>(buf16);
    res.len = out_len;
    res.is_one_byte = is_one_byte;
    res.error = false;
    res.oom = false;
}

static Napi::Value ParseJSTemplateLiterals(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "parseJSTemplateLiterals: input must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    auto ThrowSyntaxError = [&env]() {
        Napi::SyntaxError::New(env, "parseJSTemplateLiterals: invalid input").ThrowAsJavaScriptException();
        return env.Null();
    };

    ParsedStringResult res;
    napi_value str_val = info[0];

#ifdef USE_V8_ACCELERATION
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
        v8::Local<v8::Value> v8_val = *reinterpret_cast<v8::Local<v8::Value>*>(&str_val);
        if (v8_val->IsString()) {
            v8::Local<v8::String> v8_str = v8_val.As<v8::String>();
            v8::String::ValueView view(isolate, v8_str);
            if (view.is_one_byte()) {
                ParseJSTemplateLiteralsImpl(reinterpret_cast<const uint8_t*>(view.data8()), view.length(), res);
            } else {
                ParseJSTemplateLiteralsImpl(reinterpret_cast<const char16_t*>(view.data16()), view.length(), res);
            }
            goto done;
        }
    }
#endif

    {
        size_t utf16_len = 0;
        napi_status status = napi_get_value_string_utf16(env, str_val, nullptr, 0, &utf16_len);
        NAPI_THROW_IF_FAILED(env, status, env.Null());

        size_t utf8_len = 0;
        status = napi_get_value_string_utf8(env, str_val, nullptr, 0, &utf8_len);
        NAPI_THROW_IF_FAILED(env, status, env.Null());

        if (utf8_len == utf16_len) {
            constexpr size_t kStackCap = 1024;
            char stack_buf[kStackCap];
            char* buf = stack_buf;
            std::string heap_buf;
            if (utf16_len + 1 > kStackCap) {
                heap_buf.resize(utf16_len + 1);
                buf = heap_buf.data();
            }
            size_t copied = 0;
            status = napi_get_value_string_latin1(env, str_val, buf, utf16_len + 1, &copied);
            NAPI_THROW_IF_FAILED(env, status, env.Null());
            ParseJSTemplateLiteralsImpl(reinterpret_cast<const uint8_t*>(buf), utf16_len, res);
        } else {
            constexpr size_t kStackCap16 = 512;
            char16_t stack_buf16[kStackCap16];
            char16_t* buf16 = stack_buf16;
            std::u16string heap_buf16;
            if (utf16_len + 1 > kStackCap16) {
                heap_buf16.resize(utf16_len + 1);
                buf16 = heap_buf16.data();
            }
            size_t copied = 0;
            status = napi_get_value_string_utf16(env, str_val, buf16, utf16_len + 1, &copied);
            NAPI_THROW_IF_FAILED(env, status, env.Null());
            ParseJSTemplateLiteralsImpl(reinterpret_cast<const char16_t*>(buf16), utf16_len, res);
        }
    }

#ifdef USE_V8_ACCELERATION
done:
#endif
    if (res.oom) {
        Napi::Error::New(env, "Out of memory").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (res.error) {
        return ThrowSyntaxError();
    }
    if (res.is_one_byte) {
        return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
    }
    return MakeUtf16String(env, static_cast<char16_t*>(res.buf), res.len);
}

// ============================================================================
// 6. V8 Feature Detection & Module Initialization
// ============================================================================


#ifdef USE_V8_ACCELERATION
#if defined(_MSC_VER)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static bool TestV8Internal(v8::Isolate* isolate, napi_value str) {
    v8::Local<v8::Value>* v8_val = reinterpret_cast<v8::Local<v8::Value>*>(&str);
    if (!(*v8_val)->IsString()) return false;

    v8::Local<v8::String> v8_str = (*v8_val).As<v8::String>();
    v8::String::ValueView view(isolate, v8_str);
    return view.is_one_byte() && view.length() == 1 && view.data8() != nullptr && view.data8()[0] == '`';
}

#if defined(_MSC_VER)
static bool SafeCallTestV8(v8::Isolate* isolate, napi_value str) {
    __try {
        return TestV8Internal(isolate, str);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#endif

static bool SafeTestV8(napi_env env) {
#if !defined(_MSC_VER)
    if (!dlsym(RTLD_DEFAULT, "_ZN2v87Isolate10GetCurrentEv") ||
        (!dlsym(RTLD_DEFAULT, "_ZN2v86String9ValueViewC1EPNS_7IsolateENS_5LocalIS0_EE") &&
         !dlsym(RTLD_DEFAULT, "_ZN2v86String9ValueViewC2EPNS_7IsolateENS_5LocalIS0_EE"))) {
        return false;
    }
#endif
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (!isolate) return false;

    napi_value str = nullptr;
    if (napi_create_string_utf8(env, "`", 1, &str) != napi_ok || !str) return false;

#if defined(_MSC_VER)
    return SafeCallTestV8(isolate, str);
#else
    return TestV8Internal(isolate, str);
#endif
}
#endif

Napi::Object Init(Napi::Env env, Napi::Object exports) {
#ifdef USE_V8_ACCELERATION
    if (!SafeTestV8(env)) {
        Napi::Error::New(env, "V8 acceleration is not supported in current runtime").ThrowAsJavaScriptException();
        return Napi::Object();
    }
#endif

    Napi::Function ctor = Napi::Function::New(env, EncodeResult_Constructor, "EncodeResult");
    Napi::Object proto = ctor.Get("prototype").As<Napi::Object>();
    proto.Set("toString", Napi::Function::New(env, EncodeResult_ToString, "toString"));
    proto.Set("toJSTemplateLiterals", Napi::Function::New(env, EncodeResult_ToJSTemplateLiterals, "toJSTemplateLiterals"));

    AddonData* addonData = new AddonData();
    addonData->encodeResultConstructor = Napi::Persistent(ctor);
    env.SetInstanceData<AddonData, CleanupAddonData>(addonData);

    Napi::Object defaultObj = Napi::Object::New(env);

    auto set = [&](const char* name, Napi::Value val) {
        exports.Set(name, val);
        defaultObj.Set(name, val);
    };

    set("EncodeResult", ctor);
    set("encode", Napi::Function::New(env, Encode, "encode"));
    set("decode", Napi::Function::New(env, Decode, "decode"));
    set("parseJSTemplateLiterals", Napi::Function::New(env, ParseJSTemplateLiterals, "parseJSTemplateLiterals"));

#ifdef USE_V8_ACCELERATION
    set("_impl", Napi::String::New(env, "v8"));
#else
    set("_impl", Napi::String::New(env, "napi"));
#endif

    exports.Set("default", defaultObj);

    return exports;
}

NODE_API_MODULE(base128, Init)
