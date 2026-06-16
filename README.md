Smaller than base64, only use ASCII, can run in web browser.

Build for [vite-plugin-singlefile-compression](https://github.com/bddjr/vite-plugin-singlefile-compression)

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

Encode this jpg file, use base128 is `112487 Bytes` smaller than base64:

```
screenshot-45.519.jpg
file length: 682086

base128:
time encode: 8.742ms
time toString: 2.279ms
time toJSTemplateLiterals: 13.645ms
toJSTemplateLiterals length: 796961
time eval: 5.98ms
time decode: 5.857ms
equal: true

base64:
encoded length: 909448
```

Encode `50MB` file, use base128 is `8180525 Bytes` smaller than base64:

```
50MB
file length: 50000000

base128:
time encode: 70.423ms
time toString: 9.012ms
time toJSTemplateLiterals: 150.798ms
toJSTemplateLiterals length: 58486143
time eval: 1.051s
time decode: 183.591ms
equal: true

base64:
encoded length: 66666668
```
