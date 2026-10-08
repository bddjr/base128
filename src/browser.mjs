export class EncodeResult {
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
 * @param {Uint8Array | Uint8ClampedArray} input
 */
export function encode(input) {
    if (input == null)
        throw TypeError("encode: input must be a Uint8Array or Uint8ClampedArray")
    // Use Symbol.toStringTag to support recognizing objects from iframes
    switch (input[Symbol.toStringTag]) {
        case "Uint8Array":
        case "Uint8ClampedArray":
            break
        default:
            throw TypeError("encode: input must be a Uint8Array or Uint8ClampedArray")
    }
    const il = input.length
        , rem = il % 7
        , fullChunks = (il - rem) / 7
        , out = new Uint8Array(fullChunks * 8 + Math.ceil(rem / 0.875))
        , limit = fullChunks * 7
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
 * @param {string} input
 */
export function decode(input) {
    //     0        1        2        3        4        5        6        7
    // in  _0000000 _1111111 _2222222 _3333333 _4444444 _5555555 _6666666 _7777777
    // out 00000001 11111122 22222333 33334444 44455555 55666666 67777777
    if (typeof input != 'string')
        throw TypeError("decode: input must be a string");
    const il = input.length
        , rem = il % 8
        , fullChunks = (il - rem) / 8
        , out = new Uint8Array(il * 0.875)
        , limit = fullChunks * 8
    var ii = 0
        , oi = 0
    while (ii < limit) {
        const c0 = input.charCodeAt(ii++)
            , c1 = input.charCodeAt(ii++)
            , c2 = input.charCodeAt(ii++)
            , c3 = input.charCodeAt(ii++)
            , c4 = input.charCodeAt(ii++)
            , c5 = input.charCodeAt(ii++)
            , c6 = input.charCodeAt(ii++)
            , c7 = input.charCodeAt(ii++)

        out[oi++] = (c0 << 1) | (c1 >> 6)
        out[oi++] = (c1 << 2) | (c2 >> 5)
        out[oi++] = (c2 << 3) | (c3 >> 4)
        out[oi++] = (c3 << 4) | (c4 >> 3)
        out[oi++] = (c4 << 5) | (c5 >> 2)
        out[oi++] = (c5 << 6) | (c6 >> 1)
        out[oi++] = (c6 << 7) | c7
    }
    if (rem > 1) {
        let prev = input.charCodeAt(ii++)
        for (let r = 1; r < rem; r++) {
            const curr = input.charCodeAt(ii++)
            out[oi++] = (prev << r) | (curr >> (7 - r))
            prev = curr
        }
    }
    return out
}

/**
 * @param {string} input
 */
export function parseJSTemplateLiterals(input) {
    if (typeof input != 'string')
        throw TypeError("parseJSTemplateLiterals: input must be a string");

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
        if (c !== 32 && (c < 9 || c > 13) && c !== 160 && c !== 65279) throw SyntaxError(err)
    }
    for (; ; end--) {
        if (end === start) throw SyntaxError(err);
        const c = input.charCodeAt(end)
        // '`'
        if (c === 96) break
        if (c !== 32 && (c < 9 || c > 13) && c !== 160 && c !== 65279) throw SyntaxError(err)
    }

    var m
    var i = start + 1
    var out = ''
    const re = /[\\`$]/g
    re.lastIndex = i

    function hexVal(endIndex) {
        if (endIndex > end) throw SyntaxError(err);
        var val = 0
        for (; i < endIndex; i++) {
            const c = input.charCodeAt(i)
            // '0' - '9'
            if (c >= 48 && c <= 57) {
                val = (val << 4) | (c - 48)
            } else {
                const lower = c | 32
                // 'a' - 'f'
                if (lower < 97 || lower > 102) throw SyntaxError(err);
                val = (val << 4) | (lower - 87)
            }
        }
        return val
    }

    while (m = re.exec(input)) {
        const idx = m.index
        if (idx >= end) break

        if (idx > i) {
            out += input.slice(i, idx)
        }

        const c = input.charCodeAt(idx)
        // '`'
        if (c === 96) throw SyntaxError(err)
        // '$'
        if (c === 36) {
            // '{'
            if (idx + 1 < end && input.charCodeAt(idx + 1) === 123) throw SyntaxError(err)
            out += '$'
            i = idx + 1
            re.lastIndex = i
            continue
        }

        const nextIdx = idx + 1
        if (nextIdx >= end) throw SyntaxError(err)
        const afterNext = nextIdx + 1
        i = afterNext
        const next = input.charCodeAt(nextIdx)
        switch (next) {
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
            // '\\'
            case 92: out += '\\'; break
            // '`'
            case 96: out += '`'; break
            // "'"
            case 39: out += "'"; break
            // '"'
            case 34: out += '"'; break
            // '$'
            case 36: out += '$'; break
            // '0'
            case 48: {
                // '0' - '9'
                if (afterNext < end) {
                    const c = input.charCodeAt(afterNext)
                    if (c >= 48 && c <= 57) throw SyntaxError(err);
                }
                out += '\0'
                break
            }
            // 'x'
            case 120: {
                out += String.fromCharCode(hexVal(afterNext + 2))
                break
            }
            // 'u'
            case 117: a: {
                // '{'
                if (afterNext < end && input.charCodeAt(afterNext) === 123) {
                    const startHex = afterNext + 1
                    let cp = 0
                    const maxScan = Math.min(end, startHex + 7)
                    for (let k = startHex; k < maxScan; k++) {
                        const ch = input.charCodeAt(k)
                        // '}'
                        if (ch === 125) {
                            if (k === startHex || cp > 0x10FFFF) throw SyntaxError(err);
                            out += String.fromCodePoint(cp)
                            i = k + 1
                            break a
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
                    }
                    throw SyntaxError(err);
                }
                out += String.fromCharCode(hexVal(afterNext + 4))
                break
            }
            // '\r'
            case 13: {
                // '\n'
                if (afterNext < end && input.charCodeAt(afterNext) === 10)
                    i = afterNext + 1
                break
            }
            // '\n'
            case 10: break
            default:
                out += input[nextIdx]
                break
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