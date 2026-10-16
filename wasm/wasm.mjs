// base128-ascii (The Unlicense)

const memory = new WebAssembly.Memory({ initial: 2 })
const response = fetch(new URL('./base128.wasm', import.meta.url))

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
} = /** @type {*} */ (
        (await (
            WebAssembly.instantiateStreaming
                ? WebAssembly.instantiateStreaming(response, { env: { memory } })
                : response.then(r => r.arrayBuffer()).then(b => WebAssembly.instantiate(b, { env: { memory } }))
        )).instance.exports
    )

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
 * Writes string or Uint8Array to WASM memory at offset.
 * @param {string | Uint8Array | Uint8ClampedArray} input
 * @param {number} inOffset
 * @param {number} maxInBytes
 */
function writeInput(input, inOffset, maxInBytes) {
    if (typeof input == 'string') {
        return textEncoder.encodeInto(input, new Uint8Array(memory.buffer, inOffset, maxInBytes)).written
    }
    new Uint8Array(memory.buffer, inOffset, input.length).set(input)
    return input.length
}

/**
 * 16-byte alignment without branch and without truncating to int32
 * @param {number} n
 */
function align16(n) {
    return n + (-n & 15)
}

/** @type {WeakRef<EncodeResult> | EncodeResult | null} */
let lastPendingResult = null

function getPendingResult() {
    return lastPendingResult
        ? typeof (/** @type {*} */ (lastPendingResult)).deref == 'function'
            ? (/** @type {WeakRef<EncodeResult>} */ (lastPendingResult)).deref()
            : (/** @type {EncodeResult} */ (lastPendingResult))
        : null
}

function materializePending() {
    if (lastPendingResult !== null) {
        const res = getPendingResult()
        if (res && res._wasmOffset !== undefined) {
            res.bytes
        }
        lastPendingResult = null
    }
}

export class EncodeResult {
    /**
     * @param {Uint8Array<ArrayBuffer> | null} bytes
     * @param {number} [wasmOffset]
     * @param {number} [wasmLen]
     */
    constructor(bytes, wasmOffset, wasmLen) {
        if (wasmOffset !== undefined) {
            /** @type {number | undefined} */
            this._wasmOffset = wasmOffset
            /** @type {number | undefined} */
            this._wasmLen = wasmLen
            /** @param {Uint8Array<ArrayBuffer>} val */
            const defineValue = (val) => {
                Object.defineProperty(this, 'bytes', {
                    configurable: true,
                    enumerable: true,
                    writable: true,
                    value: val
                })
                if (getPendingResult() === this) lastPendingResult = null
                delete this._wasmOffset
                delete this._wasmLen
                return val
            }
            Object.defineProperty(this, 'bytes', {
                configurable: true,
                enumerable: true,
                get: () => defineValue(
                    this._wasmOffset !== undefined && this._wasmLen !== undefined
                        ? new Uint8Array(memory.buffer, this._wasmOffset, this._wasmLen).slice()
                        : new Uint8Array()
                ),
                set: defineValue
            })
        } else {
            this.bytes = bytes
        }
    }
    toString() {
        // Intentionally do not check the type of `this` or `this.bytes` here.
        return textDecoder.decode(
            this === getPendingResult() && this._wasmOffset !== undefined
                ? new Uint8Array(memory.buffer, this._wasmOffset, this._wasmLen)
                : this.bytes
        )
    }
    toJSTemplateLiterals() {
        if (this === getPendingResult() && this._wasmOffset !== undefined && this._wasmLen !== undefined) {
            return this._wasmLen === 0 ? '``' : toJSTemplateLiteralsFromMemory(this._wasmOffset, this._wasmLen)
        }

        materializePending()

        // Intentionally do not check the type of `this` or `this.bytes` here.
        const input = this.bytes ?? this.toString()
        const inLen = input.length
        if (!inLen) return '``'

        const isStr = typeof input == 'string'
        const maxInBytes = isStr ? inLen * 3 : inLen
        const outOffset = align16(heapBase + maxInBytes)
        ensureCapacity(outOffset + maxInBytes * 5 + 16)

        const actualInBytes = writeInput(input, heapBase, maxInBytes)
        return toJSTemplateLiteralsFromMemory(heapBase, actualInBytes)
    }
}

/**
 * @param {number} inOffset
 * @param {number} inLen
 */
function toJSTemplateLiteralsFromMemory(inOffset, inLen) {
    const maxOutBytes = inLen * 5 + 16
    const outOffset = align16(inOffset + inLen)
    ensureCapacity(outOffset + maxOutBytes)
    const outLen = base128wasm_to_js_template_literals(inOffset, inLen, outOffset)
    return textDecoder.decode(new Uint8Array(memory.buffer, outOffset, outLen))
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
    if (!inLen) return new EncodeResult(new Uint8Array())

    materializePending()

    const inOffset = heapBase
    const maxInBytes = isStr ? inLen * 3 : inLen
    const outOffset = align16(inOffset + maxInBytes)
    ensureCapacity(outOffset + Math.ceil(maxInBytes / 0.875) + 16)

    const actualInBytes = writeInput(input, inOffset, maxInBytes)
    const actualOutOffset = align16(inOffset + actualInBytes)
    ensureCapacity(actualOutOffset + Math.ceil(actualInBytes / 0.875) + 16)

    const outLen = base128wasm_encode(inOffset, actualInBytes, actualOutOffset)
    const result = new EncodeResult(null, actualOutOffset, outLen)
    lastPendingResult = typeof WeakRef == 'function' ? new WeakRef(result) : result
    return result
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function decode(input) {
    const isStr = typeof input == 'string'
    if (!isStr && !isUint8Array(input))
        throw TypeError("decode: input must be a string, Uint8Array or Uint8ClampedArray")
    const inLen = input.length
    if (!inLen) return new Uint8Array()
    if ((inLen & 7) === 1) throw SyntaxError("decode: invalid input")

    materializePending()

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
    if (outLen < 0) throw SyntaxError("decode: invalid input")

    return new Uint8Array(memory.buffer, outOffset, outLen).slice()
}

/**
 * @param {string | Uint8Array | Uint8ClampedArray} input
 */
export function parseJSTemplateLiterals(input) {
    const isStr = typeof input == 'string'
    if (!isStr && !isUint8Array(input))
        throw TypeError("parseJSTemplateLiterals: input must be a string, Uint8Array or Uint8ClampedArray")
    const inLen = input.length
    if (inLen < 2) throw SyntaxError("parseJSTemplateLiterals: invalid input")

    materializePending()

    const inOffset = heapBase
    const maxInBytes = isStr ? inLen * 3 : inLen
    const outOffset = align16(inOffset + maxInBytes)
    ensureCapacity(outOffset + maxInBytes + 16)

    const actualInBytes = writeInput(input, inOffset, maxInBytes)
    const actualOutOffset = align16(inOffset + actualInBytes)
    ensureCapacity(actualOutOffset + actualInBytes + 16)

    const outLen = base128wasm_parse_js_template_literals(inOffset, actualInBytes, actualOutOffset)
    if (outLen < 0) throw SyntaxError("parseJSTemplateLiterals: invalid input")

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
