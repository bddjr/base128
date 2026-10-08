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

struct ParsedStringResult {
    void* buf = nullptr;
    size_t len = 0;
    bool is_one_byte = true;
    bool error = false;
};

template <typename CharT>
static inline void EscapeToTemplateLiterals(const CharT* src, size_t len, ParsedStringResult& res) {
    res.error = true;
    res.buf = nullptr;
    res.len = 0;
    res.is_one_byte = true;

    size_t cap = len + (len / 4) + 128;
    char* buf8 = static_cast<char*>(std::malloc(cap));
    if (!buf8) return;

    if constexpr (sizeof(CharT) == 1) {
        char* dst = buf8;
        *dst++ = '`';

        size_t i = 0;
        while (i < len) {
            size_t start = i;

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
                if (vmaxvq_u8(any) != 0) {
                    break;
                }
                i += 16;
            }
#endif

            while (i < len) {
                CharT c = src[i];
                if (kIsSpecialChar[static_cast<uint8_t>(c)]) break;
                i++;
            }

            size_t chunk_len = i - start;
            if (chunk_len > 0) {
                if (static_cast<size_t>(dst - buf8) + chunk_len + 32 > cap) {
                    size_t offset = dst - buf8;
                    cap = cap * 2 + chunk_len + 128;
                    char* nb = static_cast<char*>(std::realloc(buf8, cap));
                    if (!nb) { std::free(buf8); return; }
                    buf8 = nb;
                    dst = buf8 + offset;
                }
                std::memcpy(dst, src + start, chunk_len);
                dst += chunk_len;
            }
            if (i >= len) break;

            if (static_cast<size_t>(dst - buf8) + 16 > cap) {
                size_t offset = dst - buf8;
                cap = cap * 2 + 128;
                char* nb = static_cast<char*>(std::realloc(buf8, cap));
                if (!nb) { std::free(buf8); return; }
                buf8 = nb;
                dst = buf8 + offset;
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
                        *dst++ = static_cast<char>(src[i + 1]);
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
                        const char tag[] = { '<', '\\', '/', 's', 'c', 'r', 'i', 'p', 't' };
                        std::memcpy(dst, tag, sizeof(tag));
                        dst += 9;
                        i += 8;
                    } else {
                        *dst++ = '<'; i++;
                    }
                    break;
                default:
                    *dst++ = static_cast<char>(c); i++; break;
            }
        }
        if (static_cast<size_t>(dst - buf8) + 2 > cap) {
            size_t offset = dst - buf8;
            cap += 16;
            char* nb = static_cast<char*>(std::realloc(buf8, cap));
            if (!nb) { std::free(buf8); return; }
            buf8 = nb;
            dst = buf8 + offset;
        }
        *dst++ = '`';
        res.buf = buf8;
        res.len = dst - buf8;
        res.is_one_byte = true;
        res.error = false;
        return;
    }

    char16_t* buf16 = nullptr;
    bool is_one_byte = true;
    size_t out_len = 0;

    auto switchToTwoByte = [&]() -> bool {
        buf16 = static_cast<char16_t*>(std::malloc(cap * sizeof(char16_t)));
        if (!buf16) {
            std::free(buf8);
            buf8 = nullptr;
            return false;
        }
        for (size_t k = 0; k < out_len; k++) {
            buf16[k] = static_cast<uint8_t>(buf8[k]);
        }
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
                    std::free(buf8);
                    buf8 = nullptr;
                    return false;
                }
                buf8 = nb;
            } else {
                char16_t* nb = static_cast<char16_t*>(std::realloc(buf16, cap * sizeof(char16_t)));
                if (!nb) {
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
                if (kIsSpecialChar[static_cast<uint8_t>(c)]) break;
            } else {
                if (c <= 255 && kIsSpecialChar[static_cast<uint8_t>(c)]) break;
            }
            i++;
        }

        size_t chunk_len = i - start;
        if (chunk_len > 0) {
            if (!ensureCap(chunk_len + 32)) return;
            if (is_one_byte) {
                for (size_t k = 0; k < chunk_len; k++) {
                    buf8[out_len + k] = static_cast<char>(src[start + k]);
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
                    if (i + 7 < len &&
                        src[i + 1] == '/' && src[i + 2] == 's' &&
                        src[i + 3] == 'c' && src[i + 4] == 'r' &&
                        src[i + 5] == 'i' && src[i + 6] == 'p' &&
                        src[i + 7] == 't') {
                        const char tag[] = { '<', '\\', '/', 's', 'c', 'r', 'i', 'p', 't' };
                        std::memcpy(buf8 + out_len, tag, sizeof(tag));
                        out_len += 9;
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
                    if (i + 7 < len &&
                        src[i + 1] == '/' && src[i + 2] == 's' &&
                        src[i + 3] == 'c' && src[i + 4] == 'r' &&
                        src[i + 5] == 'i' && src[i + 6] == 'p' &&
                        src[i + 7] == 't') {
                        const char16_t tag[] = { u'<', u'\\', u'/', u's', u'c', u'r', u'i', u'p', u't' };
                        std::memcpy(buf16 + out_len, tag, sizeof(tag));
                        out_len += 9;
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
            ParsedStringResult res;
            EscapeToTemplateLiterals(data, len, res);
            if (res.error) return env.Null();
            return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
        }
    }

    // String path: read uniformly
    napi_value strVal = self;
    if (!self.IsString()) {
        Napi::Value strRes = self.ToObject().Get("toString").As<Napi::Function>().Call(self, {});
        if (env.IsExceptionPending()) {
            return env.Null();
        }
        strVal = strRes;
    }

    ParsedStringResult res;

#ifdef USE_V8_ACCELERATION
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate) {
        v8::Local<v8::Value> v8_val = *reinterpret_cast<v8::Local<v8::Value>*>(&strVal);
        if (v8_val->IsString()) {
            v8::Local<v8::String> v8_str = v8_val.As<v8::String>();
            {
                v8::String::ValueView view(isolate, v8_str);
                if (view.is_one_byte()) {
                    EscapeToTemplateLiterals(reinterpret_cast<const char*>(view.data8()), view.length(), res);
                } else {
                    EscapeToTemplateLiterals(reinterpret_cast<const char16_t*>(view.data16()), view.length(), res);
                }
            }
            if (res.error) return env.Null();
            if (res.is_one_byte) {
                return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
            }
            return MakeUtf16String(env, static_cast<char16_t*>(res.buf), res.len);
        }
    }
#endif

    std::u16string input = Napi::Value(env, strVal).As<Napi::String>().Utf16Value();
    EscapeToTemplateLiterals(input.data(), input.length(), res);
    if (res.error) return env.Null();
    if (res.is_one_byte) {
        return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
    }
    return MakeUtf16String(env, static_cast<char16_t*>(res.buf), res.len);
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

template <typename CharT>
static inline int HexVal(CharT c) {
    if (c >= '0' && c <= '9') return c - '0';
    char lower = static_cast<char>(c | 32);
    if (lower >= 'a' && lower <= 'f') return lower - 'a' + 10;
    return -1;
}


template <typename CharT>
static void ParseJSTemplateLiteralsImpl(const CharT* input, size_t len, ParsedStringResult& res) {
    res.error = true;
    res.buf = nullptr;
    res.len = 0;

    if (len < 2) return;

    size_t start = 0;
    size_t end = len - 1;

    for (;; start++) {
        if (start == end) return;
        CharT c = input[start];
        if (c == '`') break;
        if (c != ' ' && (c < '\t' || c > '\r') && static_cast<uint16_t>(c) != 0x00A0 && static_cast<uint16_t>(c) != 0xFEFF) {
            return;
        }
    }

    for (;; end--) {
        if (end == start) return;
        CharT c = input[end];
        if (c == '`') break;
        if (c != ' ' && (c < '\t' || c > '\r') && static_cast<uint16_t>(c) != 0x00A0 && static_cast<uint16_t>(c) != 0xFEFF) {
            return;
        }
    }

    size_t i = start + 1;
    size_t cap = (end > start) ? (end - start) : 1;

    char* buf8 = static_cast<char*>(std::malloc(cap));
    if (!buf8) return;
    char16_t* buf16 = nullptr;
    bool is_one_byte = true;
    size_t out_len = 0;

    auto cleanup = [&]() {
        if (is_one_byte) {
            std::free(buf8);
        } else {
            std::free(buf16);
        }
    };

    auto switchToTwoByte = [&]() {
        buf16 = static_cast<char16_t*>(std::malloc(cap * sizeof(char16_t)));
        for (size_t k = 0; k < out_len; k++) {
            buf16[k] = static_cast<uint8_t>(buf8[k]);
        }
        std::free(buf8);
        buf8 = nullptr;
        is_one_byte = false;
    };

    auto pushChar = [&](char16_t ch) {
        if (is_one_byte) {
            if (ch <= 255) {
                buf8[out_len++] = static_cast<char>(ch);
                return;
            }
            switchToTwoByte();
        }
        buf16[out_len++] = ch;
    };

    while (i < end) {
        size_t chunkStart = i;
        while (i < end) {
            CharT c = input[i];
            if (c == '\\' || c == '`' || c == '$') {
                break;
            }
            i++;
        }
        if (i > chunkStart) {
            size_t chunkLen = i - chunkStart;
            if (is_one_byte) {
                if constexpr (sizeof(CharT) == 1) {
                    std::memcpy(buf8 + out_len, input + chunkStart, chunkLen);
                } else {
                    bool has_wide = false;
                    for (size_t k = 0; k < chunkLen; k++) {
                        if (static_cast<uint16_t>(input[chunkStart + k]) > 255) {
                            has_wide = true;
                            break;
                        }
                    }
                    if (has_wide) {
                        switchToTwoByte();
                        for (size_t k = 0; k < chunkLen; k++) {
                            buf16[out_len + k] = static_cast<char16_t>(input[chunkStart + k]);
                        }
                    } else {
                        for (size_t k = 0; k < chunkLen; k++) {
                            buf8[out_len + k] = static_cast<char>(input[chunkStart + k]);
                        }
                    }
                }
            } else {
                if constexpr (sizeof(CharT) == 1) {
                    for (size_t k = 0; k < chunkLen; k++) {
                        buf16[out_len + k] = static_cast<uint8_t>(input[chunkStart + k]);
                    }
                } else {
                    std::memcpy(buf16 + out_len, input + chunkStart, chunkLen * sizeof(char16_t));
                }
            }
            out_len += chunkLen;
        }
        if (i >= end) break;

        CharT c = input[i];
        if (c == '`') {
            cleanup();
            return;
        }
        if (c == '$') {
            if (i + 1 < end && input[i + 1] == '{') {
                cleanup();
                return;
            }
            pushChar(u'$');
            i++;
            continue;
        }

        // c is '\\'
        i++;
        if (i >= end) {
            cleanup();
            return;
        }
        CharT next = input[i];
        switch (next) {
            case 'r': pushChar(u'\r'); i++; break;
            case 'n': pushChar(u'\n'); i++; break;
            case 't': pushChar(u'\t'); i++; break;
            case 'b': pushChar(u'\b'); i++; break;
            case 'f': pushChar(u'\f'); i++; break;
            case 'v': pushChar(u'\v'); i++; break;
            case '\\': pushChar(u'\\'); i++; break;
            case '`': pushChar(u'`'); i++; break;
            case '\'': pushChar(u'\''); i++; break;
            case '"': pushChar(u'"'); i++; break;
            case '$': pushChar(u'$'); i++; break;
            case '0': {
                if (i + 1 < end && input[i + 1] >= '0' && input[i + 1] <= '9') {
                    cleanup();
                    return;
                }
                pushChar(u'\0');
                i++;
                break;
            }
            case 'x': {
                if (i + 2 >= end) { cleanup(); return; }
                int h1 = HexVal(input[i + 1]);
                if (h1 == -1) { cleanup(); return; }
                int h2 = HexVal(input[i + 2]);
                if (h2 == -1) { cleanup(); return; }
                pushChar(static_cast<char16_t>((h1 << 4) | h2));
                i += 3;
                break;
            }
            case 'u': {
                if (i + 1 < end && input[i + 1] == '{') {
                    size_t startHex = i + 2;
                    uint32_t cp = 0;
                    size_t maxScan = std::min(end, startHex + 7);
                    size_t k = startHex;
                    for (; k < maxScan; k++) {
                        CharT ch = input[k];
                        if (ch == '}') {
                            if (k == startHex || cp > 0x10FFFF) {
                                cleanup();
                                return;
                            }
                            if (cp <= 0xFFFF) {
                                pushChar(static_cast<char16_t>(cp));
                            } else {
                                cp -= 0x10000;
                                if (is_one_byte) switchToTwoByte();
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
                    }
                    if (k >= maxScan) {
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
                pushChar(static_cast<char16_t>((h1 << 12) | (h2 << 8) | (h3 << 4) | h4));
                i += 5;
                break;
            }
            case '\r': {
                if (i + 1 < end && input[i + 1] == '\n') i++;
                i++;
                break;
            }
            case '\n': {
                i++;
                break;
            }
            default: {
                pushChar(static_cast<char16_t>(next));
                i++;
                break;
            }
        }
    }

    res.buf = is_one_byte ? static_cast<void*>(buf8) : static_cast<void*>(buf16);
    res.len = out_len;
    res.is_one_byte = is_one_byte;
    res.error = false;
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
                    ParseJSTemplateLiteralsImpl(reinterpret_cast<const char*>(view.data8()), view.length(), res);
                } else {
                    ParseJSTemplateLiteralsImpl(reinterpret_cast<const char16_t*>(view.data16()), view.length(), res);
                }
            }
            if (res.error) {
                return ThrowSyntaxError();
            }
            if (res.is_one_byte) {
                return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
            }
            return MakeUtf16String(env, static_cast<char16_t*>(res.buf), res.len);
        }
    }
#endif

    std::u16string input = info[0].As<Napi::String>().Utf16Value();
    ParseJSTemplateLiteralsImpl(input.data(), input.length(), res);
    if (res.error) {
        return ThrowSyntaxError();
    }
    if (res.is_one_byte) {
        return MakeLatin1String(env, static_cast<char*>(res.buf), res.len);
    }
    return MakeUtf16String(env, static_cast<char16_t*>(res.buf), res.len);
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
