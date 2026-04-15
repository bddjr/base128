export declare class EncodeResult {
    constructor(bytes: Uint8Array<ArrayBuffer>);

    /**
     * Returns a base128 string.
     */
    toString(): string;

    /**
     * Returns a base128 [Template literals](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals).
     */
    toJSTemplateLiterals(): string;

    bytes: Uint8Array<ArrayBuffer>;
}

export declare function encode(input: Uint8Array): EncodeResult;

export declare function decode(input: string): Uint8Array<ArrayBuffer>;

declare const base128: {
    EncodeResult: typeof EncodeResult,
    encode: typeof encode,
    decode: typeof decode,
};

export default base128;
