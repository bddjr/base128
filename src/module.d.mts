export declare class EncodeOutput {
    constructor(out: Uint8Array);
    uint8Array: Uint8Array;
    toString(): string;
    toJSTemplateLiterals(): string;
}
export declare function encode(input: Uint8Array | string): EncodeOutput;
export declare function decode(input: string): Uint8Array;
declare const base128: {
    EncodeOutput: typeof EncodeOutput;
    encode: typeof encode;
    decode: typeof decode;
};
export default base128;
