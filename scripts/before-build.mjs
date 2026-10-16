import fs from "node:fs";
import path from "node:path";

const rootDir = path.resolve(import.meta.dirname, "..");
const linkPath = path.join(rootDir, "binding.gyp");

try {
    const stat = fs.lstatSync(linkPath);
    if (!stat.isSymbolicLink() || fs.readlinkSync(linkPath) !== ".binding.gyp") {
        fs.unlinkSync(linkPath);
        fs.symlinkSync(".binding.gyp", linkPath, "file");
    }
} catch {
    fs.symlinkSync(".binding.gyp", linkPath, "file");
}

const websiteDir = path.join(rootDir, "website");
const websiteLinks = [
    { link: path.join(websiteDir, "wasm.mjs"), target: "../wasm/wasm.mjs" },
    { link: path.join(websiteDir, "base128.wasm"), target: "../wasm/base128.wasm" }
];

for (const { link, target } of websiteLinks) {
    try {
        const stat = fs.lstatSync(link);
        if (!stat.isSymbolicLink() || fs.readlinkSync(link) !== target) {
            fs.unlinkSync(link);
            fs.symlinkSync(target, link, "file");
        }
    } catch {
        fs.symlinkSync(target, link, "file");
    }
}

