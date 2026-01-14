declare class EncodeOutput {
    constructor(out: Uint8Array);
    uint8Array: Uint8Array;
    toString(): string;
    toJSTemplateLiterals(): string;
}
declare const base128: {
    EncodeOutput: typeof EncodeOutput;
    encode(input: Uint8Array | string): EncodeOutput;
    decode(input: string): Uint8Array;
};
export = base128;
