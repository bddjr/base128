function isMusl() {
    if (process.report && typeof process.report.getReport === "function") {
        try {
            var rep = process.report.getReport();
            if (rep && rep.header && rep.header.glibcVersionRuntime) {
                return false;
            }
            if (rep && Array.isArray(rep.sharedObjects)) {
                for (var i = 0; i < rep.sharedObjects.length; i++) {
                    var obj = rep.sharedObjects[i];
                    if (typeof obj === "string" && (obj.indexOf("libc.musl-") !== -1 || obj.indexOf("ld-musl-") !== -1)) {
                        return true;
                    }
                }
            }
        } catch (e) {}
    }
    try {
        var ldd = require("fs").readFileSync("/usr/bin/ldd", "utf8");
        if (ldd.indexOf("musl") !== -1) {
            return true;
        }
    } catch (e) {}
    return false;
}

function getBindingPackage() {
    var platform = process.platform;
    var arch = process.arch;

    if (platform === "darwin") {
        if (arch === "arm64") return "@base128-ascii/binding-darwin-arm64";
        if (arch === "x64") return "@base128-ascii/binding-darwin-x64";
    } else if (platform === "win32") {
        if (arch === "x64") return "@base128-ascii/binding-win32-x64-msvc";
        if (arch === "arm64") return "@base128-ascii/binding-win32-arm64-msvc";
    } else if (platform === "linux") {
        if (arch === "x64") {
            return isMusl()
                ? "@base128-ascii/binding-linux-x64-musl"
                : "@base128-ascii/binding-linux-x64-gnu";
        }
        if (arch === "arm64") {
            return isMusl()
                ? "@base128-ascii/binding-linux-arm64-musl"
                : "@base128-ascii/binding-linux-arm64-gnu";
        }
        if (arch === "arm") return "@base128-ascii/binding-linux-arm-gnueabihf";
        if (arch === "ppc64") return "@base128-ascii/binding-linux-ppc64-gnu";
        if (arch === "s390x") return "@base128-ascii/binding-linux-s390x-gnu";
    } else if (platform === "android") {
        if (arch === "arm64") return "@base128-ascii/binding-android-arm64";
        if (arch === "arm") return "@base128-ascii/binding-android-arm-eabi";
    } else if (platform === "freebsd") {
        if (arch === "x64") return "@base128-ascii/binding-freebsd-x64";
    } else if (platform === "openharmony") {
        if (arch === "arm64") return "@base128-ascii/binding-openharmony-arm64";
    }
    return null;
}

// 1. Production environment: try loading platform-specific subpackage
var bindingPkg = getBindingPackage();
if (bindingPkg) {
    try {
        module.exports = require(bindingPkg);
    } catch (e) {}
}

// 2. Local development / CI test: load local build if subpackage not loaded
if (!module.exports || !module.exports.encode) {
    try {
        module.exports = require("../build/Release/base128.node");
    } catch (e) {
        // 3. Fallback: pure JS implementation
        module.exports = require("./browser.mjs");
    }
}
