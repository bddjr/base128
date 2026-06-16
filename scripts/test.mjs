//@ts-check

import base128 from "../main.js";
import fs from "node:fs"

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

        console.time('time encode')
        const result = base128.encode(file)
        console.timeEnd('time encode')

        console.time('time toString')
        const encodedString = result.toString()
        console.timeEnd('time toString')

        console.time('time toJSTemplateLiterals')
        const encodedTemplate = result.toJSTemplateLiterals.call(encodedString)
        console.timeEnd('time toJSTemplateLiterals')

        // console.log(euq)
        console.log('toJSTemplateLiterals length:', encodedTemplate.length)

        if (enableOutput)
            fs.writeFileSync(`${outputDir}/${name}.js`, encodedTemplate)

        console.time('time eval')
        const euqeval = (0, eval)(encodedTemplate)
        console.timeEnd('time eval')
        // console.log('eval length:', euqeval.length)
        console.time('time decode')
        const decoded = base128.decode(euqeval)
        console.timeEnd('time decode')
        // console.log('decoded length:', decoded.length)
        // euqal?
        const isEqual = file.equals(decoded);
        allSuccess &&= isEqual
        console.log('equal:', isEqual)
        console.log()

        console.log('base64:')
        // const b64 = f.toString('base64')
        // console.log(b64)
        console.log('encoded length:', Math.ceil(file.length / 3) * 4)
    }

    fs.readdirSync(inputDir)
        .map(name => ({ name, size: fs.statSync(inputDir + '/' + name).size }))
        .sort((a, b) => b.size - a.size)
        .forEach(v => test2(v.name))
}

// test esm
test(base128);

console.log('------------------')

// base128.encode(class { static buffer = new ArrayBuffer })

console.log('allSuccess:', allSuccess)
console.log()


allSuccess || process.exit(1)
