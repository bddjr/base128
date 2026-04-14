//@ts-nocheck

export class EncodeResult {
    /**
     * @param {Uint8Array} bytes 
     */
    constructor(bytes) {
        if (!(bytes instanceof Uint8Array)) {
            throw TypeError(`EncodeResult: Must input Uint8Array`)
        }
        Object.defineProperty(this, "bytes", { value: bytes, enumerable: true })
    }
    toString() {
        return new TextDecoder().decode(this.bytes)
    }
    toJSTemplateLiterals() {
        return `\`${this.toString().replace(
            /[\r\\`]|\$\{|<\/script/g,
            (match) => (
                match == '\r'
                    ? '\\r'
                    : match == '</script'
                        ? '<\\/script'
                        : '\\' + match
            )
        )}\``
    }
    get buffer() {
        return this.bytes.buffer
    }
}

/**
 * @param {Uint8Array | string | ArrayLike<number> | ArrayBuffer | Pick<ArrayBufferView, "buffer">} input
 */
export function encode(input) {
    if (input == null) {
        throw TypeError(`encode: Cannot input null or undefined`)
    }
    if (input instanceof Uint8Array) {
        // Uint8Array | Buffer
    } else if (typeof input == 'string') {
        // string
        input = new TextEncoder().encode(input)
    } else if (input instanceof ArrayBuffer) {
        // ArrayBuffer
        input = new Uint8Array(input)
    } else if (input.buffer instanceof ArrayBuffer) {
        // TypedArray | DataView | Pick<ArrayBufferView, "buffer">
        input = new Uint8Array(input.buffer)
    }
    // else ArrayLike<number>
    var il = input.length
    if (typeof il != 'number') {
        throw TypeError(`encode: typeof input.length must be number`)
    }
    var out = new Uint8Array(Math.ceil(il / 7 * 8))
        , ii = 0
        , oi = 0
    while (ii < il) {
        //     0        1        2        3        4        5        6        7
        // in  00000000 11111111 22222222 33333333 44444444 55555555 66666666
        // out _0000000 _0111111 _1122222 _2223333 _3333444 _4444455 _5555556 _6666666

        /* 0 */ out[oi++] = input[ii] >> 1 & 127
        /* 1 */ out[oi++] = (input[ii++] << 6 | input[ii] >> 2) & 127
        /* 2 */ out[oi++] = (input[ii++] << 5 | input[ii] >> 3) & 127
        /* 3 */ out[oi++] = (input[ii++] << 4 | input[ii] >> 4) & 127
        /* 4 */ out[oi++] = (input[ii++] << 3 | input[ii] >> 5) & 127
        /* 5 */ out[oi++] = (input[ii++] << 2 | input[ii] >> 6) & 127
        /* 6 */ out[oi++] = (input[ii++] << 1 | input[ii] >> 7) & 127
        /* 7 */ out[oi++] = input[ii++] & 127
    }
    return new EncodeResult(out)
}

/**
 * @param {string} input
 */
export function decode(input) {
    if (typeof input != 'string') {
        throw TypeError(`decode: Must input string`)
    }
    var il = input.length
        , out = new Uint8Array(il / 8 * 7)
        , ii = 0
        , oi = 0
        , cache
        , next = () => (
            (cache = input.charCodeAt(ii++)) > 127
                ? cache = 0 // In HTML, 0 is likely to be converted to 65533 (�)
                : cache
        )
    while (ii < il) {
        //     0        1        2        3        4        5        6        7
        // in  _0000000 _1111111 _2222222 _3333333 _4444444 _5555555 _6666666 _7777777
        // out 00000001 11111122 22222333 33334444 44455555 55666666 67777777

        /* 0 */ out[oi++] = next() << 1 | next() >> 6
        /* 1 */ out[oi++] = cache << 2 | next() >> 5
        /* 2 */ out[oi++] = cache << 3 | next() >> 4
        /* 3 */ out[oi++] = cache << 4 | next() >> 3
        /* 4 */ out[oi++] = cache << 5 | next() >> 2
        /* 5 */ out[oi++] = cache << 6 | next() >> 1
        /* 6 */ out[oi++] = cache << 7 | next()
    }
    return out
}

export default {
    EncodeResult,
    encode,
    decode
}