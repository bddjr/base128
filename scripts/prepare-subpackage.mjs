import fs from "node:fs";
import path from "node:path";
import { execSync } from "node:child_process";

const target = process.argv[2];
if (!target) {
    console.error("Usage: node scripts/prepare-subpackage.mjs <target-name>");
    process.exit(1);
}

const rootPkg = JSON.parse(fs.readFileSync("package.json", "utf8"));
const subPkgPath = path.join("npm", target, "package.json");

if (!fs.existsSync(subPkgPath)) {
    console.error(`Subpackage package.json not found: ${subPkgPath}`);
    process.exit(1);
}

// 1. Sync version
const subPkg = JSON.parse(fs.readFileSync(subPkgPath, "utf8"));
subPkg.version = rootPkg.version;
fs.writeFileSync(subPkgPath, JSON.stringify(subPkg, null, 2) + "\n");

// 2. Helper to strip binary on Unix-like systems if strip tool is available
function stripFile(file) {
    if (process.platform !== "win32") {
        try {
            const stripFlag = process.platform === "darwin" ? "-x" : "--strip-all";
            execSync(`strip ${stripFlag} "${file}"`, { stdio: "ignore" });
            console.log(`Stripped ${file}`);
        } catch {}
    }
}

// 3. Locate and copy built .node files
if (process.argv[3]) {
    const src = process.argv[3];
    const filename = path.basename(src);
    const dest = path.join("npm", target, filename);
    if (!fs.existsSync(src)) {
        console.error(`Built binary not found at ${src}`);
        process.exit(1);
    }
    stripFile(src);
    fs.copyFileSync(src, dest);
    console.log(`Prepared subpackage ${target}: copied ${src} -> ${dest} (version ${rootPkg.version})`);
} else {
    let copiedCount = 0;
    for (const name of ["napi.node", "v8.node"]) {
        const src = path.join("build", "Release", name);
        const dest = path.join("npm", target, name);
        if (fs.existsSync(src)) {
            stripFile(src);
            fs.copyFileSync(src, dest);
            console.log(`Prepared subpackage ${target}: copied ${src} -> ${dest} (version ${rootPkg.version})`);
            copiedCount++;
        }
    }
    if (copiedCount === 0) {
        console.error(`No built binary found in build/Release for ${target}`);
        process.exit(1);
    }
}
