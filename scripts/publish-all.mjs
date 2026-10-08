import fs from "node:fs";
import path from "node:path";
import { execSync } from "node:child_process";

const rootPkgPath = path.resolve("package.json");
const rootPkg = JSON.parse(fs.readFileSync(rootPkgPath, "utf8"));
const version = rootPkg.version;

const downloadDir = path.resolve(process.argv[2] || "downloaded-bindings");
const npmDir = path.resolve("npm");

// 1. Collect all expected subpackages from npm/ directory
const expectedSubpkgs = fs.readdirSync(npmDir)
    .filter(dir => fs.existsSync(path.join(npmDir, dir, "package.json")))
    .map(dir => {
        const pkg = JSON.parse(fs.readFileSync(path.join(npmDir, dir, "package.json"), "utf8"));
        return { dir, name: pkg.name };
    });

if (expectedSubpkgs.length === 0) {
    throw new Error("No subpackage definitions found under npm/ directory");
}

console.log(`Expected ${expectedSubpkgs.length} architecture bindings:`);
for (const { dir, name } of expectedSubpkgs) {
    console.log(`  - ${dir} (${name})`);
}

if (!fs.existsSync(downloadDir)) {
    throw new Error(`Download directory not found: ${downloadDir}`);
}

// 2. Scan downloaded artifacts
const downloadedSubpkgs = new Map();
const entries = fs.readdirSync(downloadDir, { withFileTypes: true });
for (const entry of entries) {
    if (entry.isDirectory()) {
        const subDir = path.join(downloadDir, entry.name);
        const subPkgPath = path.join(subDir, "package.json");
        if (fs.existsSync(subPkgPath)) {
            const subPkg = JSON.parse(fs.readFileSync(subPkgPath, "utf8"));
            const hasBinary = ["napi.node", "v8.node"].some(f => fs.existsSync(path.join(subDir, f)));
            downloadedSubpkgs.set(subPkg.name, {
                subDir,
                subPkgPath,
                subPkg,
                hasBinary,
            });
        }
    }
}

// 3. Strictly verify that ALL expected architectures are present with built binaries
const missing = [];
for (const { dir, name } of expectedSubpkgs) {
    const found = downloadedSubpkgs.get(name);
    if (!found) {
        missing.push(`${name} (${dir}): not found in downloaded artifacts`);
    } else if (!found.hasBinary) {
        missing.push(`${name} (${dir}): found in artifacts but missing native .node binary`);
    }
}

if (missing.length > 0) {
    console.error(`\n[FATAL] Not all architecture bindings were built successfully!`);
    console.error(`Missing ${missing.length} of ${expectedSubpkgs.length} bindings:`);
    for (const m of missing) {
        console.error(`  x ${m}`);
    }
    throw new Error(`Publish aborted: ${missing.length} architecture bindings missing. All architectures must be built before publishing.`);
}

console.log(`\nAll ${expectedSubpkgs.length} architecture bindings verified! Proceeding with publish...`);

// 4. Publish all subpackages
const publishedBindings = {};
for (const { dir, name } of expectedSubpkgs) {
    const { subDir, subPkgPath, subPkg } = downloadedSubpkgs.get(name);
    subPkg.version = version;
    fs.writeFileSync(subPkgPath, JSON.stringify(subPkg, null, 2) + "\n");

    console.log(`\n========================================`);
    console.log(`Publishing subpackage: ${subPkg.name}@${version}`);
    console.log(`========================================`);
    execSync(`pnpm -C "${subDir}" publish --no-git-checks --access public`, { stdio: "inherit" });
    publishedBindings[subPkg.name] = version;
}

// 5. Inject all verified subpackages into root package.json before publishing root
rootPkg.optionalDependencies = publishedBindings;
fs.writeFileSync(rootPkgPath, JSON.stringify(rootPkg, null, 2) + "\n");
console.log(`\nInjected all ${Object.keys(publishedBindings).length} optionalDependencies into root package.json`);

// 6. Publish the root package
console.log(`\n========================================`);
console.log(`Publishing root package: ${rootPkg.name}@${version}`);
console.log(`========================================`);
execSync(`pnpm publish --no-git-checks --access public`, { stdio: "inherit" });
