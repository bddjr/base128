import base128 from "../dist/index.js";
import fs from "fs"

fs.existsSync("test-output") || fs.mkdirSync("test-output")

let allSuccess = true

/**
 * @param {typeof base128} base128
 */
function test(base128) {
    /**
     * @param {string} name
     */
    function test2(name) {
        console.log('------------------')
        console.log(name)
        const f = fs.readFileSync("test-input/" + name)
        console.log('file length:', f.length)
        console.log()

        console.log('encode:')
        const encodedTemplate = base128.encode(f).toJSTemplateLiterals()
        // console.log(euq)
        console.log('bytes length:', encodedTemplate.length)
        fs.writeFileSync(`test-output/${name}.js`, encodedTemplate)

        const euqeval = eval(encodedTemplate)
        console.log('eval length:', euqeval.length)
        const decoded = base128.decode(euqeval)
        console.log('decoded length:', decoded.length)
        // euqal?
        const isEqual = (() => {
            if (decoded.length !== f.length) return false
            for (const i in decoded) {
                if (decoded[i] !== f[i]) return false
            }
            return true
        })()
        allSuccess &&= isEqual
        console.log('equal:', isEqual)
        console.log()

        console.log('base64:')
        const b64 = f.toString('base64')
        // console.log(b64)
        console.log('length:', b64.length)
    }

    test2("0.txt")
    test2("1.txt")
    test2("2.txt")
    test2("3.txt")
    test2("4.txt")
    test2("5.txt")
    test2("6.txt")
    test2("7.txt")
    test2("hello.html.gz")
    test2("hello.html")
    test2("neuro-head-ball.jpg")
    test2("screenshot-45.519.jpg")
}

// test npm
test(base128);

// test browser
((self) => {
    // @ts-ignore
    delete self.base128

    eval(fs.readFileSync('dist/browser.min.js').toString())

    // @ts-ignore
    test(self.base128)
})(global);

console.log('------------------')
console.log('allSuccess:', allSuccess)
console.log()
