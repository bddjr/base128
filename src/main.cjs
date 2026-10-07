loadBinding: {
    // 1. Production environment: try loading platform-specific subpackage
    const { platform, arch } = process;
    const isMusl = () => {
        if (process.report && typeof process.report.getReport === "function") {
            try {
                const rep = process.report.getReport();
                if (rep && rep.header && rep.header.glibcVersionRuntime) {
                    return false;
                }
                if (rep && Array.isArray(rep.sharedObjects)) {
                    for (var i = 0; i < rep.sharedObjects.length; i++) {
                        const obj = rep.sharedObjects[i];
                        if (typeof obj === "string" && (obj.indexOf("libc.musl-") !== -1 || obj.indexOf("ld-musl-") !== -1)) {
                            return true;
                        }
                    }
                }
            } catch (e) { }
        }
        try {
            const ldd = require("fs").readFileSync("/usr/bin/ldd", "utf8");
            if (ldd.indexOf("musl") !== -1) {
                return true;
            }
        } catch (e) { }
        return false;
    };

    let bindingPkg;
    if (platform === "darwin") {
        if (arch === "arm64")
            bindingPkg = "darwin-arm64";
        else if (arch === "x64")
            bindingPkg = "darwin-x64";
    } else if (platform === "win32") {
        if (arch === "x64")
            bindingPkg = "win32-x64-msvc";
        else if (arch === "arm64")
            bindingPkg = "win32-arm64-msvc";
    } else if (platform === "linux") {
        if (arch === "x64") {
            bindingPkg = isMusl()
                ? "linux-x64-musl"
                : "linux-x64-gnu";
        } else if (arch === "arm64") {
            bindingPkg = isMusl()
                ? "linux-arm64-musl"
                : "linux-arm64-gnu";
        } else if (arch === "ppc64")
            bindingPkg = "linux-ppc64-gnu";
        else if (arch === "s390x")
            bindingPkg = "linux-s390x-gnu";
    } else if (platform === "android") {
        if (arch === "arm64")
            bindingPkg = "android-arm64";
    } else if (platform === "freebsd") {
        if (arch === "x64")
            bindingPkg = "freebsd-x64";
    } else if (platform === "openharmony") {
        if (arch === "arm64")
            bindingPkg = "openharmony-arm64";
    }

    if (bindingPkg) {
        try {
            module.exports = require(`@base128-ascii/binding-${bindingPkg}/v8.node`);
            break loadBinding;
        } catch (e) { }
        try {
            module.exports = require(`@base128-ascii/binding-${bindingPkg}/napi.node`);
            break loadBinding;
        } catch (e) { }
    }

    // 2. Local development / CI test: load local build if subpackage not loaded
    try {
        module.exports = require("../build/Release/v8.node");
        break loadBinding;
    } catch (e) { }
    try {
        module.exports = require("../build/Release/napi.node");
        break loadBinding;
    } catch (e) { }

    // 3. Fallback: pure JS implementation
    module.exports = require("./browser.mjs");
}
