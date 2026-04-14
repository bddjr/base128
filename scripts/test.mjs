//@ts-check

import base128 from "../main.js";
import fs from "fs"

const enableOutput = false

const inputDir = "testdata/input"
const outputDir = "testdata/output"

fs.rmSync(outputDir, { recursive: true, force: true })

if (enableOutput && !fs.existsSync(outputDir))
    fs.mkdirSync(outputDir)

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
        const file = fs.readFileSync(inputDir + "/" + name)
        console.log('file length:', file.length)
        console.log()

        console.log('base128:')
        const encodedTemplate = base128.encode(file).toJSTemplateLiterals()
        // console.log(euq)
        console.log('toJSTemplateLiterals length:', encodedTemplate.length)

        if (enableOutput)
            fs.writeFileSync(`${outputDir}/${name}.js`, encodedTemplate)

        const euqeval = eval(encodedTemplate)
        // console.log('eval length:', euqeval.length)
        const decoded = base128.decode(euqeval)
        // console.log('decoded length:', decoded.length)
        // euqal?
        const isEqual = (() => {
            if (decoded.length !== file.length) return false
            for (const i in decoded) {
                if (decoded[i] !== file[i]) return false
            }
            return true
        })()
        allSuccess &&= isEqual
        console.log('equal:', isEqual)
        console.log()

        console.log('base64:')
        // const b64 = f.toString('base64')
        // console.log(b64)
        console.log('encoded length:', Math.ceil(file.length / 3) * 4)
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

// test esm
test(base128);

console.log('------------------')

// test prototype
const bb = new base128.Base128Bytes(7)
if (!(bb.buffer instanceof ArrayBuffer)) throw TypeError()
if (bb.byteLength !== 7) throw TypeError()
if (bb.byteOffset !== 0) throw TypeError()
if (bb.length !== 7) throw TypeError()

console.log('allSuccess:', allSuccess)
console.log()

allSuccess || process.exit(1)
