# Writing your first C++ Testing Toolchain

Audience: You maintain a bespoke platform that needs to do special setup to run
a `cc_test`.

NOTE: We assume familiarity with extending Bazel and with `cc_test` rules.

This was written for Google, but the basic steps should apply for Bazel. I will
leave Bazel WORKSPACE and repo management as an exercise for the reader.

## Background

`cc_test` as implemented in Starlark provides a rich interface for extending its
Test Runner environment. This was born out of a need to run tests targeting
platforms which may not otherwise be suitable Exec Platforms. Think Android
emulators, or Browsers for WebAssembly.

We were able to use a new feature of Bazel:
[Toolchains](https://bazel.build/extending/toolchains).

When we ported `cc_test` from "Native (Java)" to Starlark, we built-in
extensibility from the start, enabling platform maintainers to write a custom
cc_test toolchain in Bazel describing how to run a test on their platform.

TODO: Add original design doc.

TODO: Add existing examples.

Some good things to be familiar with before proceeding:

*   https://bazel.build/extending/platforms
*   https://bazel.build/extending/rules
*   https://bazel.build/extending/toolchains

## Getting Started

Create a new package for this toolchain:

```shell
$ mkdir -p experimental/users/$USERNAME/cc_test_toolchain
```

## BUILD Rules

```shell
$ touch experimental/users/$USERNAME/cc_test_toolchain/BUILD
```

In the BUILD file, we're going to instantiate our new toolchain, and bind it
with a `toolchain()` rule.

TIP: You may want to read up on
[Platforms](https://bazel.build/extending/platforms), now, as we're going to be
working with `constraints` here.

Don't worry, we'll define our toolchain later.

Edit the BUILD file:

```python
# BUILD file
load(":toolchain.bzl", "my_cool_toolchain")

my_cool_toolchain(
    name = "cool_prod_linux_runner",
)

toolchain(
    name = "prod_linux_toolchain",
    exec_compatible_with = [
        # TODO: Where are these for Bazel?
        "//third_party/bazel_platforms/os:linux",
    ],
    target_compatible_with = [
        # TODO: Where are these for Bazel?
        "//third_party/bazel_platforms/os:linux",
    ],
    toolchain = ":cool_prod_linux_runner",
    # TODO: Where are these for Bazel?
    toolchain_type = "//tools/cpp:test_runner_toolchain_type",
)
```

## Create the Toolchain

```shell
$ touch experimental/users/$USERNAME/cc_test_toolchain/toolchain.bzl
```

TIP: Be sure you've read [Rules](https://bazel.build/extending/rules) and
[Toolchains](https://bazel.build/extending/toolchains) by this point.

### Definitions

We're going to be defining a toolchain for `cc_test`.

First let's talk about what `cc_test` is going to need to be able to build/run.

*   Runner: Optional, this is where you can setup an environment, spin up an
    emulator, etc. This is optional since you might just be executing your test
    binary in an Exec Platform that is already setup. But if you need to run in
    a constrained or otherwise simulated environment, the "Runner" is what does
    that for you. In some sense, this is where the "magic" is&mdash;we slip a
    partially-bound function to `cc_test`. This allows you to change the
    behavior of `cc_test` for your platform without a Bazel release.

*   Link options: (See [`cc_test.linkopts`](http://go/be#cc_test.linkopts)) A
    test toolchain should allow you to override how tests get linked by default
    in this configuration. Imagine linking in extra system runtime libraries,
    disabling stripping, etc. These linkopts get merged with the BUILD,
    command-line, and cc_toolchain linkopts later.

*   Static / Dynamic linking: Decide if you want your `cc_test` executable to be
    statically or dynamically linked.

Our toolchain is going to encapsulate these concepts in a way that the `cc_test`
rule implementation can digest. We'll be creating a "Provider" which will be
read in the `cc_test` rule to do its final setup.

*   `get_runner`: A Starlark struct that is effectively a partially-bound
    function. This is where the "magic," such as file creation, happens.
*   `linkopts`: See above.
*   `linkstatic`: See above.
*   `use_legacy_cc_test`: As the name implies, this is a hook to just force
    usage of the legacy cc_test which just executes the test. We won't be making
    use of this.

As mentioned above, `get_runner` is a struct that has a `func` and `args`. 

TODO: Show how we we partially bind some arguments we construct

`func` is called with the following arguments:
*   `ctx`: the `RuleContext` of the cc_test itself
*   `binary_info`: a `struct` that mimics `DefaultInfo` from the `cc_binary`
    (e.g. it has `files`, `runfiles`, etc.). The reason this is not just a
    `DefaultInfo` is that the `DefaultInfo` is an opaque type that is harder to
    manipulate. As such, `cc_binary_impl` (which `cc_test` calls) just returns
    this struct instead, and `cc_binary` and `cc_test` rules are responsible for
    making the final `DefaultInfo`.
*   `processed_environment`: This is the initial environment for the test as
    calculated by Bazel, taking into account flags and attributes. It is
    provided so that your rule can extend it as needed.
*   `*args`: Pass in the remaining `args` from `get_runner`.

### Example

Edit your toolchain.bzl:

TODO: Where is CcTestRunnerInfo coming from?

```bzl
"""
Simple toolchain which overrides env and exec requirements.
"""
load(":toolchain.bzl", "CcTestRunnerInfo")

def _get_runner(
        ctx,
        binary_info,
        processed_environment,
        execution_requirements,
        test_environment):
    test_env = {"COOL": "value"}
    test_env.update(test_environment)
    test_env.update(processed_environment)

    return [
        DefaultInfo(
            files = binary_info.files,
            # Here is where we would return our own runner.
            executable = binary_info.executable,
            runfiles = binary_info.runfiles,
        ),
        testing.ExecutionInfo(execution_requirements or {}),
        testing.TestEnvironment(
            environment = test_env,
        ),
    ]

def _my_cool_toolchain_impl(ctx):
    return [platform_common.ToolchainInfo(
        cc_test_info = CcTestRunnerInfo(
            get_runner = struct(
                func = _get_runner,
                args = {
                    "execution_requirements": ctx.attr.execution_requirements,
                    "test_environment": ctx.attr.test_environment,
                },
            ),
            linkopts = [],
            linkstatic = True,
            use_legacy_cc_test = False,
        ),
    )]

my_cool_toolchain = rule(
    implementation = _my_cool_toolchain_impl,
    attrs = {
        "execution_requirements": attr.string_dict(),
        "test_environment": attr.string_dict(),
    }
)
```

#### Let's try it out!

While you typically will register toolchains in the top-level WORKSPACE file,
let's play around with it locally first (with `--extra_toolchains`):

```sh
$ bazel test \
--subcommands --nocache_test_results \
--extra_toolchains=//experimental/users/$USERNAME/cc_test_toolchain:prod_linux_toolchain \
//some:example_cc_test
```

(Eventually) you should see the test execution, and your modified environment:

```
(17:23:14) SUBCOMMAND: # //some:example_cc_test [action 'Testing //some:example_cc_test', ...]
(cd ... && \
  exec env - \
    COOL=value \
...
```

Now you can modify your BUILD file to set test_environment and
execution_requirements attributes which your toolchain has.

(The `load` and `toolchain` statements remain the same).

```build
# BUILD file
load(":toolchain.bzl", "my_cool_toolchain")

# Modified.
my_cool_toolchain(
    name = "cool_prod_linux_runner",
    execution_requirements = {
        # Too cool for Distributed Build.
        "local": "1",
    },
    test_environment = {
        "SUPER_COOL": "better_value",
    },
)

toolchain(
    name = "prod_linux_toolchain",
    exec_compatible_with = [
        "//third_party/bazel_platforms/os:linux",
    ],
    target_compatible_with = [
        "//third_party/bazel_platforms/os:linux",
    ],
    toolchain = ":cool_prod_linux_runner",
    toolchain_type = "//tools/cpp:test_runner_toolchain_type",
)
```

Observe `SUPER_COOL` in subcommands, and now the test will be forced to run
locally.

Congrats, you've written a cc_test toolchain!

## Next Steps

### Build out your toolchain

After you've tried your toy toolchain, it's time to clean it up, give it a real
name, and maybe make it do more interesting things.

If your test environment could easily be expressed as a `--run_under` flag,
consider using our toolchain in //.../cc_test/toolchain.bzl. (See examples
in the sibling BUILD file.)

### Register your toolchain

Toolchains must be registered in the WORKSPACE file.

Please see <snipped> for more information.
