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

const encodedTemplate = base128.encode(input).toJSTemplateLiterals()

const decodedBytes = base128.decode(eval(encodedTemplate))
```

---

## Effect

Encode this jpg file, use base128 is `104,588 Bytes` smaller than base64:

```
screenshot-45.519.jpg
file length: 682086

base128:
time encode: 10.296ms
time toString: 0.244ms
time toJSTemplateLiterals: 11.763ms
toJSTemplateLiterals length: 804860
time eval: 5.963ms
time decode: 6.246ms
equal: true

base64:
encoded length: 909448
```

Encode `50MB` file, use base128 is `7,664,748 Bytes` smaller than base64:

```
50MB
file length: 50000000

base128:
time encode: 69.265ms
time toString: 9.311ms
time toJSTemplateLiterals: 211.551ms
toJSTemplateLiterals length: 59001920
time eval: 1.049s
time decode: 170.828ms
equal: true

base64:
encoded length: 66666668
```
