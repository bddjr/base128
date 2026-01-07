Smaller than base64, only use ASCII, can run in web browser.

The project was born for [vite-plugin-singlefile-compression](https://github.com/bddjr/vite-plugin-singlefile-compression).

## Setup

### npm

```
npm i base128-ascii
```

```js
import base128 from "base128-ascii"
import fs from 'fs'

const encodedTemplate = base128.encode(fs.readFileSync("example.gz")).toJSTemplateLiterals()

const decoded = base128.decode(eval(encodedTemplate))
```

### Browser

```js browser
(()=>{var l=class{constructor(r){this.uint8Array=r}toString(){return new TextDecoder().decode(this.uint8Array)}toJSTemplateLiterals(){return`\`${this.toString().replace(/[\r\\`]|\${|<\/script/g,r=>r=="\r"?"\\r":r=="<\/script"?"<\\/script":"\\"+r)}\``}};function h(e){for(var r=new Uint8Array(Math.ceil(e.length/7*8)),o=0,t=0;o<e.length;)r[t++]=e[o]>>1,r[t++]=e[o++]<<6&127|e[o]>>2,r[t++]=e[o++]<<5&127|e[o]>>3,r[t++]=e[o++]<<4&127|e[o]>>4,r[t++]=e[o++]<<3&127|e[o]>>5,r[t++]=e[o++]<<2&127|e[o]>>6,r[t++]=e[o++]<<1&127|e[o]>>7,r[t++]=e[o++]&127;return new l(r)}function s(e){for(var r=new Uint8Array(Math.floor(e.length/8*7)),o=0,t=0,c,a=()=>(c=e.charCodeAt(o++))>>7?c=0:c;o<e.length;)r[t++]=a()<<1|a()>>6,r[t++]=c<<2|a()>>5,r[t++]=c<<3|a()>>4,r[t++]=c<<4|a()>>3,r[t++]=c<<5|a()>>2,r[t++]=c<<6|a()>>1,r[t++]=c<<7|a();return r}base128={EncodeOutput:l,encode:h,decode:s};})();
```

---

## Effect

Encode this jpg file, use base128 is `109.85 KiB` smaller than base64:

```
screenshot-45.519.jpg
file length: 682086

encode:
bytes length: 796961
eval length: 779527
decoded length: 682086
equal: true

base64:
length: 909448
```

![img](img.jpg)
