export interface EncodeResultPrototype {
    /**
     * Converts the Base128 encoded 7-bit bytes into an ASCII string
     * where each character's code point is in the range of 0–127.
     *
     * @returns Base128 ASCII string.
     */
    toString(): string;

    /**
     * Formats the Base128 encoded bytes into a valid JavaScript
     * [Template literal](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals) enclosed in backticks (`` `...` ``).
     *
     * Special characters such as `\r`, `\`, `` ` ``, `\0` (`\x000`–`\x009`), `${`,
     * and `</script` are safely escaped so the resulting string can be directly embedded
     * into JavaScript or HTML source files without syntax errors or injection risks.
     *
     * @returns A safely escaped JavaScript template literal string.
     */
    toJSTemplateLiterals(): string;
}

export interface EncodeResult extends EncodeResultPrototype {
    /**
     * Base128 encoded bytes. Each element is a 7-bit integer in the range of 0–127 (0x00–0x7F).
     */
    bytes: Uint8Array<ArrayBuffer>;
}

export declare var EncodeResult: {
    /**
     * Creates an `EncodeResult` instance wrapping the given Base128 7-bit byte array.
     *
     * @param bytes Base128 encoded bytes (range 0–127).
     */
    new(bytes: Uint8Array<ArrayBuffer>): EncodeResult;
    prototype: EncodeResultPrototype;
};

/**
 * Encodes binary data or a string into Base128.
 *
 * Base128 encodes every 7 bytes (56 bits) of input into 8 bytes (7 bits each, in range 0–127).
 * The resulting payload size is ~12.5% smaller than standard Base64.
 *
 * If `input` is a string, it will be automatically UTF-8 encoded before Base128 encoding.
 * In environments supporting V8 acceleration (`_impl === 'v8'`), ASCII strings benefit from
 * zero-copy acceleration directly from the engine's internal string representation.
 *
 * @param input The data to encode: a UTF-8 string, `Uint8Array`, or `Uint8ClampedArray`.
 * @returns An `EncodeResult` object containing the encoded 7-bit bytes, with methods to convert to ASCII string or JS template literals.
 * @throws {TypeError} If `input` is not a string, `Uint8Array`, or `Uint8ClampedArray`.
 */
export declare function encode(input: string | Uint8Array | Uint8ClampedArray): EncodeResult;

/**
 * Decodes a Base128 encoded string or byte array back to original binary data (`Uint8Array`).
 *
 * Unpacks 8 bytes of Base128 (7 bits each) into 7 bytes of raw binary data (8 bits each).
 *
 * @param input Base128 encoded data: an ASCII string, `Uint8Array`, or `Uint8ClampedArray` where each byte is in range 0–127.
 * @returns The decoded raw binary bytes as a `Uint8Array`.
 * @throws {TypeError} If `input` is not a string, `Uint8Array`, or `Uint8ClampedArray`.
 * @throws {SyntaxError} If `input` contains invalid characters (byte >= 128) or an invalid remainder length (such as ending with a single remainder character).
 */
export declare function decode(input: string | Uint8Array | Uint8ClampedArray): Uint8Array<ArrayBuffer>;

/**
 * Safely parses and unescapes a JavaScript [Template literal](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals)
 * into an unescaped Base128 ASCII string without using `eval` or executing code.
 *
 * Reverses the escaping produced by {@link EncodeResultPrototype.toJSTemplateLiterals}, handling escape sequences
 * such as `\r`, `\\`, `` ` ``, `\0`, `\x000`–`\x009`, `\${`, and `<\/script`.
 * Leading and trailing backticks (`` ` ``) are automatically stripped if present.
 *
 * @param input The template literal to parse: a string, `Uint8Array`, or `Uint8ClampedArray`.
 * @returns The unescaped Base128 string, suitable for passing directly to {@link decode}.
 * @throws {TypeError} If `input` is not a string, `Uint8Array`, or `Uint8ClampedArray`.
 * @throws {SyntaxError} If the template literal syntax is invalid (e.g. unclosed backtick or invalid escape sequence).
 */
export declare function parseJSTemplateLiterals(input: string | Uint8Array | Uint8ClampedArray): string;

/**
 * The underlying active implementation in the current runtime:
 * - `'v8'`: High-performance native addon for Node.js using V8 C++ internal APIs and SIMD/BMI2 acceleration.
 * - `'napi'`: Node-API native addon providing ABI stability in non-V8 runtimes such as Bun or Deno.
 * - `'wasm'`: WebAssembly (WASM) SWAR-accelerated implementation for browsers and modern JS runtimes.
 * - `'js'`: Pure JavaScript portable fallback that runs in all web browsers and JS environments.
 */
export declare const _impl: 'v8' | 'napi' | 'wasm' | 'js';

declare namespace Base128 {
    export { _impl, EncodeResult, encode, decode, parseJSTemplateLiterals };
}

export default Base128;
