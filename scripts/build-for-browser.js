import fs from 'fs'
import { minify_sync } from 'terser';
import { rimrafSync } from 'rimraf'

let js = fs.readFileSync('dist/browser.js').toString()

// @ts-ignore
js = minify_sync(js, {
    compress: {
        ecma: 2015,
        keep_classnames: true,
    },
    mangle: {
        keep_classnames: true,
    },
    format: {
        wrap_iife: false,
        ascii_only: true,
        ecma: 2015,
        inline_script: true,
        shebang: false,
    }
}).code.replaceAll("</script", "<\\/script")

fs.writeFileSync("dist/browser.min.js", js)

rimrafSync(['dist/browser.js', 'dist/browser.d.ts'])

//========================
// README

const readmeName = 'README.md'

const oldReadme = fs.readFileSync(readmeName).toString()

let newReadme = oldReadme.replace(
    /(\n```html browser\r?\n)[\d\D]+?(\n```)/,
    (m, a, b) => (a + '<script>\n' + js + '\n</script>' + b)
)

newReadme !== oldReadme && fs.writeFileSync(readmeName, newReadme)
