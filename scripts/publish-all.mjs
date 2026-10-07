import fs from "node:fs";
import path from "node:path";
import { execSync } from "node:child_process";

const rootPkgPath = path.resolve("package.json");
const rootPkg = JSON.parse(fs.readFileSync(rootPkgPath, "utf8"));
const version = rootPkg.version;

const downloadDir = path.resolve(process.argv[2] || "downloaded-bindings");
const publishedBindings = {};

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
                publishedBindings[subPkg.name] = version;
            }
        }
    }
} else {
    console.log(`Directory ${downloadDir} not found, skipping subpackages publish.`);
}

// 2. Inject actually published subpackages into root package.json before publishing root
if (Object.keys(publishedBindings).length > 0) {
    rootPkg.optionalDependencies = publishedBindings;
    fs.writeFileSync(rootPkgPath, JSON.stringify(rootPkg, null, 2) + "\n");
    console.log(`\nInjected ${Object.keys(publishedBindings).length} optionalDependencies into root package.json`);
}

// 3. Publish the root package
console.log(`\n========================================`);
console.log(`Publishing root package: ${rootPkg.name}@${version}`);
console.log(`========================================`);
execSync(`pnpm publish --no-git-checks --access public`, { stdio: "inherit" });
