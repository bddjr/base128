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

```html browser
<script>
{class EncodeOutput{constructor(t){this.uint8Array=t}toString(){return(new TextDecoder).decode(this.uint8Array)}toJSTemplateLiterals(){return`\`${this.toString().replace(/[\r\\`]|\${|<\/script/g,t=>"\r"==t?"\\r":"<\/script"==t?"<\\/script":"\\"+t)}\``}}self.base128={EncodeOutput,encode(t){"string"==typeof t&&(t=(new TextEncoder).encode(t));for(var e=new Uint8Array(Math.ceil(t.length/7*8)),r=0,n=0;r<t.length;)e[n++]=t[r]>>1,e[n++]=t[r++]<<6&127|t[r]>>2,e[n++]=t[r++]<<5&127|t[r]>>3,e[n++]=t[r++]<<4&127|t[r]>>4,e[n++]=t[r++]<<3&127|t[r]>>5,e[n++]=t[r++]<<2&127|t[r]>>6,e[n++]=t[r++]<<1&127|t[r]>>7,e[n++]=127&t[r++];return new EncodeOutput(e)},decode(t){for(var e,r=new Uint8Array(Math.floor(t.length/8*7)),n=0,o=0,c=()=>(e=t.charCodeAt(n++))>>7?e=0:e;n<t.length;)r[o++]=c()<<1|c()>>6,r[o++]=e<<2|c()>>5,r[o++]=e<<3|c()>>4,r[o++]=e<<4|c()>>3,r[o++]=e<<5|c()>>2,r[o++]=e<<6|c()>>1,r[o++]=e<<7|c();return r}}}
</script>
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
