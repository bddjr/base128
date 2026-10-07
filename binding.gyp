{
  "targets": [
    {
      "target_name": "napi",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
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
          "AdditionalOptions": [ "/utf-8" ]
        }
      }
    },
    {
      "target_name": "v8",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
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
          "AdditionalOptions": [ "/utf-8" ]
        }
      }
    }
  ]
}
