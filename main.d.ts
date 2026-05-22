interface EncodeResultPrototype {
    /**
     * Returns a base128 string.
     */
    toString(): string;

    /**
     * Returns a base128 [Template literals](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals).
     */
    toJSTemplateLiterals(): string;
}

interface EncodeResult extends EncodeResultPrototype {
    bytes: Uint8Array<ArrayBuffer>;
}

export declare var EncodeResult: {
    new(bytes: Uint8Array<ArrayBuffer>): EncodeResult;
    prototype: EncodeResultPrototype;
};

export declare function encode(input: Uint8Array): EncodeResult;

export declare function decode(input: string): Uint8Array<ArrayBuffer>;

declare const base128: {
    EncodeResult: typeof EncodeResult,
    encode: typeof encode,
    decode: typeof decode,
};

export default base128;
