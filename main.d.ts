export declare class Base128Bytes {
    constructor();
    constructor(length: number);
    constructor(array: ArrayLike<number>);
    constructor<TArrayBuffer extends ArrayBufferLike = ArrayBuffer>(buffer: TArrayBuffer, byteOffset?: number, length?: number);
    constructor(buffer: ArrayBuffer, byteOffset?: number, length?: number);
    constructor(array: ArrayLike<number> | ArrayBuffer);

    /**
     * Returns a base128 string.
     */
    toString(): string;

    /**
     * Returns a base128 [Template literals](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals).
     */
    toJSTemplateLiterals(): string;

    /**
     * The ArrayBuffer instance referenced by the array.
     */
    get buffer(): ArrayBufferLike;

    /**
     * The length in bytes of the array.
     */
    get byteLength(): number;

    /**
     * The offset in bytes of the array.
     */
    get byteOffset(): number;

    /**
     * The length of the array.
     */
    get length(): number;

    [index: number]: number;
}

export declare function encode(input: Uint8Array | string): Base128Bytes;

export declare function decode(input: string): Uint8Array;

declare const base128 = {
    Base128Bytes,
    encode,
    decode,
};

export default base128;
