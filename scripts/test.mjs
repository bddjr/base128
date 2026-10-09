//@ts-check

import base128 from "base128-ascii";
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

// Verify that the imported base128 is indeed a native implementation
const nativeEncodeStr = String(base128.encode);
if (!nativeEncodeStr.includes("[native code]")) {
    throw new Error(`Test failed: base128.encode is not a native implementation!\nGot: ${nativeEncodeStr}`);
}

const require = createRequire(import.meta.url);

// Determine which binary main.cjs imported
const isV8 = base128._impl === "v8";
const currentBindingName = isV8 ? "v8.node" : "napi.node";

/**
 * @returns {typeof base128 | undefined}
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

if (!isV8 && base128._impl !== "napi") {
    throw new Error(`Expected napi.node implementation when V8 acceleration is not active, got: ${base128._impl}`);
}

const extraBase128 = isV8 && loadExtraBinding("napi.node");
if (extraBase128 && extraBase128._impl !== "napi") {
    throw new Error(`Expected extra binding to be napi.node, got: ${extraBase128._impl}`);
}

/**
 * @param {typeof base128} base128
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
console.log(`################## base128 (${currentBindingName} via main.cjs) ##################`);
test(base128, currentBindingName);

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
        const encodedString = base128.encode(file).toString();

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
    const nativeRes = base128.encode(file);
    const browserRes = browserBase128.encode(file);

    const bytesMatch = Buffer.from(nativeRes.bytes).equals(Buffer.from(browserRes.bytes));
    allSuccess &&= bytesMatch;

    const strMatch = nativeRes.toString() === browserRes.toString();
    allSuccess &&= strMatch;

    const jstlMatch = nativeRes.toJSTemplateLiterals() === browserRes.toJSTemplateLiterals();
    allSuccess &&= jstlMatch;

    const cross1 = file.equals(base128.decode(browserRes.toString()));
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

// Verify parseJSTemplateLiterals syntax errors (e.g. \1-\9, \0 followed by digit)
for (const impl of [base128, browserBase128, extraBase128].filter(Boolean)) {
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
}

// base128.encode(class { static buffer = new ArrayBuffer })

console.log('allSuccess:', allSuccess)
console.log()

if (!allSuccess) {
    throw new Error("Test failed: one or more checks did not succeed");
}
