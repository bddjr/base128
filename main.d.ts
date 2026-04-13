export declare class Base128Bytes extends Uint8Array {
    /**
     * Returns a base128 string.
     */
    toString(): string;
    /**
     * Returns a base128 [Template literals](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Template_literals).
     */
    toJSTemplateLiterals(): string;
}
export declare function encode(input: Uint8Array | string): Base128Bytes;
export declare function decode(input: string): Uint8Array;
declare const base128: {
    Base128Bytes: typeof Base128Bytes;
    encode: typeof encode;
    decode: typeof decode;
};
export default base128;
