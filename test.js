import base128_ascii from "./dist/index.js";
import fs from "fs"

fs.existsSync("test-output") || fs.mkdirSync("test-output")

let allSuccess = true

function test(name) {
    console.log('------------------')
    console.log(name)
    const f = fs.readFileSync("test-input/" + name)
    console.log('file length:', f.length)
    console.log()

    console.log('encodeToTemplateLiterals:')
    const euq = base128_ascii.encodeToTemplateLiterals(Uint8Array.from(f))
    // console.log(euq)
    console.log('bytes length:', Buffer.from(euq).length)
    fs.writeFileSync(`test-output/${name}.js`, euq)

    const euqeval = eval(euq)
    console.log('eval length:', euqeval.length)
    const decoded = base128_ascii.decode(euqeval)
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

test("0.txt")
test("1.txt")
test("2.txt")
test("3.txt")
test("4.txt")
test("5.txt")
test("6.txt")
test("7.txt")
test("hello.html.gz")
test("hello.html")
test("neuro-head-ball.jpg")
test("screenshot-45.519.jpg")

console.log('------------------')
console.log('allSuccess:', allSuccess)
console.log()
