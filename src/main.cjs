try {
    // 1. Production environment: load platform-specific subpackage directly
    module.exports = require(`@base128-ascii/binding-${process.platform}-${process.arch}`);
} catch (e) {
    // 2. Local development / CI test: load local build
    try {
        module.exports = require("../build/Release/base128.node");
    } catch (e) {
        // 3. Fallback: pure JS implementation
        module.exports = require("./browser.mjs");
    }
}
