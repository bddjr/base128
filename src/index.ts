export function encode(input: Uint8Array) {
    let out = new Uint8Array(Math.ceil(input.length / 7) * 8 + 1)

    const suffixSize = input.length % 7
    out[0] = suffixSize + 48

    for (let ii = 0, oi = 1; ii < input.length;) {
        let chunk = BigInt(input[ii++] || 0) << 48n
        chunk |= BigInt(input[ii++] || 0) << 40n
        chunk |= BigInt(input[ii++] || 0) << 32n
        chunk |= BigInt(input[ii++] || 0) << 24n
        chunk |= BigInt(input[ii++] || 0) << 16n
        chunk |= BigInt(input[ii++] || 0) << 8n
        chunk |= BigInt(input[ii++] || 0)

        out[oi++] = Number(chunk >> 49n)
        out[oi++] = Number((chunk >> 42n) & 127n)
        out[oi++] = Number((chunk >> 35n) & 127n)
        out[oi++] = Number((chunk >> 28n) & 127n)
        out[oi++] = Number((chunk >> 21n) & 127n)
        out[oi++] = Number((chunk >> 14n) & 127n)
        out[oi++] = Number((chunk >> 7n) & 127n)
        out[oi++] = Number(chunk & 127n)
    }

    if (suffixSize) out = out.slice(0, Math.ceil(suffixSize / 7 * 8) - 8)
    let str: string | undefined
    let jstl: string | undefined
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
    const u8a = new TextEncoder().encode(input)
    const out = new Uint8Array(Math.ceil((u8a.length - 1) / 8) * 7)
    for (let ii = 1, oi = 0; ii < u8a.length;) {
        let chunk = BigInt(u8a[ii++] || 0) << 49n
        chunk |= BigInt(u8a[ii++] || 0) << 42n
        chunk |= BigInt(u8a[ii++] || 0) << 35n
        chunk |= BigInt(u8a[ii++] || 0) << 28n
        chunk |= BigInt(u8a[ii++] || 0) << 21n
        chunk |= BigInt(u8a[ii++] || 0) << 14n
        chunk |= BigInt(u8a[ii++] || 0) << 7n
        chunk |= BigInt(u8a[ii++] || 0)

        out[oi++] = Number(chunk >> 48n)
        out[oi++] = Number((chunk >> 40n) & 255n)
        out[oi++] = Number((chunk >> 32n) & 255n)
        out[oi++] = Number((chunk >> 24n) & 255n)
        out[oi++] = Number((chunk >> 16n) & 255n)
        out[oi++] = Number((chunk >> 8n) & 255n)
        out[oi++] = Number(chunk & 255n)
    }
    const suffixSize = u8a[0] - 48
    if (suffixSize) return out.slice(0, suffixSize - 7)
    return out
}

const base128 = {
    encode,
    decode
}

export default base128