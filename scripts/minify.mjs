import package_json from '../package.json' with {type: 'json'}
import fs from 'fs'
import { minify_sync } from 'terser';

fs.existsSync('dist') || fs.mkdirSync('dist')

let js = fs.readFileSync(package_json.main).toString()

// @ts-ignore
js = minify_sync('{' + js.replace('module.exports', 'this.base128') + '}', {
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

fs.writeFileSync(package_json.browser, js)

//========================
// README

const readmeName = 'README.md'

if (fs.existsSync(readmeName)) {
    const oldReadme = fs.readFileSync(readmeName).toString()

    let newReadme = oldReadme.replace(
        /(\n```html browser\r?\n)[\d\D]+?(\n```)/,
        (m, a, b) => (a + '<script>\n' + js + '\n</script>' + b)
    )

    newReadme !== oldReadme && fs.writeFileSync(readmeName, newReadme)
}