// node scripts/clean.mjs

import fs from "node:fs";
for (const p of fs.globSync([
    "binding.gyp",
    "build",
    "prebuilds",
    "downloaded-bindings",
    "target",
    "dist",
    "npm/**/*.node",
    "node_modules",
])) {
    fs.rmSync(p, { recursive: true, force: true })
}
