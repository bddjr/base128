import fs from "node:fs";
import path from "node:path";
import { execSync } from "node:child_process";

const rootPkgPath = path.resolve("package.json");
const rootPkg = JSON.parse(fs.readFileSync(rootPkgPath, "utf8"));
const version = rootPkg.version;

// 1. Inject optionalDependencies into root package.json before publishing
rootPkg.optionalDependencies = {
    "@base128-ascii/binding-darwin-arm64": version,
    "@base128-ascii/binding-darwin-x64": version,
    "@base128-ascii/binding-linux-arm64": version,
    "@base128-ascii/binding-linux-x64": version,
    "@base128-ascii/binding-win32-x64": version,
};
fs.writeFileSync(rootPkgPath, JSON.stringify(rootPkg, null, 2) + "\n");
console.log(`Injected optionalDependencies (v${version}) into root package.json`);

// 2. Discover and publish all downloaded subpackages
const downloadDir = path.resolve(process.argv[2] || "downloaded-bindings");
if (fs.existsSync(downloadDir)) {
    const entries = fs.readdirSync(downloadDir, { withFileTypes: true });
    for (const entry of entries) {
        if (entry.isDirectory()) {
            const subDir = path.join(downloadDir, entry.name);
            const subPkgPath = path.join(subDir, "package.json");
            if (fs.existsSync(subPkgPath)) {
                // Ensure version in subpackage package.json matches
                const subPkg = JSON.parse(fs.readFileSync(subPkgPath, "utf8"));
                subPkg.version = version;
                fs.writeFileSync(subPkgPath, JSON.stringify(subPkg, null, 2) + "\n");

                console.log(`\n========================================`);
                console.log(`Publishing subpackage: ${subPkg.name}@${version}`);
                console.log(`========================================`);
                execSync(`pnpm -C "${subDir}" publish --no-git-checks --access public`, { stdio: "inherit" });
            }
        }
    }
} else {
    console.log(`Directory ${downloadDir} not found, skipping subpackages publish.`);
}

// 3. Publish the root package
console.log(`\n========================================`);
console.log(`Publishing root package: ${rootPkg.name}@${version}`);
console.log(`========================================`);
execSync(`pnpm publish --no-git-checks --access public`, { stdio: "inherit" });
