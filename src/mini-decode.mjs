/**
 * @param {string} input
 */
export var decode = (input) => {
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
