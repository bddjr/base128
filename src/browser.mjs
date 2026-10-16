// base128-ascii (The Unlicense)

export class EncodeResult {
    /**
     * @param {Uint8Array<ArrayBuffer>} bytes
     */
    constructor(bytes) {
        this.bytes = bytes
    }
    toString() {
        // TextDecoder keeps the default UTF-8, which is already the fastest.
        return new TextDecoder().decode(this.bytes)
    }
    toJSTemplateLiterals() {
        return '`' + this.toString().replace(
            /[\r\\`]|\0\d?|\$\{|<\/script/gi,
            (match) => {
                switch (match) {
                    case '\r': return '\\r';
                    case '\\': return '\\\\';
                    case '`': return '\\`';
                    case '\0': return '\\0';
                    case '\x000': return '\\x000';
                    case '\x001': return '\\x001';
                    case '\x002': return '\\x002';
                    case '\x003': return '\\x003';
                    case '\x004': return '\\x004';
                    case '\x005': return '\\x005';
                    case '\x006': return '\\x006';
                    case '\x007': return '\\x007';
                    case '\x008': return '\\x008';
                    case '\x009': return '\\x009';
                    case '${': return '\\${';
                    default: return '<\\/' + match.slice(2);
                }
            }
        ) + '`'
    }
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function encode(input) {
    a: if (typeof input == 'string') {
        input = new TextEncoder().encode(input)
    } else {
        // Use Symbol.toStringTag to support recognizing objects from iframes
        if (input) switch (input[Symbol.toStringTag]) {
            case "Uint8Array":
            case "Uint8ClampedArray":
                break a
        }
        throw TypeError("encode: input must be a string, Uint8Array or Uint8ClampedArray")
    }
    const il = input.length
        , rem = il % 7
        , limit = il - rem
        , out = new Uint8Array(il + (limit / 7) + /**@type {*}*/(!!rem))
    var ii = 0
        , oi = 0
    while (ii < limit) {
        //     0        1        2        3        4        5        6        7
        // in  00000000 11111111 22222222 33333333 44444444 55555555 66666666
        // out _0000000 _0111111 _1122222 _2223333 _3333444 _4444455 _5555556 _6666666

        const b0 = input[ii++]
            , b1 = input[ii++]
            , b2 = input[ii++]
            , b3 = input[ii++]
            , b4 = input[ii++]
            , b5 = input[ii++]
            , b6 = input[ii++]

        /* 0 */ out[oi++] = b0 >> 1
        /* 1 */ out[oi++] = 127 & (b0 << 6 | b1 >> 2)
        /* 2 */ out[oi++] = 127 & (b1 << 5 | b2 >> 3)
        /* 3 */ out[oi++] = 127 & (b2 << 4 | b3 >> 4)
        /* 4 */ out[oi++] = 127 & (b3 << 3 | b4 >> 5)
        /* 5 */ out[oi++] = 127 & (b4 << 2 | b5 >> 6)
        /* 6 */ out[oi++] = 127 & (b5 << 1 | b6 >> 7)
        /* 7 */ out[oi++] = 127 & b6
    }
    if (rem) {
        let prev = input[ii++]
        out[oi++] = prev >> 1
        for (let r = 1; r < rem; r++) {
            const curr = input[ii++]
            out[oi++] = 127 & (prev << (7 - r) | curr >> (1 + r))
            prev = curr
        }
        out[oi++] = 127 & (prev << (7 - rem))
    }
    return new EncodeResult(out)
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function decode(input) {
    //     0        1        2        3        4        5        6        7
    // in  _0000000 _1111111 _2222222 _3333333 _4444444 _5555555 _6666666 _7777777
    // out 00000001 11111122 22222333 33334444 44455555 55666666 67777777
    const isStr = typeof input == 'string'
    a: if (!isStr) {
        // Use Symbol.toStringTag to support recognizing objects from iframes
        if (input) switch (input[Symbol.toStringTag]) {
            case "Uint8Array":
            case "Uint8ClampedArray":
                break a
        }
        throw TypeError("decode: input must be a string, Uint8Array or Uint8ClampedArray");
    }

    const errInvalidInput = `decode: invalid input`
        , il = input.length
        , rem = il & 7
    if (rem === 1) throw SyntaxError(errInvalidInput)
    const limit = il - rem
        , out = new Uint8Array(il - (limit / 8) - /**@type {*}*/(!!rem))
    var ii = 0
        , oi = 0
        , c0
        , c1

    if (isStr) {
        while (ii < limit) {
            c0 = input.charCodeAt(ii++)
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            c1 = input.charCodeAt(ii++)
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 1) | (c1 >> 6)

            c0 = input.charCodeAt(ii++)
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c1 << 2) | (c0 >> 5)

            c1 = input.charCodeAt(ii++)
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 3) | (c1 >> 4)

            c0 = input.charCodeAt(ii++)
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c1 << 4) | (c0 >> 3)

            c1 = input.charCodeAt(ii++)
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 5) | (c1 >> 2)

            c0 = input.charCodeAt(ii++)
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c1 << 6) | (c0 >> 1)

            c1 = input.charCodeAt(ii++)
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 7) | c1
        }
        if (rem) {
            c0 = input.charCodeAt(ii++)
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            for (let r = 1; r < rem; r++) {
                c1 = input.charCodeAt(ii++)
                if (c1 > 127) throw SyntaxError(errInvalidInput)
                out[oi++] = (c0 << r) | (c1 >> (7 - r))
                c0 = c1
            }
        }
    } else {
        while (ii < limit) {
            c0 = input[ii++]
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            c1 = input[ii++]
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 1) | (c1 >> 6)

            c0 = input[ii++]
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c1 << 2) | (c0 >> 5)

            c1 = input[ii++]
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 3) | (c1 >> 4)

            c0 = input[ii++]
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c1 << 4) | (c0 >> 3)

            c1 = input[ii++]
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 5) | (c1 >> 2)

            c0 = input[ii++]
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c1 << 6) | (c0 >> 1)

            c1 = input[ii++]
            if (c1 > 127) throw SyntaxError(errInvalidInput)
            out[oi++] = (c0 << 7) | c1
        }
        if (rem) {
            c0 = input[ii++]
            if (c0 > 127) throw SyntaxError(errInvalidInput)
            for (let r = 1; r < rem; r++) {
                c1 = input[ii++]
                if (c1 > 127) throw SyntaxError(errInvalidInput)
                out[oi++] = (c0 << r) | (c1 >> (7 - r))
                c0 = c1
            }
        }
    }
    return out
}

/**
 * @param {number} c
 */
function isTemplateLiteralWhitespace(c) {
    return c === 32 || (c >= 9 && c <= 13) || c === 160 || c === 65279 ||
        c === 8232 || c === 8233 || c === 0x1680 || (c >= 0x2000 && c <= 0x200A) ||
        c === 0x202F || c === 0x205F || c === 0x3000;
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function parseJSTemplateLiterals(input) {
    a: if (typeof input != 'string') {
        // Use Symbol.toStringTag to support recognizing objects from iframes
        if (input) switch (input[Symbol.toStringTag]) {
            case "Uint8Array":
            case "Uint8ClampedArray":
                input = new TextDecoder().decode(input)
                break a
        }
        throw TypeError("parseJSTemplateLiterals: input must be a string, Uint8Array or Uint8ClampedArray");
    }

    const err = "parseJSTemplateLiterals: invalid input"

    const len = input.length
    if (len < 2) throw SyntaxError(err);

    var end = len - 1
    var start = 0
    for (; ; start++) {
        if (start === end) throw SyntaxError(err);
        const c = input.charCodeAt(start)
        // '`'
        if (c === 96) break
        if (!isTemplateLiteralWhitespace(c)) throw SyntaxError(err)
    }
    for (; ; end--) {
        if (end === start) throw SyntaxError(err);
        const c = input.charCodeAt(end)
        // '`'
        if (c === 96) break
        if (!isTemplateLiteralWhitespace(c)) throw SyntaxError(err)
    }

    var m
    var i = start + 1
    var out = ''

    /**
     * @param {number} endIndex
     */
    const hexVal = (endIndex) => {
        if (endIndex > end) throw SyntaxError(err);
        var val = 0
        for (; i < endIndex; i++) {
            let c = input.charCodeAt(i)
            // '0' - '9'
            if (c >= 48 && c <= 57) {
                val = (val << 4) | (c - 48)
            } else {
                c |= 32
                // 'a' - 'f'
                if (c < 97 || c > 102) throw SyntaxError(err);
                val = (val << 4) | (c - 87)
            }
        }
        return val
    }

    const re = /[\\`$\r]/g
    re.lastIndex = i

    while (m = re.exec(input)) {
        const idx = m.index
        if (idx >= end) break

        if (idx > i) {
            out += input.slice(i, idx)
        }

        const c = input.charCodeAt(idx)
        // '\r'
        if (c === 13) {
            out += '\n'
            i = idx + 1
            // '\n'
            if (i < end && input.charCodeAt(i) === 10)
                i++
            re.lastIndex = i
            continue
        }
        // '`'
        if (c === 96) throw SyntaxError(err)
        // '$'
        if (c === 36) {
            // '{'
            if (idx + 1 < end && input.charCodeAt(idx + 1) === 123) throw SyntaxError(err)
            out += '$'
            re.lastIndex = i = idx + 1
            continue
        }

        const nextIdx = idx + 1
        if (nextIdx >= end) throw SyntaxError(err)
        i = nextIdx + 1
        const next = input.charCodeAt(nextIdx)
        SWITCH: switch (next) {
            // 'r'
            case 114: out += '\r'; break
            // 'n'
            case 110: out += '\n'; break
            // 't'
            case 116: out += '\t'; break
            // 'b'
            case 98: out += '\b'; break
            // 'f'
            case 102: out += '\f'; break
            // 'v'
            case 118: out += '\v'; break
            // '0'
            case 48:
                // '0' - '9'
                if (i < end) {
                    const c = input.charCodeAt(i)
                    if (c >= 48 && c <= 57) throw SyntaxError(err);
                }
                out += '\0'
                break
            // 'x'
            case 120:
                out += String.fromCharCode(hexVal(i + 2))
                break
            // 'u'
            case 117:
                // '{'
                if (i < end && input.charCodeAt(i) === 123) {
                    const startHex = ++i
                    for (let cp = 0; i < end; i++) {
                        const ch = input.charCodeAt(i)
                        // '}'
                        if (ch === 125) {
                            if (i === startHex) throw SyntaxError(err);
                            out += String.fromCodePoint(cp)
                            i++
                            break SWITCH
                        }
                        // '0' - '9'
                        if (ch >= 48 && ch <= 57) {
                            cp = (cp << 4) | (ch - 48)
                        } else {
                            const lower = ch | 32
                            // 'a' - 'f'
                            if (lower < 97 || lower > 102) throw SyntaxError(err);
                            cp = (cp << 4) | (lower - 87)
                        }
                        if (cp > 0x10FFFF) throw SyntaxError(err);
                    }
                    throw SyntaxError(err);
                }
                out += String.fromCharCode(hexVal(i + 4))
                break
            // '\r'
            case 13:
                // '\n'
                if (i < end && input.charCodeAt(i) === 10)
                    i++
                break
            // '\n', LS, PS
            case 10:
            case 0x2028:
            case 0x2029:
                break
            default:
                // '1' - '9'
                if (next >= 49 && next <= 57) throw SyntaxError(err);
                out += input[nextIdx]
        }
        re.lastIndex = i
    }
    if (i < end) {
        out += input.slice(i, end)
    }
    return out
}

export const _impl = 'js'

export default {
    _impl,
    EncodeResult,
    encode,
    decode,
    parseJSTemplateLiterals
}