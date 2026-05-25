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
time encode: 7.306ms
time toString: 0.168ms
time toJSTemplateLiterals: 1.876ms
toJSTemplateLiterals length: 796961
time eval: 6.242ms
time decode: 10.614ms
equal: true

base64:
encoded length: 909448
```

Encode `50MB` file, use base128 is `8180525 Bytes` smaller than base64:

```
50MB
file length: 50000000

base128:
time encode: 67.717ms
time toString: 10.025ms
time toJSTemplateLiterals: 197.364ms
toJSTemplateLiterals length: 58486143
time eval: 1.083s
time decode: 171.262ms
equal: true

base64:
encoded length: 66666668
```
