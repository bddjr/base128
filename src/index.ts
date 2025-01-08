export function encode(input: Uint8Array) {
    let ii = 0, oi = 0,
        str: string | undefined,
        jstl: string | undefined;
    const out = new Uint8Array(Math.ceil(input.length / 7 * 8))
    while (ii < input.length) {
        //     0        1        2        3        4        5        6        7
        // in  00000000 11111111 22222222 33333333 44444444 55555555 66666666
        // out _0000000 _0111111 _1122222 _2223333 _3333444 _4444455 _5555556 _6666666

        /* 0 */ out[oi++] = input[ii] >> 1
        /* 1 */ out[oi++] = input[ii++] << 6 & 127 | input[ii] >> 2
        /* 2 */ out[oi++] = input[ii++] << 5 & 127 | input[ii] >> 3
        /* 3 */ out[oi++] = input[ii++] << 4 & 127 | input[ii] >> 4
        /* 4 */ out[oi++] = input[ii++] << 3 & 127 | input[ii] >> 5
        /* 5 */ out[oi++] = input[ii++] << 2 & 127 | input[ii] >> 6
        /* 6 */ out[oi++] = input[ii++] << 1 & 127 | input[ii] >> 7
        /* 7 */ out[oi++] = input[ii++] & 127
    }

    return {
        uint8Array: out,
        toString() {
            return str ??= new TextDecoder().decode(out)
        },
        toJSTemplateLiterals() {
            return jstl ??= '`' + (this.toString() as string).replace(/[\r\\`]|\${|\0\d?|<\/script/g, (match) => {
                if (match == '\r')
                    return '\\r'
                if (match == '</script')
                    return '<\\/script'
                if (match[0] == '\0') {
                    if (match.length == 2)
                        return '\\x00' + match[1]
                    return '\\0'
                }
                return '\\' + match
            }) + '`'
        }
    }
}

export function decode(input: string) {
    let ii = 0, oi = 0, cache: number
    const out = new Uint8Array(Math.floor(input.length / 8 * 7))
    function update() {
        return cache = input.charCodeAt(ii++) || 0
    }
    while (ii < input.length) {
        //     0        1        2        3        4        5        6        7
        // in  _0000000 _1111111 _2222222 _3333333 _4444444 _5555555 _6666666 _7777777
        // out 00000001 11111122 22222333 33334444 44455555 55666666 67777777

        /* 0 */ out[oi++] = update() << 1 | update() >> 6
        /* 1 */ out[oi++] = cache << 2 | update() >> 5
        /* 2 */ out[oi++] = cache << 3 | update() >> 4
        /* 3 */ out[oi++] = cache << 4 | update() >> 3
        /* 4 */ out[oi++] = cache << 5 | update() >> 2
        /* 5 */ out[oi++] = cache << 6 | update() >> 1
        /* 6 */ out[oi++] = cache << 7 | update()
    }
    return out
}

const base128 = {
    encode,
    decode
}

export default base128