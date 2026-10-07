try {
    module.exports = require("node-gyp-build")(require("node:path").join(__dirname, ".."));
} catch (e) {
    module.exports = require("./browser.mjs");
}
