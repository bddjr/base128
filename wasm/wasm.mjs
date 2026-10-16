// base128-ascii (The Unlicense)

const wasmUrl = new URL('./base128.wasm', import.meta.url)
const memory = new WebAssembly.Memory({ initial: 2 })
const importObject = { env: { memory } }

const response = fetch(wasmUrl)
/** @type {WebAssembly.WebAssemblyInstantiatedSource} */
let wis
if (WebAssembly.instantiateStreaming) {
    wis = await WebAssembly.instantiateStreaming(response, importObject)
} else {
    wis = await response
        .then(r => r.arrayBuffer())
        .then(bytes => WebAssembly.instantiate(bytes, importObject))
}

/**
 * @type {{
 *     base128wasm_encode: (inPtr: number, inLen: number, outPtr: number) => number,
 *     base128wasm_decode: (inPtr: number, inLen: number, outPtr: number) => number,
 *     base128wasm_to_js_template_literals: (inPtr: number, inLen: number, outPtr: number) => number,
 *     base128wasm_parse_js_template_literals: (inPtr: number, inLen: number, outPtr: number) => number,
 *     __heap_base?: { value: number }
 * }}
 */
const {
    base128wasm_encode,
    base128wasm_decode,
    base128wasm_to_js_template_literals,
    base128wasm_parse_js_template_literals,
    __heap_base
} = /** @type {*} */ (wis.instance.exports)

const heapBase = __heap_base ? __heap_base.value : 65536
const textEncoder = new TextEncoder()
const textDecoder = new TextDecoder()

/**
 * @param {any} input
 */
function isUint8Array(input) {
    // Use Symbol.toStringTag to support recognizing objects from iframes
    if (input) switch (input[Symbol.toStringTag]) {
        case "Uint8Array":
        case "Uint8ClampedArray":
            return true
    }
    return false
}

/**
 * 16-byte alignment without branch and without truncating to int32
 * @param {number} n
 */
function align16(n) {
    return n + (-n & 15)
}

export class EncodeResult {
    /**
     * @param {Uint8Array<ArrayBuffer>} bytes
     */
    constructor(bytes) {
        this.bytes = bytes
    }
    toString() {
        return textDecoder.decode(this.bytes)
    }
    toJSTemplateLiterals() {
        var bytes = this ? this.bytes : undefined
        if (isUint8Array(bytes)) {
            const inLen = bytes.length
            if (inLen === 0) return '``'

            const inOffset = heapBase
            const maxOutBytes = inLen * 5 + 16
            const outOffset = align16(inOffset + inLen)
            ensureCapacity(outOffset + maxOutBytes)

            new Uint8Array(memory.buffer, inOffset, inLen).set(bytes)
            const outLen = base128wasm_to_js_template_literals(inOffset, inLen, outOffset)
            return textDecoder.decode(new Uint8Array(memory.buffer, outOffset, outLen))
        }

        const str = typeof this === 'string' ? this : this?.toString()
        if (typeof str !== 'string') {
            throw TypeError("EncodeResult.prototype.toJSTemplateLiterals: 'this.bytes' must be a Uint8Array or Uint8ClampedArray")
        }
        const strLen = str.length
        if (strLen === 0) return '``'

        const inOffset = heapBase
        const maxInBytes = strLen * 3
        const maxOutBytes = maxInBytes * 5 + 16
        const outOffset = align16(inOffset + maxInBytes)
        ensureCapacity(outOffset + maxOutBytes)

        const res = textEncoder.encodeInto(str, new Uint8Array(memory.buffer, inOffset, maxInBytes))
        const actualInBytes = res.written
        const actualOutOffset = align16(inOffset + actualInBytes)
        ensureCapacity(actualOutOffset + actualInBytes * 5 + 16)

        const outLen = base128wasm_to_js_template_literals(inOffset, actualInBytes, actualOutOffset)
        return textDecoder.decode(new Uint8Array(memory.buffer, actualOutOffset, outLen))
    }
}

/**
 * Ensures WASM linear memory has at least `neededBytes`.
 * @param {number} neededBytes
 */
function ensureCapacity(neededBytes) {
    const currentBytes = memory.buffer.byteLength
    if (currentBytes < neededBytes) {
        const neededPages = Math.ceil((neededBytes - currentBytes) / 65536)
        memory.grow(neededPages)
    }
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function encode(input) {
    const isStr = typeof input == 'string'
    if (!isStr && !isUint8Array(input))
        throw TypeError("encode: input must be a string, Uint8Array or Uint8ClampedArray")
    const inLen = input.length

    if (inLen === 0) {
        return new EncodeResult(new Uint8Array(0))
    }

    const inOffset = heapBase
    const maxInBytes = isStr ? inLen * 3 : inLen
    const maxOutBytes = Math.ceil(maxInBytes / 0.875) + 16
    const outOffset = align16(inOffset + maxInBytes)
    ensureCapacity(outOffset + maxOutBytes)

    var actualInBytes = inLen
    if (isStr) {
        const res = textEncoder.encodeInto(input, new Uint8Array(memory.buffer, inOffset, maxInBytes))
        actualInBytes = res.written
    } else {
        new Uint8Array(memory.buffer, inOffset, inLen).set(input)
    }

    const actualOutOffset = align16(inOffset + actualInBytes)
    ensureCapacity(actualOutOffset + Math.ceil(actualInBytes / 0.875) + 16)

    const outLen = base128wasm_encode(inOffset, actualInBytes, actualOutOffset)
    const outBytes = new Uint8Array(memory.buffer, actualOutOffset, outLen).slice()
    return new EncodeResult(outBytes)
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function decode(input) {
    const isStr = typeof input === 'string'
    if (!isStr && !isUint8Array(input))
        throw TypeError("decode: input must be a string, Uint8Array or Uint8ClampedArray")
    const inLen = input.length

    if (inLen === 0) {
        return new Uint8Array(0)
    }

    if ((inLen & 7) === 1) {
        throw SyntaxError("decode: invalid input")
    }

    const inOffset = heapBase
    const outOffset = align16(inOffset + inLen)
    const maxOutBytes = inLen + 16
    ensureCapacity(outOffset + maxOutBytes)

    if (isStr) {
        const res = textEncoder.encodeInto(input, new Uint8Array(memory.buffer, inOffset, inLen))
        if (res.read !== inLen || res.written !== inLen) {
            throw SyntaxError("decode: invalid input")
        }
    } else {
        new Uint8Array(memory.buffer, inOffset, inLen).set(input)
    }

    const outLen = base128wasm_decode(inOffset, inLen, outOffset)
    if (outLen < 0) {
        throw SyntaxError("decode: invalid input")
    }

    return new Uint8Array(memory.buffer, outOffset, outLen).slice()
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function parseJSTemplateLiterals(input) {
    const isStr = typeof input === 'string'
    if (!isStr && !isUint8Array(input))
        throw TypeError("parseJSTemplateLiterals: input must be a string, Uint8Array or Uint8ClampedArray")
    const inLen = input.length
    if (inLen < 2) {
        throw SyntaxError("parseJSTemplateLiterals: invalid input")
    }

    const inOffset = heapBase
    const maxInBytes = isStr ? inLen * 3 : inLen
    const outOffset = align16(inOffset + maxInBytes)
    const maxOutBytes = maxInBytes + 16
    ensureCapacity(outOffset + maxOutBytes)

    var actualInBytes = inLen
    if (isStr) {
        const res = textEncoder.encodeInto(input, new Uint8Array(memory.buffer, inOffset, maxInBytes))
        actualInBytes = res.written
    } else {
        new Uint8Array(memory.buffer, inOffset, inLen).set(input)
    }

    const actualOutOffset = align16(inOffset + actualInBytes)
    ensureCapacity(actualOutOffset + actualInBytes + 16)

    const outLen = base128wasm_parse_js_template_literals(inOffset, actualInBytes, actualOutOffset)
    if (outLen < 0) {
        throw SyntaxError("parseJSTemplateLiterals: invalid input")
    }

    return textDecoder.decode(new Uint8Array(memory.buffer, actualOutOffset, outLen))
}

export const _impl = 'wasm'

export default {
    _impl,
    EncodeResult,
    encode,
    decode,
    parseJSTemplateLiterals
}
