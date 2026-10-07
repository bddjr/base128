#include <napi.h>
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

template <typename CharT>
static inline std::string EscapeToTemplateLiteralsOneByte(const CharT* src, size_t len) {
    std::string out;
    out.reserve(len + (len / 8) + 4);
    out.push_back('`');

    size_t i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && !kIsSpecialChar[static_cast<uint8_t>(src[i])]) {
            i++;
        }
        if (i > start) {
            if constexpr (std::is_same_v<CharT, uint8_t> || std::is_same_v<CharT, char>) {
                out.append(reinterpret_cast<const char*>(src + start), i - start);
            } else {
                size_t chunk_len = i - start;
                size_t old_size = out.size();
                out.resize(old_size + chunk_len);
                char* dst = &out[old_size];
                for (size_t k = 0; k < chunk_len; ++k) {
                    dst[k] = static_cast<char>(src[start + k]);
                }
            }
        }
        if (i >= len) break;

        CharT c = src[i];
        switch (c) {
            case '\r':
                out.append("\\r");
                i++;
                break;
            case '\\':
                out.append("\\\\");
                i++;
                break;
            case '`':
                out.append("\\`");
                i++;
                break;
            case '\0':
                if (i + 1 < len && src[i + 1] >= '0' && src[i + 1] <= '9') {
                    out.append("\\x00");
                    out.push_back(static_cast<char>(src[i + 1]));
                    i += 2;
                } else {
                    out.append("\\0");
                    i++;
                }
                break;
            case '$':
                if (i + 1 < len && src[i + 1] == '{') {
                    out.append("\\${");
                    i += 2;
                } else {
                    out.push_back('$');
                    i++;
                }
                break;
            case '<':
                if (i + 7 < len &&
                    src[i + 1] == '/' &&
                    src[i + 2] == 's' &&
                    src[i + 3] == 'c' &&
                    src[i + 4] == 'r' &&
                    src[i + 5] == 'i' &&
                    src[i + 6] == 'p' &&
                    src[i + 7] == 't') {
                    out.append("<\\/script");
                    i += 8;
                } else {
                    out.push_back('<');
                    i++;
                }
                break;
            default:
                out.push_back(static_cast<char>(c));
                i++;
                break;
        }
    }
    out.push_back('`');
    return out;
}

static inline std::u16string EscapeToTemplateLiteralsTwoByte(const char16_t* src, size_t len) {
    std::u16string out;
    out.reserve(len + (len / 8) + 4);
    out.push_back(u'`');

    size_t i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && (src[i] > 255 || !kIsSpecialChar[static_cast<uint8_t>(src[i])])) {
            i++;
        }
        if (i > start) {
            out.append(src + start, i - start);
        }
        if (i >= len) break;

        char16_t c = src[i];
        switch (c) {
            case u'\r':
                out.append(u"\\r");
                i++;
                break;
            case u'\\':
                out.append(u"\\\\");
                i++;
                break;
            case u'`':
                out.append(u"\\`");
                i++;
                break;
            case u'\0':
                if (i + 1 < len && src[i + 1] >= u'0' && src[i + 1] <= u'9') {
                    out.append(u"\\x00");
                    out.push_back(src[i + 1]);
                    i += 2;
                } else {
                    out.append(u"\\0");
                    i++;
                }
                break;
            case u'$':
                if (i + 1 < len && src[i + 1] == u'{') {
                    out.append(u"\\${");
                    i += 2;
                } else {
                    out.push_back(u'$');
                    i++;
                }
                break;
            case u'<':
                if (i + 7 < len &&
                    src[i + 1] == u'/' &&
                    src[i + 2] == u's' &&
                    src[i + 3] == u'c' &&
                    src[i + 4] == u'r' &&
                    src[i + 5] == u'i' &&
                    src[i + 6] == u'p' &&
                    src[i + 7] == u't') {
                    out.append(u"<\\/script");
                    i += 8;
                } else {
                    out.push_back(u'<');
                    i++;
                }
                break;
            default:
                out.push_back(c);
                i++;
                break;
        }
    }
    out.push_back(u'`');
    return out;
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
            const uint8_t* data = reinterpret_cast<const uint8_t*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
            size_t len = ta.ByteLength();
            std::string escaped = EscapeToTemplateLiteralsOneByte(data, len);
            napi_value res;
            napi_status status = napi_create_string_latin1(env, escaped.data(), escaped.size(), &res);
            if (status == napi_ok) {
                return Napi::Value(env, res);
            }
            return Napi::String::New(env, escaped);
        }
    }

    // Fallback: convert to string via ToString() and detect single-byte vs two-byte
    Napi::Value strRes = self.ToObject().Get("toString").As<Napi::Function>().Call(self, {});
    if (env.IsExceptionPending()) {
        return env.Null();
    }
    Napi::String strVal = strRes.ToString();

    std::u16string u16str = strVal.Utf16Value();
    bool isTwoByte = std::any_of(u16str.begin(), u16str.end(), [](char16_t c) { return c > 255; });

    if (isTwoByte) {
        std::u16string escaped = EscapeToTemplateLiteralsTwoByte(u16str.data(), u16str.size());
        return Napi::String::New(env, escaped.data(), escaped.size());
    }

    std::string escaped = EscapeToTemplateLiteralsOneByte(u16str.data(), u16str.size());
    napi_value res;
    napi_status status = napi_create_string_latin1(env, escaped.data(), escaped.size(), &res);
    if (status == napi_ok) {
        return Napi::Value(env, res);
    }
    return Napi::String::New(env, escaped);
}

static Napi::Value Encode(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsTypedArray()) {
        Napi::TypeError::New(env, "encode: input must be a Uint8Array").ThrowAsJavaScriptException();
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
    if (info.Length() < 1) {
        Napi::TypeError::New(env, "decode: input must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }
    const uint8_t* in = nullptr;
    size_t il = 0;
    std::string str_holder;

    if (info[0].IsString()) {
        napi_value str_val = info[0];
        napi_get_value_string_latin1(env, str_val, nullptr, 0, &il);
        str_holder.resize(il);
        size_t copied = 0;
        napi_get_value_string_latin1(env, str_val, &str_holder[0], il + 1, &copied);
        in = reinterpret_cast<const uint8_t*>(str_holder.data());
    } else if (info[0].IsTypedArray()) {
        Napi::TypedArray ta = info[0].As<Napi::TypedArray>();
        il = ta.ByteLength();
        in = reinterpret_cast<const uint8_t*>(ta.ArrayBuffer().Data()) + ta.ByteOffset();
    } else {
        Napi::TypeError::New(env, "decode: input must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

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

static inline void ThrowSyntaxError(Napi::Env env, const char* msg) {
    Napi::Function syntaxErrorCtor = env.Global().Get("SyntaxError").As<Napi::Function>();
    Napi::Object err = syntaxErrorCtor.New({ Napi::String::New(env, msg) });
    napi_throw(env, err);
}

static inline bool IsHexDigit(char16_t c) {
    return (c >= u'0' && c <= u'9') ||
           (c >= u'a' && c <= u'f') ||
           (c >= u'A' && c <= u'F');
}

static Napi::Value ParseJSTemplateLiterals(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "parseJSTemplateLiterals: input must be a string").ThrowAsJavaScriptException();
        return env.Null();
    }

    const char* err = "parseJSTemplateLiterals: invalid input";
    std::u16string input = info[0].As<Napi::String>().Utf16Value();

    // Trim whitespace
    size_t start = 0;
    while (start < input.length() && (
        input[start] == u' ' || input[start] == u'\t' ||
        input[start] == u'\r' || input[start] == u'\n' ||
        input[start] == u'\v' || input[start] == u'\f' ||
        input[start] == 0x00A0 || input[start] == 0xFEFF)) {
        start++;
    }
    size_t end = input.length();
    while (end > start && (
        input[end - 1] == u' ' || input[end - 1] == u'\t' ||
        input[end - 1] == u'\r' || input[end - 1] == u'\n' ||
        input[end - 1] == u'\v' || input[end - 1] == u'\f' ||
        input[end - 1] == 0x00A0 || input[end - 1] == 0xFEFF)) {
        end--;
    }

    if (end - start < 2) {
        ThrowSyntaxError(env, err);
        return env.Null();
    }
    if (input[start] != u'`' || input[end - 1] != u'`') {
        ThrowSyntaxError(env, err);
        return env.Null();
    }

    size_t loopMaxIndex = end - 1;
    size_t i = start + 1;

    std::u16string out;
    out.reserve(loopMaxIndex - i);

    while (i < loopMaxIndex) {
        size_t chunkStart = i;
        while (i < loopMaxIndex) {
            char16_t c = input[i];
            if (c == u'\\' || c == u'`' || c == u'$') {
                break;
            }
            i++;
        }
        if (i > chunkStart) {
            out.append(input.data() + chunkStart, i - chunkStart);
        }
        if (i >= loopMaxIndex) break;

        char16_t c = input[i];
        if (c == u'`') {
            ThrowSyntaxError(env, err);
            return env.Null();
        }
        if (c == u'$') {
            if (i + 1 < loopMaxIndex && input[i + 1] == u'{') {
                ThrowSyntaxError(env, err);
                return env.Null();
            }
            out.push_back(u'$');
            i++;
            continue;
        }

        // c is u'\\'
        i++;
        if (i >= loopMaxIndex) {
            ThrowSyntaxError(env, err);
            return env.Null();
        }
        char16_t next = input[i];
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
                if (i + 1 < loopMaxIndex && input[i + 1] >= u'0' && input[i + 1] <= u'9') {
                    ThrowSyntaxError(env, err);
                    return env.Null();
                }
                out.push_back(u'\0');
                i++;
                break;
            }
            case u'x': {
                if (i + 2 < loopMaxIndex && IsHexDigit(input[i + 1]) && IsHexDigit(input[i + 2])) {
                    char hex[3] = { static_cast<char>(input[i + 1]), static_cast<char>(input[i + 2]), 0 };
                    out.push_back(static_cast<char16_t>(strtoul(hex, nullptr, 16)));
                    i += 3;
                } else {
                    ThrowSyntaxError(env, err);
                    return env.Null();
                }
                break;
            }
            case u'u': {
                if (i + 1 < loopMaxIndex && input[i + 1] == u'{') {
                    size_t closeBrace = input.find(u'}', i + 2);
                    if (closeBrace != std::u16string::npos && closeBrace <= loopMaxIndex && closeBrace - (i + 2) <= 6) {
                        std::string hex;
                        for (size_t k = i + 2; k < closeBrace; k++) {
                            if (!IsHexDigit(input[k])) { hex.clear(); break; }
                            hex.push_back(static_cast<char>(input[k]));
                        }
                        if (!hex.empty()) {
                            uint32_t cp = static_cast<uint32_t>(strtoul(hex.c_str(), nullptr, 16));
                            if (cp <= 0xFFFF) {
                                out.push_back(static_cast<char16_t>(cp));
                            } else if (cp <= 0x10FFFF) {
                                cp -= 0x10000;
                                out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
                                out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
                            } else {
                                ThrowSyntaxError(env, err);
                                return env.Null();
                            }
                            i = closeBrace + 1;
                        } else {
                            ThrowSyntaxError(env, err);
                            return env.Null();
                        }
                    } else {
                        ThrowSyntaxError(env, err);
                        return env.Null();
                    }
                } else if (i + 4 < loopMaxIndex &&
                           IsHexDigit(input[i + 1]) && IsHexDigit(input[i + 2]) &&
                           IsHexDigit(input[i + 3]) && IsHexDigit(input[i + 4])) {
                    char hex[5] = {
                        static_cast<char>(input[i + 1]), static_cast<char>(input[i + 2]),
                        static_cast<char>(input[i + 3]), static_cast<char>(input[i + 4]), 0
                    };
                    out.push_back(static_cast<char16_t>(strtoul(hex, nullptr, 16)));
                    i += 5;
                } else {
                    ThrowSyntaxError(env, err);
                    return env.Null();
                }
                break;
            }
            case u'\r': {
                if (i + 1 < loopMaxIndex && input[i + 1] == u'\n') i++;
                i++;
                break;
            }
            case u'\n': {
                i++;
                break;
            }
            default: {
                out.push_back(next);
                i++;
                break;
            }
        }
    }

    return Napi::String::New(env, out.data(), out.size());
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
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

    exports.Set("default", defaultObj);

    return exports;
}

NODE_API_MODULE(base128, Init)
