Smaller than base64, only use ASCII, can run in web browser.

Build for [vite-plugin-singlefile-compression](https://bddjr.github.io/vite-plugin-singlefile-compression/#/)

Preview: https://bddjr.github.io/base128/

## Setup

```
npm i base128-ascii@latest
```

```js
import base128 from "base128-ascii"
import fs from "fs"

const input = fs.readFileSync("example.gz")

// encode to Template literals
const encodedTemplate = base128.encode(input).toJSTemplateLiterals()

// (Safe eval) Parse Template literals to string
const jstlToStr = base128.parseJSTemplateLiterals(encodedTemplate)

// decode to bytes
const decodedBytes = base128.decode(jstlToStr)
```

---

## Effect

Encode this jpg file, use base128 is `104,588 Bytes` smaller than base64:

```
screenshot-45.519.jpg
file length: 682086

base128:
time encode: 10.167ms
time toString: 0.345ms
time toJSTemplateLiterals: 17.59ms
toJSTemplateLiterals length: 804860
time parseJSTemplateLiterals: 9.112ms
time decode: 5.306ms
equal: true

base64:
encoded length: 909448
```

Encode `50MB` file, use base128 is `7,664,748 Bytes` smaller than base64:

```
50MB
file length: 50000000

base128:
time encode: 68.431ms
time toString: 8.745ms
time toJSTemplateLiterals: 205.384ms
toJSTemplateLiterals length: 59001920
time parseJSTemplateLiterals: 1.178s
time decode: 161.5ms
equal: true

base64:
encoded length: 66666668
```
