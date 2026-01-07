import esbuild from 'esbuild'
import fs from 'fs'

let js = fs.readFileSync('dist/index.js').toString()

js = js
    .replace('\nexport default base128', '\n')
    .replaceAll(/(\n|^)export /g, '\n')
    .replace('\nconst base128 =', '\nbase128=')

const tempName = 'dist/build-for-browser.temp.index.js'
fs.writeFileSync(tempName, js)

let result = esbuild.buildSync({
    entryPoints: [tempName],
    bundle: true,
    minify: true,
    treeShaking: true,
    platform: 'browser',
    format: 'iife',
    logLevel: "warning",
    write: false,
})

if (result.errors.length) {
    throw result.errors
}

fs.rmSync(tempName)

js = result.outputFiles[0].text.replace(/;?\s*$/, '')

fs.writeFileSync("dist/browser.js", js)
