import fs from "node:fs";
import path from "node:path";
import { execSync } from "node:child_process";

const rootDir = path.resolve(import.meta.dirname, "..");
const wasmDir = path.join(rootDir, "wasm");
const cppSource = path.join(wasmDir, "base128.cpp");
const wasmOutput = path.join(wasmDir, "base128.wasm");

function findClang() {
    if (process.env.CLANG && fs.existsSync(process.env.CLANG)) {
        return process.env.CLANG;
    }

    const defaultWindowsPath = "C:\\Program Files\\LLVM\\bin\\clang.exe";
    if (process.platform === "win32" && fs.existsSync(defaultWindowsPath)) {
        return defaultWindowsPath;
    }

    try {
        const checkCmd = process.platform === "win32" ? "where clang" : "which clang";
        const result = execSync(checkCmd, { stdio: ["pipe", "pipe", "ignore"], encoding: "utf8" }).trim().split(/\r?\n/)[0];
        if (result && fs.existsSync(result)) {
            return result;
        }
    } catch {
        // clang not found in PATH
    }

    return null;
}

const clang = findClang();

if (!clang) {
    if (fs.existsSync(wasmOutput)) {
        console.warn("[build-wasm] Clang compiler not found; skipping WASM compilation and keeping existing wasm/base128.wasm.");
        process.exit(0);
    }
    console.error("[build-wasm] Error: Clang compiler with wasm32 support not found.");
    console.error("Please install LLVM/Clang or set the CLANG environment variable.");
    process.exit(1);
}

fs.mkdirSync(wasmDir, { recursive: true });

console.log(`[build-wasm] Compiling ${cppSource} to ${wasmOutput} with ${clang}...`);

const compileCmd = [
    `"${clang}"`,
    "--target=wasm32",
    "-O3",
    "-flto",
    "-nostdlib",
    "-msimd128",
    "-Wl,--no-entry",
    "-Wl,--export=base128wasm_encode",
    "-Wl,--export=base128wasm_decode",
    "-Wl,--export=base128wasm_to_js_template_literals",
    "-Wl,--export=base128wasm_parse_js_template_literals",
    "-Wl,--export=__heap_base",
    "-Wl,--import-memory",
    "-Wl,--strip-all",
    "-Wl,--lto-O3",
    "-Wl,--gc-sections",
    "-o",
    `"${wasmOutput}"`,
    `"${cppSource}"`
].join(" ");

try {
    execSync(compileCmd, { stdio: "inherit" });
} catch (e) {
    console.error("[build-wasm] Compilation failed:", e);
    process.exit(1);
}

const stat = fs.statSync(wasmOutput);
console.log(`[build-wasm] Compiled WASM binary size: ${stat.size} bytes`);
