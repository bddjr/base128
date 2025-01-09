Smaller than base64, only use ASCII, can run in web browser.

## Setup

```
npm i base128-ascii
```

```js
import base128 from "base128-ascii"

const encodedTemplate = base128.encode(Uint8Array.from(fs.readFileSync("example.gz"))).toJSTemplateLiterals()

const decoded = base128.decode(eval(encodedTemplate))
```

---

Char Code (ASCII) :

```js
'\0\1\2\3\4\5\6\7\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F'
```

Encode this jpg file, use base128 is 102KiB smaller than base64:

```
screenshot-45.519.jpg
file length: 682086

encode:
bytes length: 804860
eval length: 779527
decoded length: 682086
equal: true

base64:
length: 909448
```

![](img.jpg)

---

The project was born for [vite-plugin-singlefile-compression](https://github.com/bddjr/vite-plugin-singlefile-compression).

Made by bddjr, using MIT License.
