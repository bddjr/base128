let _bytesToStr = (
    (
        typeof Buffer == 'function' &&
        Buffer.prototype &&
        typeof Buffer.prototype.latin1Slice == 'function' &&
        // Exclude Deno. See https://github.com/bddjr/base128/pull/4
        typeof Deno == 'undefined'
    )
        ? (bytes) => Buffer.prototype.latin1Slice.call(bytes)
        : (bytes) => {
            // TextDecoder keeps the default UTF-8, which is already the fastest.
            const td = new TextDecoder
            return (_bytesToStr = (bytes) => td.decode(bytes))(bytes)
        }
)

export class EncodeResult {
    /**
     * @param {Uint8Array<ArrayBuffer>} bytes 
     */
    constructor(bytes) {
        this.bytes = bytes
    }
    toString() {
        return _bytesToStr(this.bytes)
    }
    toJSTemplateLiterals() {
        return `\`${this.toString().replace(
            /[\r\\`]|\$\{|<\/script/g,
            (match) => (
                match === '\r'
                    ? '\\r'
                    : match === '</script'
                        ? '<\\/script'
                        : '\\' + match
            )
        )}\``
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
    var il = input.length
        , out = new Uint8Array(il / 8 * 7)
        , ii = 0
        , oi = 0
        , k
        , cache
        , next = _ =>
            (cache = input.charCodeAt(ii++)) >> 7
                ? cache = 0 // In HTML, 0 is likely to be converted to 65533 (�)
                : cache
    while (ii < il) {
        //     0        1        2        3        4        5        6        7
        // in  _0000000 _1111111 _2222222 _3333333 _4444444 _5555555 _6666666 _7777777
        // out 00000001 11111122 22222333 33334444 44455555 55666666 67777777
        k || next(k = 7)
        out[oi++] = cache << 8 - k | next() >> --k
    }
    return out
}

export default {
    EncodeResult,
    encode,
    decode
}