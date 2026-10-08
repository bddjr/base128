#ifndef NAPI_VERSION
#define NAPI_VERSION 10
#endif
#include <napi.h>
#ifdef USE_V8_ACCELERATION
#include <v8.h>
#include <v8-primitive.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cctype>

static Napi::FunctionReference encodeResultConstructor;

static const bool kIsSpecialChar[256] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, // 0: \0, 13: \r
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 36: $
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // 60: <
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // 92: backslash
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 96: `
};

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#endif

static Napi::Value MakeLatin1String(napi_env env, char* buf, size_t len) {
    if (len >= 1024) {
#if NAPI_VERSION >= 10
        bool copied = false;
        napi_value res;
        napi_status status = node_api_create_external_string_latin1(
            env, buf, len,
            [](node_api_basic_env, void* data, void*) {
                std::free(data);
            },
            nullptr, &res, &copied);
        if (status == napi_ok) {
            return Napi::Value(env, res);
        }
#endif
    }
    napi_value res;
    napi_status status = napi_create_string_latin1(env, buf, len, &res);
    if (status == napi_ok) {
        std::free(buf);
        return Napi::Value(env, res);
    }
    Napi::Value fallback = Napi::String::New(env, buf, len);
    std::free(buf);
    return fallback;
}

static Napi::Value MakeUtf16String(napi_env env, char16_t* buf, size_t len) {
    if (len >= 1024) {
#if NAPI_VERSION >= 10
        bool copied = false;
        napi_value res;
        napi_status status = node_api_create_external_string_utf16(
            env, buf, len,
            [](node_api_basic_env, void* data, void*) {
                std::free(data);
            },
            nullptr, &res, &copied);
        if (status == napi_ok) {
            return Napi::Value(env, res);
        }
#endif
    }
    napi_value res;
    napi_status status = napi_create_string_utf16(env, buf, len, &res);
    if (status == napi_ok) {
        std::free(buf);
        return Napi::Value(env, res);
    }
    Napi::Value fallback = Napi::String::New(env, buf, len);
    std::free(buf);
    return fallback;
}

template <typename CharT>
static inline CharT* EscapeToTemplateLiterals(const CharT* src, size_t len, size_t* out_len) {
    size_t cap = len + (len / 4) + 128;
    CharT* out_buf = static_cast<CharT*>(std::malloc(cap * sizeof(CharT)));
    CharT* dst = out_buf;
    *dst++ = static_cast<CharT>('`');

    size_t i = 0;
    while (i < len) {
        size_t start = i;

#if defined(__x86_64__) || defined(_M_X64)
        if constexpr (sizeof(CharT) == 1) {
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
                if (mask != 0) {
#if defined(_MSC_VER)
                    unsigned long offset;
                    _BitScanForward(&offset, mask);
                    i += offset;
#else
                    i += __builtin_ctz(mask);
#endif
                    break;
                }
                i += 16;
            }
        }
#elif defined(__aarch64__) || defined(_M_ARM64)
        if constexpr (sizeof(CharT) == 1) {
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
                if (vmaxvq_u8(any) != 0) {
                    break;
                }
                i += 16;
            }
        }
#endif

        while (i < len) {
            CharT c = src[i];
            if constexpr (sizeof(CharT) == 1) {
                if (kIsSpecialChar[static_cast<uint8_t>(c)]) break;
            } else {
                if (c <= 255 && kIsSpecialChar[static_cast<uint8_t>(c)]) break;
            }
            i++;
        }

        size_t chunk_len = i - start;
        if (chunk_len > 0) {
            if (static_cast<size_t>(dst - out_buf) + chunk_len + 32 > cap) {
                size_t offset = dst - out_buf;
                cap = cap * 2 + chunk_len + 128;
                out_buf = static_cast<CharT*>(std::realloc(out_buf, cap * sizeof(CharT)));
                dst = out_buf + offset;
            }
            std::memcpy(dst, src + start, chunk_len * sizeof(CharT));
            dst += chunk_len;
        }
        if (i >= len) break;

        if (static_cast<size_t>(dst - out_buf) + 16 > cap) {
            size_t offset = dst - out_buf;
            cap = cap * 2 + 128;
            out_buf = static_cast<CharT*>(std::realloc(out_buf, cap * sizeof(CharT)));
            dst = out_buf + offset;
        }

        CharT c = src[i];
        switch (c) {
            case '\r':
                *dst++ = '\\'; *dst++ = 'r'; i++; break;
            case '\\':
                *dst++ = '\\'; *dst++ = '\\'; i++; break;
            case '`':
                *dst++ = '\\'; *dst++ = '`'; i++; break;
            case '\0':
                if (i + 1 < len && src[i + 1] >= '0' && src[i + 1] <= '9') {
                    *dst++ = '\\'; *dst++ = 'x'; *dst++ = '0'; *dst++ = '0';
                    *dst++ = src[i + 1];
                    i += 2;
                } else {
                    *dst++ = '\\'; *dst++ = '0'; i++;
                }
                break;
            case '$':
                if (i + 1 < len && src[i + 1] == '{') {
                    *dst++ = '\\'; *dst++ = '$'; *dst++ = '{';
                    i += 2;
                } else {
                    *dst++ = '$'; i++;
                }
                break;
            case '<':
                if (i + 7 < len &&
                    src[i + 1] == '/' && src[i + 2] == 's' &&
                    src[i + 3] == 'c' && src[i + 4] == 'r' &&
                    src[i + 5] == 'i' && src[i + 6] == 'p' &&
                    src[i + 7] == 't') {
                    const CharT tag[] = { '<', '\\', '/', 's', 'c', 'r', 'i', 'p', 't' };
                    std::memcpy(dst, tag, sizeof(tag));
                    dst += 9;
                    i += 8;
                } else {
                    *dst++ = '<'; i++;
                }
                break;
            default:
                *dst++ = c; i++; break;
        }
    }
    *dst++ = static_cast<CharT>('`');
    *out_len = dst - out_buf;
    return out_buf;
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
    if (self.IsObject()) {
        Napi::Object obj = self.As<Napi::Object>();
        Napi::Value bytesVal = obj.Get("bytes");
        if (bytesVal.IsTypedArray()) {
            Napi::TypedArray ta = bytesVal.As<Napi::TypedArray>();
            const char* data = reinterpret_cast<const char*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
            size_t len = ta.ByteLength();
            napi_value res;
            napi_status status = napi_create_string_latin1(env, data, len, &res);
            if (status == napi_ok) {
                return Napi::Value(env, res);
            }
            return Napi::String::New(env, data, len);
        }
    }
    return Napi::String::New(env, "");
}

static Napi::Value EncodeResult_ToJSTemplateLiterals(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    Napi::Value self = info.This();

    if (self.IsObject()) {
        Napi::Value bytesVal = self.As<Napi::Object>().Get("bytes");
        if (bytesVal.IsTypedArray()) {
            // Read this.bytes directly to accelerate string construction (guaranteed ASCII Base128)
            Napi::TypedArray ta = bytesVal.As<Napi::TypedArray>();
            const char* data = reinterpret_cast<const char*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
            size_t len = ta.ByteLength();
            size_t out_len = 0;
            char* out_buf = EscapeToTemplateLiterals(data, len, &out_len);
            return MakeLatin1String(env, out_buf, out_len);
        }
    }

    // String path: read uniformly as UTF-8
    napi_value strVal = self;
    if (!self.IsString()) {
        Napi::Value strRes = self.ToObject().Get("toString").As<Napi::Function>().Call(self, {});
        if (env.IsExceptionPending()) {
            return env.Null();
        }
        strVal = strRes;
    }

#ifdef USE_V8_ACCELERATION
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
        v8::Local<v8::Value> v8_val = *reinterpret_cast<v8::Local<v8::Value>*>(&strVal);
        if (v8_val->IsString()) {
            v8::Local<v8::String> v8_str = v8_val.As<v8::String>();
            bool is_one_byte = false;
            size_t out_len = 0;
            void* out_buf = nullptr;
            {
                v8::String::ValueView view(isolate, v8_str);
                is_one_byte = view.is_one_byte();
                if (is_one_byte) {
                    out_buf = EscapeToTemplateLiterals(reinterpret_cast<const char*>(view.data8()), view.length(), &out_len);
                } else {
                    out_buf = EscapeToTemplateLiterals(reinterpret_cast<const char16_t*>(view.data16()), view.length(), &out_len);
                }
            }
            if (is_one_byte) {
                return MakeLatin1String(env, static_cast<char*>(out_buf), out_len);
            }
            return MakeUtf16String(env, static_cast<char16_t*>(out_buf), out_len);
        }
    }
#endif

    size_t u16len = 0;
    napi_get_value_string_utf16(env, strVal, nullptr, 0, &u16len);

    size_t utf8_len = 0;
    napi_status status = napi_get_value_string_utf8(env, strVal, nullptr, 0, &utf8_len);
    if (status != napi_ok) {
        return env.Null();
    }

    if (utf8_len == u16len) {
        char* in_buf = static_cast<char*>(std::malloc(u16len + 1));
        size_t copied = 0;
        napi_get_value_string_latin1(env, strVal, in_buf, u16len + 1, &copied);
        size_t out_len = 0;
        char* out_buf = EscapeToTemplateLiterals(in_buf, u16len, &out_len);
        std::free(in_buf);
        return MakeLatin1String(env, out_buf, out_len);
    }

    char16_t* in_buf = static_cast<char16_t*>(std::malloc((u16len + 1) * sizeof(char16_t)));
    size_t copied = 0;
    napi_get_value_string_utf16(env, strVal, in_buf, u16len + 1, &copied);
    size_t out_len = 0;
    char16_t* out_buf = EscapeToTemplateLiterals(in_buf, u16len, &out_len);
    std::free(in_buf);
    return MakeUtf16String(env, out_buf, out_len);
}

static inline bool IsUint8Array(const Napi::Value& val) {
    if (!val.IsTypedArray()) {
        return false;
    }
    napi_typedarray_type type = val.As<Napi::TypedArray>().TypedArrayType();
    return type == napi_uint8_array || type == napi_uint8_clamped_array;
}

static Napi::Value Encode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !IsUint8Array(info[0])) {
        Napi::TypeError::New(env, "encode: input must be a Uint8Array or Uint8ClampedArray").ThrowAsJavaScriptException();
        return env.Null();
    }
    Napi::TypedArray input = info[0].As<Napi::TypedArray>();
    size_t il = input.ByteLength();
    const uint8_t* in = reinterpret_cast<const uint8_t*>(input.ArrayBuffer().Data()) + input.ByteOffset();

    size_t rem = il % 7;
    size_t out_len = (il / 7) * 8;
    if (rem > 0) {
        out_len += (rem * 8 + 6) / 7;
    }

    Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, out_len);
    uint8_t* out = reinterpret_cast<uint8_t*>(ab.Data());

    auto encode_7to8 = [](const uint8_t* src, uint8_t* dst) {
        dst[0] = src[0] >> 1;
        dst[1] = 127 & ((src[0] << 6) | (src[1] >> 2));
        dst[2] = 127 & ((src[1] << 5) | (src[2] >> 3));
        dst[3] = 127 & ((src[2] << 4) | (src[3] >> 4));
        dst[4] = 127 & ((src[3] << 3) | (src[4] >> 5));
        dst[5] = 127 & ((src[4] << 2) | (src[5] >> 6));
        dst[6] = 127 & ((src[5] << 1) | (src[6] >> 7));
        dst[7] = 127 & src[6];
    };

    size_t full_chunks = il / 7;
    for (size_t c = 0; c < full_chunks; c++, in += 7, out += 8) {
        encode_7to8(in, out);
    }

    if (rem > 0) {
        uint8_t b[7] = {0};
        memcpy(b, in, rem);
        uint8_t tmp[8];
        encode_7to8(b, tmp);
        memcpy(out, tmp, rem + 1);
    }

    Napi::Uint8Array out_ta = Napi::Uint8Array::New(env, out_len, ab, 0);
    return encodeResultConstructor.New({ out_ta });
}

static Napi::Value Decode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "decode: input must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }
    const uint8_t* in = nullptr;
    size_t il = 0;
    std::string str_holder;

    napi_value str_val = info[0];
    napi_get_value_string_latin1(env, str_val, nullptr, 0, &il);
    str_holder.resize(il);
    size_t copied = 0;
    napi_get_value_string_latin1(env, str_val, &str_holder[0], il + 1, &copied);
    in = reinterpret_cast<const uint8_t*>(str_holder.data());

    size_t out_len = (il * 7) / 8;
    Napi::ArrayBuffer ab = Napi::ArrayBuffer::New(env, out_len);
    uint8_t* out = reinterpret_cast<uint8_t*>(ab.Data());

    auto decode_8to7 = [](const uint8_t* src, uint8_t* dst) {
        dst[0] = (src[0] << 1) | (src[1] >> 6);
        dst[1] = (src[1] << 2) | (src[2] >> 5);
        dst[2] = (src[2] << 3) | (src[3] >> 4);
        dst[3] = (src[3] << 4) | (src[4] >> 3);
        dst[4] = (src[4] << 5) | (src[5] >> 2);
        dst[5] = (src[5] << 6) | (src[6] >> 1);
        dst[6] = (src[6] << 7) | src[7];
    };

    size_t full_chunks = il / 8;
    for (size_t c = 0; c < full_chunks; c++, in += 8, out += 7) {
        decode_8to7(in, out);
    }

    size_t rem = il % 8;
    if (rem >= 2) {
        uint8_t c[8] = {0};
        memcpy(c, in, rem);
        uint8_t tmp[7];
        decode_8to7(c, tmp);
        memcpy(out, tmp, (rem * 7) / 8);
    }
    return Napi::Uint8Array::New(env, out_len, ab, 0);
}

static inline int HexVal(char16_t c) {
    if (c >= u'0' && c <= u'9') return c - u'0';
    char16_t lower = c | 32;
    if (lower >= u'a' && lower <= u'f') return lower - u'a' + 10;
    return -1;
}

template <typename CharT>
static inline uint16_t CharCode(CharT c) {
    if constexpr (sizeof(CharT) == 1) {
        return static_cast<uint8_t>(c);
    } else {
        return static_cast<uint16_t>(c);
    }
}

template <typename CharT>
static bool ParseJSTemplateLiteralsImpl(const CharT* input, size_t len, std::u16string& out) {
    if (len < 2) return false;

    size_t start = 0;
    size_t end = len - 1;

    for (;; start++) {
        if (start == end) return false;
        uint16_t c = CharCode(input[start]);
        if (c == u'`') break;
        if (c != u' ' && (c < u'\t' || c > u'\r') && c != u'\u00A0' && c != u'\uFEFF') {
            return false;
        }
    }

    for (;; end--) {
        if (end == start) return false;
        uint16_t c = CharCode(input[end]);
        if (c == u'`') break;
        if (c != u' ' && (c < u'\t' || c > u'\r') && c != u'\u00A0' && c != u'\uFEFF') {
            return false;
        }
    }

    size_t i = start + 1;

    out.clear();
    out.reserve(end - i);

    while (i < end) {
        size_t chunkStart = i;
        while (i < end) {
            uint16_t c = CharCode(input[i]);
            if (c == u'\\' || c == u'`' || c == u'$') {
                break;
            }
            i++;
        }
        if (i > chunkStart) {
            size_t chunkLen = i - chunkStart;
            size_t curSize = out.size();
            out.resize(curSize + chunkLen);
            char16_t* dst = &out[curSize];
            if constexpr (sizeof(CharT) == 1) {
                for (size_t k = 0; k < chunkLen; k++) {
                    dst[k] = static_cast<uint8_t>(input[chunkStart + k]);
                }
            } else {
                std::memcpy(dst, input + chunkStart, chunkLen * sizeof(char16_t));
            }
        }
        if (i >= end) break;

        uint16_t c = CharCode(input[i]);
        if (c == u'`') {
            return false;
        }
        if (c == u'$') {
            if (i + 1 < end && CharCode(input[i + 1]) == u'{') {
                return false;
            }
            out.push_back(u'$');
            i++;
            continue;
        }

        // c is u'\\'
        i++;
        if (i >= end) {
            return false;
        }
        uint16_t next = CharCode(input[i]);
        switch (next) {
            case u'r': out.push_back(u'\r'); i++; break;
            case u'n': out.push_back(u'\n'); i++; break;
            case u't': out.push_back(u'\t'); i++; break;
            case u'b': out.push_back(u'\b'); i++; break;
            case u'f': out.push_back(u'\f'); i++; break;
            case u'v': out.push_back(u'\v'); i++; break;
            case u'\\': out.push_back(u'\\'); i++; break;
            case u'`': out.push_back(u'`'); i++; break;
            case u'\'': out.push_back(u'\''); i++; break;
            case u'"': out.push_back(u'"'); i++; break;
            case u'$': out.push_back(u'$'); i++; break;
            case u'0': {
                if (i + 1 < end && CharCode(input[i + 1]) >= u'0' && CharCode(input[i + 1]) <= u'9') {
                    return false;
                }
                out.push_back(u'\0');
                i++;
                break;
            }
            case u'x': {
                if (i + 2 >= end) return false;
                int h1 = HexVal(CharCode(input[i + 1]));
                if (h1 == -1) return false;
                int h2 = HexVal(CharCode(input[i + 2]));
                if (h2 == -1) return false;
                out.push_back(static_cast<char16_t>((h1 << 4) | h2));
                i += 3;
                break;
            }
            case u'u': {
                if (i + 1 < end && CharCode(input[i + 1]) == u'{') {
                    size_t startHex = i + 2;
                    uint32_t cp = 0;
                    size_t maxScan = std::min(end, startHex + 7);
                    size_t k = startHex;
                    for (; k < maxScan; k++) {
                        uint16_t ch = CharCode(input[k]);
                        if (ch == u'}') {
                            if (k == startHex || cp > 0x10FFFF) {
                                return false;
                            }
                            if (cp <= 0xFFFF) {
                                out.push_back(static_cast<char16_t>(cp));
                            } else {
                                cp -= 0x10000;
                                out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
                                out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
                            }
                            i = k + 1;
                            break;
                        }
                        int hv = HexVal(ch);
                        if (hv == -1) {
                            return false;
                        }
                        cp = (cp << 4) | hv;
                    }
                    if (k >= maxScan) {
                        return false;
                    }
                    break;
                }
                if (i + 4 >= end) return false;
                int h1 = HexVal(CharCode(input[i + 1]));
                if (h1 == -1) return false;
                int h2 = HexVal(CharCode(input[i + 2]));
                if (h2 == -1) return false;
                int h3 = HexVal(CharCode(input[i + 3]));
                if (h3 == -1) return false;
                int h4 = HexVal(CharCode(input[i + 4]));
                if (h4 == -1) return false;
                out.push_back(static_cast<char16_t>((h1 << 12) | (h2 << 8) | (h3 << 4) | h4));
                i += 5;
                break;
            }
            case u'\r': {
                if (i + 1 < end && CharCode(input[i + 1]) == u'\n') i++;
                i++;
                break;
            }
            case u'\n': {
                i++;
                break;
            }
            default: {
                out.push_back(static_cast<char16_t>(next));
                i++;
                break;
            }
        }
    }

    return true;
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

    std::u16string out;
    bool ok = false;

#ifdef USE_V8_ACCELERATION
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
        napi_value str_val = info[0];
        v8::Local<v8::Value> v8_val = *reinterpret_cast<v8::Local<v8::Value>*>(&str_val);
        if (v8_val->IsString()) {
            v8::Local<v8::String> v8_str = v8_val.As<v8::String>();
            {
                v8::String::ValueView view(isolate, v8_str);
                if (view.is_one_byte()) {
                    ok = ParseJSTemplateLiteralsImpl(reinterpret_cast<const char*>(view.data8()), view.length(), out);
                } else {
                    ok = ParseJSTemplateLiteralsImpl(reinterpret_cast<const char16_t*>(view.data16()), view.length(), out);
                }
            }
            if (!ok) {
                return ThrowSyntaxError();
            }
            return Napi::String::New(env, out.data(), out.size());
        }
    }
#endif

    std::u16string input = info[0].As<Napi::String>().Utf16Value();
    ok = ParseJSTemplateLiteralsImpl(input.data(), input.length(), out);
    if (!ok) {
        return ThrowSyntaxError();
    }
    return Napi::String::New(env, out.data(), out.size());
}

#ifdef USE_V8_ACCELERATION
#if defined(_MSC_VER)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static bool SafeTestV8(napi_env env) {
#if defined(_MSC_VER)
    __try
#else
    if (!dlsym(RTLD_DEFAULT, "_ZN2v87Isolate10GetCurrentEv")) {
        return false;
    }
#endif
    {
        v8::Isolate* isolate = v8::Isolate::GetCurrent();
        if (!isolate) return false;

        napi_value str = nullptr;
        if (napi_create_string_utf8(env, "`", 1, &str) != napi_ok || !str) return false;

        v8::Local<v8::Value>* v8_val = reinterpret_cast<v8::Local<v8::Value>*>(&str);
        return (*v8_val)->IsString();
    }
#if defined(_MSC_VER)
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
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

    encodeResultConstructor = Napi::Persistent(ctor);
    encodeResultConstructor.SuppressDestruct();

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
