# How do Features work, anyway?

Author: trybka@

SUMMARY: This page is intended for Rule and/or Toolchain maintainers. If you're
looking for information about features end users may wish to set, please go here
instead: <TODO EXTERNAL LINK>

[TOC]

## What is a `feature`?

'Feature' is an unfortunately overloaded term.

There are really two kinds of features in bazel:

1.  [Rule Features](https://bazel.build/reference/be/common-definitions#common-attributes)
1.  [C++ Toolchain Features](https://bazel.build/docs/cc-toolchain-config-reference#features)

Rule Features are processed differently than C++ Toolchain features, but they
are related.

## Rule Features

A feature is a string tag that can be enabled or disabled (On or Off). Prefixing
the tag with a `'-'` disables it (Off).

It does not need to be pre-declared. It is just a string (like `tags`). If it
appears in the target attribute, package declaration, or command-line then it
*exists*.

See [Rule Feature Interactions](#rule-feature-interactions) for what happens
when those names intersect.

NOTE: For Rule Features, the set of enabled/disabled features is always computed
at the target level.

A feature can be enabled (or disabled) in one of three places:

*   Globally: (via `--features` command-line flag)
*   At the package level (via the `features` attribute of the
    [`package`](https://bazel.build/reference/be/functions#package) declaration)
*   At the rule level (via the `features`
    [attribute](https://bazel.build/reference/be/common-definitions#common-attributes)
    common to all rules)

The set of computed features for a target context includes all features
mentioned at the individual target, package level, or command-line, omitting any
`'-'` prefix.

IMPORTANT: *This is not a set of the entire "Universe" of features, and does not
consider the C++ Toolchain features at all.*

<section class="zippy">
Definitions

*   $$GlobalOn$$ -- All features enabled in the expanded canonical command-line
    (includes `--config` and `.bazelrc` files).
*   $$GlobalOff$$ -- All features disabled in the expanded canonical
    command-line (without the `'-'` prefix).
*   $$PackageOn$$ -- All features enabled in the `package()` declaration
    containing the given target.
*   $$PackageOff$$ -- All features disabled in the `package()` declaration
*   $$RuleOn$$ -- All features enabled in the `features` attribute of the given
    target.
*   $$RuleOff$$ -- All features disabled in the `features` attribute of the
    given target (without the `'-'` prefix).

</section>

$$Computed Features = GlobalOn \cup GlobalOff \cup PackageOn \cup PackageOff \cup RuleOn \cup RuleOff$$

$$Target Enabled = (((GlobalOn \cup PackageOn ) - PackageOff) \cup RuleOn) - RuleOff) - GlobalOff)$$

$$Target Disabled = ( Computed Features - Target Enabled )$$

### Use in Starlark

Rule features can be read in Starlark from RuleContext (`ctx`):

*   `ctx.features` -- Enabled features
*   `ctx.disabled_features` -- Disabled features

NOTE: There are limitations in the Starlark API for features.

From Starlark you can only inspect the fully resolved enabled/disabled features
for a target, you can't disambiguate which ones were enabled in the package vs.
the target or the command line vs. those other two.

So, for example, you cannot allowlist a feature's usage from the command-line
while disallowing it in a BUILD file.

## C++ Toolchain Features

> WARNING:
>
> Here be dragons. 🐉
>
> This section is primarily useful for C++ Toolchain Maintainers

C++ Toolchain features are a mechanism in CcToolchain definitions for describing
constraints. These constraints control when certain flags or actions are used.

e.g. `"asan"` might control whether we set `-fsanitize=address` in a
`cc_compile` action.

Features in the C++ Toolchain are more than just a string tag and
enabled/disabled bit. They describe requirements, can enable other features, and
can define sets of flags or environment variables to be applied to actions.

TODO: Add Bazel Docs for defining flags/envsets in features / example.

### Rule Feature Interaction

Rule Features don't care about C++ Toolchain features. In fact, you can use
features in bazel without caring about C++. However, they do interact. For C++
rules (e.g. `cc_binary`), the set of enabled rule features becomes a set of
"requested" C++ Toolchain features, and the set of disabled rule features
becomes a set of "unsupported" C++ Toolchain features. If the "requested"
feature meets the additional requirements specified in the toolchain, then it is
considered to be "enabled" from the Crosstool point-of-view. Likewise, C++
Toolchain feature declarations are not taken into consideration for a
RuleContext feature list. (C++ Toolchain features which are enabled in the
toolchain do not appear in `ctx.features`).

If you intend to intersperse rule features with Crosstool features, special care
/ handling may be required. See e.g.
[rules_swift/.../features.bzl](https://github.com/bazelbuild/rules_swift/blob/9cd260597aa4102bb1fa8a7a42f9fceb66c726e9/swift/internal/features.bzl#L203-L226)

### Defining C++ Toolchain Features

<section class="zippy">

The `feature()` macro constructs a `FeatureInfo` provider.

(See https://github.com/bazelbuild/rules_cc/blob/6a2520bed08bbe47a8afcf4c43d51c2533d79ed8/cc/cc_toolchain_config_lib.bzl#L379-L423)

</section>

The simplest form of a feature is just a name:

```bzl
feature(name = "c++17")
```

You can enable it by default:

```bzl
feature(
  name = "c++17",
  enabled = True
)
```

<section class="zippy">
Default-enabled C++ Toolchain features can still be disabled by C++ rules if
requested, e.g. with `--features=-c++17`, or in the BUILD rule.

```bzl
# //my/library/BUILD

cc_library(
  name = "cxx14-only-lib",
  features = ["-c++17"],
  ...
)

```

</section>

In the absence of any signal from rules, packages, or the command-line, the
Toolchain is free to enable or disable features how it wishes. That is to say,
if `"-c++17"` did not appear in this BUILD file, it would be absent from both
`ctx.features` and `ctx.disabled_features`, but could still be enabled by the
toolchain.

However, if it is requested to be disabled (as above), then that "request" is
handled by the toolchain as an additional constraint.

### C++ Toolchain Feature Relationships

Features have three attributes which define various constraints and
relationships.

Bazel has some documentation on
[Feature relationships](https://bazel.build/docs/cc-toolchain-config-reference#feature-relationships)
as well.

`requires` and `provides` describe *when* and *if* a `feature` can be enabled.
`implies` enables some set of features when the given feature is enabled.

--------------------------------------------------------------------------------

`requires`

A list of `feature_sets` defining when this feature is supported by the
toolchain. The feature is supported if any of the feature sets fully apply, that
is, when all features of a feature set are enabled. If `requires` is omitted,
the feature is supported independently of which other features are enabled.

A feature cannot be enabled unless it is supported by the toolchain as described
above.

TODO: example.

--------------------------------------------------------------------------------

`implies`

A string list of features or action configs that are automatically enabled when
this feature is enabled. This mechanism is useful for defining an "umbrella"
feature that can enable other functionality.

IMPORTANT: If any of the implied features or action configs cannot be enabled,
this feature will (silently) not be enabled either.

TODO: example.

--------------------------------------------------------------------------------

`provides`

A list of names this feature conflicts with.

Indicates that this feature is one of several mutually exclusive alternate
features, as indicated by a common string in the provides attribute.

Use this in order to ensure that incompatible features cannot be accidentally
activated at the same time, leading to hard to diagnose compiler errors.

For example, all sanitizers could specify `provides = ["sanitizer"]`.

If the user asks for two or more mutually exclusive features at once, bazel will
throw an error:

```
Error in configure_features: Symbol sanitizer is provided by all of the following features: address_sanitizer thread_sanitizer
```

TODO: example.

--------------------------------------------------------------------------------

## Using C++ Toolchain Features

### BUILD configuration (`select` / `config_setting`)

Users may wish to do things like define `config_settings` for features, e.g. to
`select()` different sets of dependencies.

You might be tempted to write the following `config_setting`:

```bzl
config_setting(
  name = "threads_enabled",
  values = {"features": "enable_threads"},
)
```

However this will not work like you expect.

This will only tell you if a feature is "requested" on the command-line.

It will not tell you if a feature was enabled in the package, and it will not
tell you if the feature was "requested" but ultimately could not be enabled
because of some other constraint the Toolchain does not meet. For example,
`enable_threads` could be gated on a particular target variant, like `wasm` vs.
`asmjs`.

If you wish to test if a feature that exists in the toolchain is actually
enabled, you need a custom flag.

```bzl
load("//tools/cpp:config_flags.bzl", "crosstool_feature_state")

crosstool_feature_state(
  name = "enable_threads",
  enabled = "threads_on",
  disabled = "threads_off",
)
```

This will create two config_setting(s), one each for `enabled` and `disabled`.

### Referencing C++ Toolchain Features in Starlark Rules

TIP: Remember, *Rule* features can be accessed in the RuleContext (`ctx`).

`ctx.features` and `ctx.disabled_features` form the basis of "requested" and
"unsupported" features (respectively), from the perspective of the C++
Toolchain.

To compute the configuration of enabled/disabled C++ Toolchain features for the
current CcToolchain:

```bzl
# Pre-requisites: You have retrieved the
#   CcToolchainInfo into `cc_toolchain`.
feature_configuration = cc_common.configure_features(
  ctx = ctx,
  cc_toolchain = cc_toolchain,
  requested_features = ctx.features,
  unsupported_features = ctx.disabled_features
)
```

This `feature_configuration` object is then used in many of the other
`cc_common` methods.

See the full Bazel
[Documentation for `cc_common`](https://bazel.build/rules/lib/cc_common).

Notably, this would allow you to check if a Toolchain feature is enabled.

```bzl
threads_enabled = cc_common.is_enabled(
  feature_configuration = feature_configuration,
  feature_name = 'enable_threads'
)
```

WARNING: A Rule-enabled feature cannot be enabled in the toolchain if it does
not exist in the toolchain. Thus, `cc_common.is_enabled` will always return
False for features that do not exist in the toolchain you are querying.
