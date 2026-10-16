//@ts-check

import Base128 from "base128-ascii";
import browserBase128 from "../src/browser.mjs";
import { decode as miniDecode } from "../src/mini-decode.mjs";
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";

const enableOutput = false

const inputDir = "testdata/input"
const outputDir = "testdata/output"

fs.rmSync(outputDir, { recursive: true, force: true })

if (enableOutput && !fs.existsSync(outputDir))
    fs.mkdirSync(outputDir)

let allSuccess = true

// Verify that the imported Base128 is indeed a native implementation
const nativeEncodeStr = String(Base128.encode);
if (!nativeEncodeStr.includes("[native code]")) {
    throw new Error(`Test failed: Base128.encode is not a native implementation!\nGot: ${nativeEncodeStr}`);
}

const require = createRequire(import.meta.url);

// Determine which binary main.cjs imported
const isV8 = Base128._impl === "v8";
const currentBindingName = isV8 ? "v8.node" : "napi.node";

/**
 * @returns {typeof Base128 | undefined}
 */
function loadExtraBinding(filename) {
    let lastError = null;
    // 1. Try local build directory
    const localPath = path.resolve("build", "Release", filename);
    if (fs.existsSync(localPath)) {
        try {
            const mod = require(localPath);
            if (mod && mod._impl) return mod;
        } catch (e) {
            lastError = e;
        }
    }
    // 2. Try subpackages under npm/
    if (fs.existsSync("npm")) {
        const subDirs = fs.readdirSync("npm");
        for (const dir of subDirs) {
            const subPath = path.resolve("npm", dir, filename);
            if (fs.existsSync(subPath)) {
                try {
                    const mod = require(subPath);
                    if (mod && mod._impl) return mod;
                } catch (e) {
                    lastError = e;
                }
            }
        }
    }
    const err = new Error(`Failed to load ${filename}: ${lastError ? lastError.message : "binary not found"}`);
    if (lastError) err.cause = lastError;
    throw err;
}

if (!isV8 && Base128._impl !== "napi") {
    throw new Error(`Expected napi.node implementation when V8 acceleration is not active, got: ${Base128._impl}`);
}

const extraBase128 = isV8 && loadExtraBinding("napi.node");
if (extraBase128 && extraBase128._impl !== "napi") {
    throw new Error(`Expected extra binding to be napi.node, got: ${extraBase128._impl}`);
}

/**
 * @param {typeof Base128} base128
 * @param {string} [implName]
 */
function test(base128, implName = 'base128') {
    /**
     * @param {string} name
     */
    function test2(name) {
        console.log('------------------')
        console.log(`[${implName}] ${name}`)
        const file = fs.readFileSync(inputDir + "/" + name)
        console.log('file length:', file.length)
        console.log()

        console.log(`${implName}:`)

        console.time('time encode')
        const result = base128.encode(file)
        console.timeEnd('time encode')

        console.time('time toJSTemplateLiterals')
        result.toJSTemplateLiterals()
        console.timeEnd('time toJSTemplateLiterals')

        console.time('time toString')
        const encodedString = result.toString()
        console.timeEnd('time toString')

        console.time('time string toJSTemplateLiterals')
        const encodedTemplate = result.toJSTemplateLiterals.call(encodedString)
        console.timeEnd('time string toJSTemplateLiterals')

        // console.log(euq)
        console.log('toJSTemplateLiterals length:', encodedTemplate.length)

        if (enableOutput)
            fs.writeFileSync(`${outputDir}/${name}.js`, encodedTemplate)

        console.time('time parseJSTemplateLiterals')
        const euqeval = base128.parseJSTemplateLiterals(encodedTemplate)
        console.timeEnd('time parseJSTemplateLiterals')
        // console.log('eval length:', euqeval.length)
        console.time('time decode')
        const decoded = base128.decode(euqeval)
        console.timeEnd('time decode')
        // console.log('decoded length:', decoded.length)
        // euqal?
        const isEqual = file.equals(decoded);
        allSuccess &&= isEqual
        console.log('equal:', isEqual)
        if (!isEqual) {
            throw new Error(`[${implName}] Test failed on ${name}: decoded data does not match original file`);
        }
        console.log()

        console.log('base64:')
        // const b64 = f.toString('base64')
        // console.log(b64)
        console.log('encoded length:', Math.ceil(file.length / 3) * 4)
    }

    fs.readdirSync(inputDir)
        .map(name => ({ name, size: fs.statSync(inputDir + '/' + name).size }))
        .sort((a, b) => b.size - a.size)
        .forEach(v => test2(v.name))
}

// test native addon (via main.cjs)
console.log(`################## Base128 (${currentBindingName} via main.cjs) ##################`);
test(Base128, currentBindingName);

// test extra native addon (napi.node when v8.node was loaded via main.cjs)
if (extraBase128) {
    console.log(`################## napi.node (additional native addon) ##################`);
    test(extraBase128, "napi.node");
}

// test pure JS (browser.mjs)
console.log('################## browser.mjs (pure JS) ##################');
test(browserBase128, 'browser.mjs');

// test mini-decode.mjs (pure JS decoder)
console.log('################## mini-decode.mjs ##################');
{
    const name = '50MB';
    if (fs.existsSync(inputDir + '/' + name)) {
        console.log('------------------');
        console.log(`[mini-decode.mjs] ${name}`);
        const file = fs.readFileSync(inputDir + '/' + name);
        console.log('file length:', file.length);
        console.log();

        console.log('mini-decode.mjs:');
        const encodedString = Base128.encode(file).toString();

        console.time('time decode');
        const decoded = miniDecode(encodedString);
        console.timeEnd('time decode');

        const isEqual = file.equals(decoded);
        allSuccess &&= isEqual;
        console.log('equal:', isEqual);
    }
}

// cross-compatibility / parity check between native, browser.mjs, extra binding, and mini-decode.mjs
console.log('------------------');
console.log('Parity / Cross-compatibility Check:');
for (const { name } of fs.readdirSync(inputDir).map(name => ({ name }))) {
    const file = fs.readFileSync(inputDir + '/' + name);
    const nativeRes = Base128.encode(file);
    const browserRes = browserBase128.encode(file);

    const bytesMatch = Buffer.from(nativeRes.bytes).equals(Buffer.from(browserRes.bytes));
    allSuccess &&= bytesMatch;

    const strMatch = nativeRes.toString() === browserRes.toString();
    allSuccess &&= strMatch;

    const jstlMatch = nativeRes.toJSTemplateLiterals() === browserRes.toJSTemplateLiterals();
    allSuccess &&= jstlMatch;

    const cross1 = file.equals(Base128.decode(browserRes.toString()));
    const cross2 = file.equals(browserBase128.decode(nativeRes.toString()));
    const cross3 = file.equals(miniDecode(nativeRes.toString()));
    allSuccess &&= cross1 && cross2 && cross3;

    if (!bytesMatch || !strMatch || !jstlMatch || !cross1 || !cross2 || !cross3) {
        console.error(`Mismatch between native, browser, or miniDecode on file: ${name}`);
        allSuccess = false;
        throw new Error(`Mismatch between native, browser, or miniDecode on file: ${name}`);
    }

    if (extraBase128) {
        const extraRes = extraBase128.encode(file);
        const extraBytesMatch = Buffer.from(extraRes.bytes).equals(Buffer.from(nativeRes.bytes));
        allSuccess &&= extraBytesMatch;

        const extraStrMatch = extraRes.toString() === nativeRes.toString();
        allSuccess &&= extraStrMatch;

        const extraJstlMatch = extraRes.toJSTemplateLiterals() === nativeRes.toJSTemplateLiterals();
        allSuccess &&= extraJstlMatch;

        const crossExtra = file.equals(extraBase128.decode(nativeRes.toString()));
        allSuccess &&= crossExtra;

        if (!extraBytesMatch || !extraStrMatch || !extraJstlMatch || !crossExtra) {
            console.error(`Mismatch between ${currentBindingName} and napi.node on file: ${name}`);
            allSuccess = false;
            throw new Error(`Mismatch between ${currentBindingName} and napi.node on file: ${name}`);
        }
    }
}
console.log('Cross-compatibility passed:', allSuccess)

console.log('------------------')

// Verify parseJSTemplateLiterals spec compliance (newline normalization, leading zeros in \u{...}, line continuations)
for (const impl of [Base128, browserBase128, extraBase128].filter(Boolean)) {
    const specTests = [
        ['`hello\\r\\nworld`', 'hello\r\nworld'],
        ['`hello\r\nworld`', 'hello\nworld'],
        ['`hello\rworld`', 'hello\nworld'],
        ['`\\u{0000000041}`', 'A'],
        ['`\\u{000010ffff}`', '\u{10ffff}'],
        ['`a\\\u2028b`', 'ab'],
        ['`a\\\u2029b`', 'ab'],
        ['\u2028`hello`\u2029', 'hello'],
        ['\u2028`hello`', 'hello'],
        ['`hello`\u2029', 'hello'],
        ['`foo$bar`', 'foo$bar'],
        ['`foo<script`', 'foo<script'],
        ['`foo<bar`', 'foo<bar'],
    ];
    for (const [input, expected] of specTests) {
        const actual = impl.parseJSTemplateLiterals(input);
        if (actual !== expected) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Spec test failed for ${JSON.stringify(input)}: got ${JSON.stringify(actual)}, expected ${JSON.stringify(expected)}`);
        }
    }

    // Verify syntax errors (e.g. \1-\9, \0 followed by digit)
    for (const d of ['1', '2', '3', '4', '5', '6', '7', '8', '9']) {
        for (const input of [`\`\\${d}\``, `\`\\0${d}\``]) {
            let threw = false;
            try {
                impl.parseJSTemplateLiterals(input);
            } catch (e) {
                threw = e instanceof SyntaxError;
            }
            if (!threw) {
                allSuccess = false;
                throw new Error(`[${impl._impl}] Expected SyntaxError for invalid escape: ${input}`);
            }
        }
    }

    // Verify whitespace (Space_Separator) support outside backticks
    for (const ws of ['\u3000', '\u1680', '\u2000', '\u202F', '\u205F']) {
        const input = `${ws}\`test\`${ws}`;
        const actual = impl.parseJSTemplateLiterals(input);
        if (actual !== 'test') {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Space_Separator test failed for ${JSON.stringify(input)}`);
        }
    }

    // Verify decode invalid input (> 127) throws SyntaxError
    const invalidDecodeInputs = [
        '\u0080',
        '\u0100',
        '\u4e2d',
        'a\u0080',
        'ab\u0100',
        'abcdef\u4e2d',
        '\u0080bcdefgh',
        'abcd\u0080fgh',
        'abcdefg\u0080',
        '12345678\u0080',
        '12345678abcdefg\u4e2d',
    ];
    for (const input of invalidDecodeInputs) {
        let threw = false;
        try {
            impl.decode(input);
        } catch (e) {
            threw = e instanceof SyntaxError && e.message === "decode: invalid input";
        }
        if (!threw) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Expected SyntaxError("decode: invalid input") for input with char > 127: ${JSON.stringify(input)}`);
        }
    }

    // Verify encode non-string / non-Uint8Array throws TypeError
    for (const input of [123, null, undefined, {}, []]) {
        let threw = false;
        try {
            // @ts-ignore
            impl.encode(input);
        } catch (e) {
            threw = e instanceof TypeError && e.message === "encode: input must be a string, Uint8Array or Uint8ClampedArray";
        }
        if (!threw) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Expected TypeError for invalid encode input: ${String(input)}`);
        }
    }

    // Verify encode(string) automatically encodes to UTF-8
    const stringInputs = [
        "",
        "a",
        "Hello, World!",
        "1234567",
        "12345678",
        "café naïve",
        "你好，世界！🌍",
        "`${test}\\r\\n\0`",
        "A".repeat(2000), // > 1024 bytes (tests stack vs heap buffer)
        "测试".repeat(500),
    ];
    for (const str of stringInputs) {
        const expectedBytes = impl.encode(new TextEncoder().encode(str)).bytes;
        const actualEnc = impl.encode(str);
        if (!Buffer.from(expectedBytes).equals(Buffer.from(actualEnc.bytes))) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] encode(string) mismatch for: ${JSON.stringify(str.slice(0, 30))}`);
        }
        // Roundtrip check
        const decoded = new TextDecoder().decode(impl.decode(actualEnc.bytes));
        if (decoded !== str) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] encode(string) roundtrip failed for: ${JSON.stringify(str.slice(0, 30))}`);
        }
    }

    // Verify encode supports Uint8ClampedArray
    {
        const raw = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
        const u8 = new Uint8Array(raw);
        const clamped = new Uint8ClampedArray(raw);
        const encU8 = impl.encode(u8);
        const encClamped = impl.encode(clamped);
        if (!Buffer.from(encU8.bytes).equals(Buffer.from(encClamped.bytes))) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] encode(Uint8ClampedArray) mismatch`);
        }
    }

    // Verify encode with TwoByte ASCII string (view.is_one_byte() is false, but all ASCII)
    {
        for (const len of [0, 1, 6, 7, 8, 13, 14, 15, 100, 2000]) {
            const rawAscii = "A".repeat(len);
            const twoByteAscii = ('\u0100' + rawAscii).slice(1);
            const enc1 = impl.encode(twoByteAscii);
            const encExpected = impl.encode(new TextEncoder().encode(twoByteAscii));
            if (!Buffer.from(enc1.bytes).equals(Buffer.from(encExpected.bytes))) {
                allSuccess = false;
                throw new Error(`[${impl._impl}] encode(TwoByte ASCII string, len=${len}) mismatch`);
            }
            const dec = new TextDecoder().decode(impl.decode(enc1.bytes));
            if (dec !== twoByteAscii) {
                allSuccess = false;
                throw new Error(`[${impl._impl}] encode(TwoByte ASCII string, len=${len}) roundtrip mismatch`);
            }
        }
    }

    // Verify decode non-string / non-Uint8Array throws TypeError
    for (const input of [123, null, undefined, {}, []]) {
        let threw = false;
        try {
            // @ts-ignore
            impl.decode(input);
        } catch (e) {
            threw = e instanceof TypeError && e.message === "decode: input must be a string, Uint8Array or Uint8ClampedArray";
        }
        if (!threw) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Expected TypeError for non-string decode input: ${String(input)}`);
        }
    }

    // Verify parseJSTemplateLiterals non-string / non-Uint8Array throws TypeError
    for (const input of [123, null, undefined, {}, []]) {
        let threw = false;
        try {
            // @ts-ignore
            impl.parseJSTemplateLiterals(input);
        } catch (e) {
            threw = e instanceof TypeError && e.message === "parseJSTemplateLiterals: input must be a string, Uint8Array or Uint8ClampedArray";
        }
        if (!threw) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Expected TypeError for non-string parseJSTemplateLiterals input: ${String(input)}`);
        }
    }

    // Verify decode and parseJSTemplateLiterals support Uint8Array and Uint8ClampedArray
    {
        const testData = new Uint8Array([0, 1, 2, 127, 255, 42, 99]);
        const enc = impl.encode(testData);

        // decode with Uint8Array
        const dec1 = impl.decode(enc.bytes);
        if (!Buffer.from(testData).equals(Buffer.from(dec1))) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] decode(Uint8Array) failed`);
        }

        // decode with Uint8ClampedArray
        const clampedBytes = new Uint8ClampedArray(enc.bytes);
        const dec2 = impl.decode(clampedBytes);
        if (!Buffer.from(testData).equals(Buffer.from(dec2))) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] decode(Uint8ClampedArray) failed`);
        }

        // parseJSTemplateLiterals with Uint8Array
        const jstlStr = enc.toJSTemplateLiterals();
        const jstlBytes = Buffer.from(jstlStr);
        const parsed1 = impl.parseJSTemplateLiterals(jstlBytes);
        if (parsed1 !== enc.toString()) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] parseJSTemplateLiterals(Uint8Array) failed`);
        }

        // parseJSTemplateLiterals with Uint8ClampedArray
        const parsed2 = impl.parseJSTemplateLiterals(new Uint8ClampedArray(jstlBytes));
        if (parsed2 !== enc.toString()) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] parseJSTemplateLiterals(Uint8ClampedArray) failed`);
        }

        // decode invalid Uint8Array byte > 127
        let threw = false;
        try {
            impl.decode(new Uint8Array([128, 0, 0, 0, 0, 0, 0, 0]));
        } catch (e) {
            threw = e instanceof SyntaxError && e.message === "decode: invalid input";
        }
        if (!threw) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Expected SyntaxError for Uint8Array with byte > 127`);
        }

        // decode invalid Uint8Array rem == 1
        threw = false;
        try {
            impl.decode(new Uint8Array([65]));
        } catch (e) {
            threw = e instanceof SyntaxError && e.message === "decode: invalid input";
        }
        if (!threw) {
            allSuccess = false;
            throw new Error(`[${impl._impl}] Expected SyntaxError for Uint8Array with rem == 1`);
        }
    }
}

// Verify EncodeResult.prototype.toString/toJSTemplateLiterals reject non-Uint8Array
{
    const res = Base128.encode(new Uint8Array([1, 2, 3]));
    res.bytes = new Uint32Array([1, 2, 3]);
    let threw = false;
    try {
        res.toString();
    } catch (e) {
        threw = e instanceof TypeError;
    }
    if (!threw) {
        allSuccess = false;
        throw new Error("Expected TypeError for non-Uint8Array this.bytes in toString");
    }
}

console.log('allSuccess:', allSuccess)
console.log()

if (!allSuccess) {
    throw new Error("Test failed: one or more checks did not succeed");
}
