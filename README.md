Smaller than base64, only use ASCII, can run in web browser.

Build for [vite-plugin-singlefile-compression](https://bddjr.github.io/vite-plugin-singlefile-compression/#/)

Preview: https://bddjr.github.io/base128/

## Features

- **Smaller than Base64**: ~12.5% smaller payload size than Base64.
- **ASCII Safe**: Encodes binary data into safe ASCII characters and JavaScript template literals.
- **High Performance**: Native C++ (Node-API) implementation for Node.js / Bun / Deno with prebuilt binaries across platforms (Linux, Windows, macOS).
- **Universal**: Pure JavaScript fallback (`src/browser.mjs`) that runs seamlessly in all web browsers and runtimes.

## Setup

```sh
npm i base128-ascii@latest
```

```js
import Base128 from "base128-ascii"
import fs from "fs"

const input = fs.readFileSync("example.gz")

// encode to Template literals
const encodedTemplate = Base128.encode(input).toJSTemplateLiterals()

// Safe parse Template literals to string
const jstlToStr = Base128.parseJSTemplateLiterals(encodedTemplate)

// decode to bytes
const decodedBytes = Base128.decode(jstlToStr)
```

## Effect

Encode this jpg file, use base128 is `104,588 Bytes` smaller than base64:

```
[v8.node] screenshot-45.519.jpg
file length: 682086

v8.node:
time encode: 0.204ms
time toJSTemplateLiterals: 0.854ms
time toString: 0.186ms
time string toJSTemplateLiterals: 0.892ms
toJSTemplateLiterals length: 804860
time parseJSTemplateLiterals: 0.715ms
time decode: 0.213ms
equal: true

base64:
encoded length: 909448
```

Encode `50MB` file, use base128 is `7,664,748 Bytes` smaller than base64:

```
[v8.node] 50MB
file length: 50000000

v8.node:
time encode: 12.145ms
time toJSTemplateLiterals: 61.905ms
time toString: 9.956ms
time string toJSTemplateLiterals: 61.732ms
toJSTemplateLiterals length: 59001920
time parseJSTemplateLiterals: 45.462ms
time decode: 12.233ms
equal: true

base64:
encoded length: 66666668
```

## Mini Decoder

If you need a minimal decoder, see [`src/mini-decode.mjs`](src/mini-decode.mjs).

It results in an extremely small footprint after minification, making it ideal for scenarios that are highly sensitive to bundle size but less sensitive to decoding performance.

```
[mini-decode.mjs] 50MB
file length: 50000000

mini-decode.mjs:
time decode: 160.442ms
equal: true
```
