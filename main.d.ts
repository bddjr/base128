export declare class EncodeResult {
    constructor(bytes: Uint8Array);

    /**
     * Returns a base128 string.
     */
    toString(): string;

    /**
     * Returns a base128 [Template literals](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals).
     */
    toJSTemplateLiterals(): string;

    get buffer(): ArrayBufferLike;

    readonly bytes: Uint8Array;
}

export declare function encode(input: Uint8Array | string | ArrayLike<number> | ArrayBuffer | Pick<ArrayBufferView, "buffer">): EncodeResult;

export declare function decode(input: string): Uint8Array;

declare const base128: {
    EncodeResult: typeof EncodeResult,
    encode: typeof encode,
    decode: typeof decode,
};

export default base128;
