loadBinding: {
    // Production environment: try loading platform-specific subpackage
    const { platform, arch } = process;
    const isMusl = () => {
        if (process.report && typeof process.report.getReport === "function") {
            try {
                return !process.report.getReport().header.glibcVersionRuntime;
            } catch (e) { }
        }
        try {
            return require("node:fs").readFileSync("/usr/bin/ldd", "utf8").includes("musl");
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
        } catch (e) {
            if (e && e.code !== 'MODULE_NOT_FOUND') try {
                module.exports = require(`@base128-ascii/binding-${bindingPkg}/napi.node`);
                break loadBinding;
            } catch (e) { }
        }
    }

    // Fallback: pure JS implementation
    module.exports = require("./browser.mjs");
}
