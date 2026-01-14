declare interface Window {
    base128: object
}

type Uint8ArrayLike = ArrayLike<number>

interface this_uint8Array {
    uint8Array: Uint8Array
}

{
    class EncodeOutput {
        constructor(out: Uint8Array) {
            (this as unknown as this_uint8Array).uint8Array = out
        }
        toString() {
            return new TextDecoder().decode((this as unknown as this_uint8Array).uint8Array)
        }
        toJSTemplateLiterals() {
            return `\`${this.toString().replace(
                /[\r\\`]|\${|<\/script/g,
                (match) => (
                    match == '\r'
                        ? '\\r'
                        : match == '</script'
                            ? '<\\/script'
                            : '\\' + match
                )
            )}\``
        }
    }

    self.base128 = {
        EncodeOutput,
        encode(input: Uint8ArrayLike | string): EncodeOutput {
            if (typeof input == 'string')
                input = new TextEncoder().encode(input)
            var il = input.length
                , out = new Uint8Array(Math.ceil(il / 7 * 8))
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
            return new EncodeOutput(out)
        },
        decode(input: string): Uint8Array {
            var il = input.length
                , out = new Uint8Array(Math.floor(il / 8 * 7))
                , ii = 0
                , oi = 0
                , cache: number
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
    }
}