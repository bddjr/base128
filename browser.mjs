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
            /[\r\\`]|\0\d?|\$\{|<\/script/g,
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
                    default: return '<\\/script';
                }
            }
        ) + '`'
    }
}

/**
 * @param {Uint8Array} input
 */
export function encode(input) {
    var il = input.length
        , out = new Uint8Array(Math.ceil(il / 7 * 8))
        , ii = 0
        , oi = 0
    while (ii < il) {
        //     0        1        2        3        4        5        6        7
        // in  00000000 11111111 22222222 33333333 44444444 55555555 66666666
        // out _0000000 _0111111 _1122222 _2223333 _3333444 _4444455 _5555556 _6666666

        /* 0 */ out[oi++] = 127 & input[ii] >> 1
        /* 1 */ out[oi++] = 127 & (input[ii++] << 6 | input[ii] >> 2)
        /* 2 */ out[oi++] = 127 & (input[ii++] << 5 | input[ii] >> 3)
        /* 3 */ out[oi++] = 127 & (input[ii++] << 4 | input[ii] >> 4)
        /* 4 */ out[oi++] = 127 & (input[ii++] << 3 | input[ii] >> 5)
        /* 5 */ out[oi++] = 127 & (input[ii++] << 2 | input[ii] >> 6)
        /* 6 */ out[oi++] = 127 & (input[ii++] << 1 | input[ii] >> 7)
        /* 7 */ out[oi++] = 127 & input[ii++]
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
    var il = input.length
        , out = new Uint8Array(il / 8 * 7)
        , ii = 0
        , oi = 0
        , k
        , cache
        , next = _ => cache = input.charCodeAt(ii++)
    for (; ii < il; out[oi++] = cache << 8 - k | next() >> --k)
        k || next(k = 7);
    return out
}

export function parseJSTemplateLiterals(input) {
    if (typeof input != 'string')
        throw TypeError("parseJSTemplateLiterals: input must be a string");
    const err = "parseJSTemplateLiterals: invalid input"
    input = input.trim()
    if (input.length < 2)
        throw SyntaxError(err);
    if (input.charCodeAt(0) !== 96) // '`'
        throw SyntaxError(err);
    const loopMaxIndex = input.length - 1;
    if (input.charCodeAt(loopMaxIndex) !== 96) // '`'
        throw SyntaxError(err);
    var i = 1
    while (i < loopMaxIndex) {
        switch (input.charCodeAt(i)) {
            case 36: // '$'
                // anti '${'
                if (input.charCodeAt(++i) === 123) // '{'
                    throw SyntaxError(err);
                break
            case 92: // '\\'
                // ignore next char
                i += 2
                break
            case 96: // '`'
                throw SyntaxError(err);
            default:
                i++
        }
    }
    if (i !== loopMaxIndex)
        throw SyntaxError(err);
    return (0, eval)(input)
}

export default {
    EncodeResult,
    encode,
    decode,
    parseJSTemplateLiterals
}