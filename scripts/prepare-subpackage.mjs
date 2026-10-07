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

// 2. Locate built .node file
const srcNode = path.join("build", "Release", "base128.node");
const destNode = path.join("npm", target, "base128.node");

if (!fs.existsSync(srcNode)) {
    console.error(`Built binary not found at ${srcNode}`);
    process.exit(1);
}

// 3. Strip binary on Unix-like systems if strip tool is available
if (process.platform !== "win32") {
    try {
        const stripFlag = process.platform === "darwin" ? "-x" : "--strip-all";
        execSync(`strip ${stripFlag} "${srcNode}"`, { stdio: "ignore" });
        console.log(`Stripped ${srcNode}`);
    } catch {}
}

fs.copyFileSync(srcNode, destNode);
console.log(`Prepared subpackage ${target}: copied ${srcNode} -> ${destNode} (version ${rootPkg.version})`);
