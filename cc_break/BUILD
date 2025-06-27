cc_library(
    name = "broken",
    srcs = ["broken.cc"],
    hdrs = ["src/nested/broken.h"],
    textual_hdrs = ["src/nested/text.h"],
    copts = [
        "-Inested",
        "-I$(GENDIR)/nested",
        "-I.",
    ],
    strip_include_prefix = "src",
    includes = [
        ".",
        "nested",
    ],
)
