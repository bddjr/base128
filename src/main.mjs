import { createRequire } from "node:module";

/** @type {typeof import("./main.d.ts")} */
export const {
    _impl,
    EncodeResult,
    encode,
    decode,
    parseJSTemplateLiterals
} = (() => {
    const require = createRequire(import.meta.url);

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

    let bindingName;
    if (platform === "darwin") {
        if (arch === "arm64")
            bindingName = "darwin-arm64";
        else if (arch === "x64")
            bindingName = "darwin-x64";
    } else if (platform === "win32") {
        if (arch === "x64")
            bindingName = "win32-x64-msvc";
        else if (arch === "arm64")
            bindingName = "win32-arm64-msvc";
    } else if (platform === "linux") {
        if (arch === "x64") {
            bindingName = isMusl()
                ? "linux-x64-musl"
                : "linux-x64-gnu";
        } else if (arch === "arm64") {
            bindingName = isMusl()
                ? "linux-arm64-musl"
                : "linux-arm64-gnu";
        } else if (arch === "ppc64")
            bindingName = "linux-ppc64-gnu";
    } else if (platform === "android") {
        if (arch === "arm64")
            bindingName = "android-arm64";
    } else if (platform === "freebsd") {
        if (arch === "x64")
            bindingName = "freebsd-x64";
    } else if (platform === "openharmony") {
        if (arch === "arm64")
            bindingName = "openharmony-arm64";
    }

    if (bindingName) try {
        return require(`@base128-ascii/binding-${bindingName}/v8.node`);
    } catch (e) {
        if (e && e.code !== 'MODULE_NOT_FOUND') try {
            return require(`@base128-ascii/binding-${bindingName}/napi.node`);
        } catch (e) { }
    }

    // Fallback: pure JS implementation
    return require("./pure.mjs");
})();

export default {
    _impl,
    EncodeResult,
    encode,
    decode,
    parseJSTemplateLiterals
};
