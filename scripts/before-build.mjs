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
