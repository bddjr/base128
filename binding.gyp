{
  "targets": [
    {
      "target_name": "napi",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags_cc": [
        "-O3",
        "-fomit-frame-pointer",
        "-fno-rtti",
        "-fvisibility=hidden",
        "-flto"
      ],
      "ldflags": [
        "-flto",
        "-Wl,--gc-sections"
      ],
      "xcode_settings": {
        "GCC_OPTIMIZATION_LEVEL": "3",
        "CLANG_CXX_OPTIMIZATION_LEVEL": "3",
        "GCC_ENABLE_CPP_RTTI": "NO",
        "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
        "LLVM_LTO": "YES"
      },
      "sources": [ "src-napi/addon.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "Optimization": 3,
          "FavorSizeOrSpeed": 1,
          "InlineFunctionExpansion": 2,
          "BufferSecurityCheck": "false",
          "StringPooling": "true",
          "FunctionLevelLinking": "true",
          "WholeProgramOptimization": "true",
          "AdditionalOptions": [ "/utf-8" ]
        },
        "VCLinkerTool": {
          "LinkTimeCodeGeneration": 1,
          "OptimizeReferences": 2,
          "EnableCOMDATFolding": 2
        }
      }
    },
    {
      "target_name": "v8",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags_cc": [
        "-O3",
        "-fomit-frame-pointer",
        "-fno-rtti",
        "-fvisibility=hidden",
        "-flto"
      ],
      "ldflags": [
        "-flto",
        "-Wl,--gc-sections"
      ],
      "xcode_settings": {
        "GCC_OPTIMIZATION_LEVEL": "3",
        "CLANG_CXX_OPTIMIZATION_LEVEL": "3",
        "GCC_ENABLE_CPP_RTTI": "NO",
        "GCC_SYMBOLS_PRIVATE_EXTERN": "YES",
        "LLVM_LTO": "YES"
      },
      "sources": [ "src-napi/addon.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS", "USE_V8_ACCELERATION" ],
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1,
          "Optimization": 3,
          "FavorSizeOrSpeed": 1,
          "InlineFunctionExpansion": 2,
          "BufferSecurityCheck": "false",
          "StringPooling": "true",
          "FunctionLevelLinking": "true",
          "WholeProgramOptimization": "true",
          "AdditionalOptions": [ "/utf-8" ]
        },
        "VCLinkerTool": {
          "LinkTimeCodeGeneration": 1,
          "OptimizeReferences": 2,
          "EnableCOMDATFolding": 2
        }
      }
    }
  ]
}
