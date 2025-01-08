Smaller than Base64, only use ASCII, can run in web browser.

---

Char Code (ASCII) :

```js
'\0\1\2\3\4\5\6\7\b\t\n\v\f\r\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7F';
```

The first byte indicates the length of the last incomplete block, 0 means no incomplete block.

```js
`4LF3 `
`0LF3!Tl7`
```

Test effect, Base128 is 102KiB smaller than Base64:

```
screenshot-45.519.jpg
file length: 682086

encodeToTemplateLiterals:
bytes length: 804861
eval length: 779528
decoded length: 682086
equal: true

base64:
length: 909448
```

---

The project was born for [vite-plugin-singlefile-compression](https://github.com/bddjr/vite-plugin-singlefile-compression).

Made by bddjr, using MIT License.
