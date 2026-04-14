Smaller than base64, only use ASCII, can run in web browser.

Build for [vite-plugin-singlefile-compression](https://github.com/bddjr/vite-plugin-singlefile-compression)

## Setup

### npm

```
npm i base128-ascii
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

Encode this jpg file, use base128 is `109.85 KiB` smaller than base64:

```
screenshot-45.519.jpg
file length: 682086

base128:
toJSTemplateLiterals length: 796961
equal: true

base64:
encoded length: 909448
```
