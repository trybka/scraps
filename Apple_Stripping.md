

# Apple Stripping Disambiguation
```
  --[no]objc_enable_binary_stripping (a boolean; default: "false")
    Whether to perform symbol and dead-code strippings on linked binaries.
    Binary strippings will be performed if both this flag and --
    compilation_mode=opt are specified.
```

## Problem

`--objc_enable_binary_stripping` is a confusing flag that conflates _at least_ two meanings of "strip" and also has other side effects. Dead code elimination happens at link time, and a debugging symbol strip action is registered for final executable links.


It's also a source of confusion externally: https://github.com/bazelbuild/bazel/pull/3756

The most confusing side effect is that it _enables_ `-g` (i.e. causes the compiler to emit debug info).

## Background

_Stripping removes inessential information from executable binary programs and object files, thus potentially resulting in better performance and sometimes significantly less disk space usage ('inessential information' means information that is not required for correct functioning of the binary in normal execution). This information may consist of debugging and symbol information; however the standard leaves the scope of changes up to the implementer._[^1]

Concretely, there are three categories of "inessential" information one may wish to remove:

*   Symbols
*   Debug Information
*   Dead Code

Debug information can be removed either by the linker (`-Wl,-S`), or with the `strip -S` command.

Dead code is eliminated by the linker

(with the flag `-dead_strip -no_dead_strip_inits_and_terms`, _hereafter referred to with just <code> -dead_strip</code>_</em>)

Symbols and Debug information can be omitted by most linkers (`-Wl,-s`), however this mode is not supported by ld64.

Symbols may be omitted by the Apple linker (`-Wl,-x`), we _may_ wish to use this as an alternative to the hard-coded `strip` action currently performed

On Apple systems, `strip` has the following behavior:



*   `-S`: as above
*   `-x`: Remove all local symbols (saving only global symbols).
*   No args (implies `-u -r`): Save all undefined symbols (needed for relocation)


### Current Behavior

The current flags behave as follows:

#### <code>objc_enable_binary_stripping</code>

*   `-g` : compile generates debug info
*   `-dead_strip` : linker enabled dead code elimination
*   Registers a `strip` [action](https://cs.opensource.google/bazel/bazel/+/master:src/main/java/com/google/devtools/build/lib/rules/objc/CompilationSupport.java;l=1634;drc=f20fc7e4f220e64765399796dfcbd67f14a59cc5) which runs:
    *   `strip -S` for tests
    *   `strip -x` for dylibs and kexts
    *   `strip` for everything else

NB: (`--objc_enable_binary_stripping` is a no-op without `-c opt`)


#### <code>strip={always,never,sometimes} </code>

*   `-Wl,-S` : linker strips debug info for `cc_binary,cc_test` rules only.

#### <code>apple_generate_dsym</code>

*   `-g`: compile generates debug info
*   `DSYM_HINT_LINKED_BINARY, DSYM_HINT_DSYM_PATH `
    *   These special flags instruct our Clang Driver Wrapper to initiate dSYM generation.
    *   This must happen in the same action as the Link action for the object files to be in the correct paths with their debug information.


## Proposal

Introduce the following new behavior:


#### <code>compilation_mode=opt (-c opt)</code>

*   `-dead_strip` : linker enabled dead code elimination

#### <code>apple_generate_dsym</code>

*   `-g`: compile generates debug info
*   `DSYM_HINT_LINKED_BINARY, DSYM_HINT_DSYM_PATH `(see above)
*   Run `strip -S` immediately after dSYM output
    *   This will happen for all link actions that are performed by the Crosstool
        *   `apple_binary`
        *   `cc_binary`
        *   `swift_binary` and other rules which use the Starlark `cc_common.link` API

#### <code>strip={always,never,sometimes} </code>

*   `-Wl,-S` : linker strips debug info for all crosstool-invoked link actions

Remove `--objc_enable_binary_stripping ` \

Notes:

`--strip` will now strip Apple Binaries (this will be incompatible with debugging). \
For context, prod only sets this for fastbuild, as it is a good option for running tests

This would not be recommended for releases.

In the future, we may want to investigate deploying `strip` in some form to strip all symbols.


### Rollout Strategy


1. Crosstool sets `-dead_strip` for` -c opt`
2. Crosstool adds `objc_executable, objcpp_executable` to the `flag_set` which is activated for <code>strip_debug_symbols</code> (i.e. <code>--strip</code>)
3. Modify clang driver wrapper to invoke <code>xcrun strip</code> if dSYM generation is requested

At this point, the toolchain should be doing the right thing for the above flags, and we can begin to remove usage of<code> --objc_enable_binary_stripping</code>

## Infrequently Asked Questions: 

_Should there be an equivalent<code> .stripped</code> target for Apple?</em>

No. Unlike on Android where we have a stripped and unstripped (w/ debug\_info), the dSYM contains all the necessary information. The distinct target here is irrelevant.


## Appendix


## Strip/ld symbol behavior investigation

`NB: The following is not my (trybka) commentary:`

I moved this from a sidebar comment, because it was getting too long...

Looking into this is making me sad. Fundamentally, apple's tools don't make the same distinction between "dynamic symbol table" and "static symbol table" that is typical in ELF tools.

Thus, "strip -u -r" (aka the default behavior of strip on a binary) removes dynamically-exported symbols. Which entirely breaks a dylib, of course, but also destroys the ability to explicitly export symbols from an executable.

OTOH, "strip -x" works mostly how a linux-user would expect strip to work -- removing only non-exported symbols.

So, I'd now say that strip "-x" and "-S" are the only modes that are really sensible to use.

But there's a big problem with that: binaries on macos are created, by default, with all symbols exported. Contrast to ELF, where binaries have a minimal dynamic symbol table, only containing  definitions that are needed by libraries the binary links against (e.g. due to weak definitions in the library, or reverse dependencies, from lib to binary). You need to use -export-dynamic if you want to export \_all\_ symbols from a binary.

Now, ld does \_have\_ an "-export\_dynamic" flag, similarly to ELF linkers. But, this has different semantics -- it only affects LTO, and causes LTO to \_also\_ treat the implicitly-exported dynamic symbols, as important to keep.

So, all this means that "strip -x" on a macos binary ends up failing to remove most symbols, because they're all defaulted to dynamically exported. Which is hardly ever what anyone wants.

There's no flag to do the useful thing in the linker, and only export required symbols. We could precisely control which symbols get exported with -exported\_symbol, but this isn't feasible to do by default, because we'd need to know every weak symbol, or else we break ODR in C++. (Possible linker feature request: add a new flag, -export\_needed, which reduces the default export set to those symbols needed by linked dylibs. Then we could use that flag to link binaries, and use "strip -x" everywhere. There's some design questions around how that should interact with -exported\_symbol options though.)

Without something like that, if we want to be able to both usefully strip a binary's symbol table, and be able to dynamically export individual symbols as needed for dlsym(), "strip" does have the "-s" argument, to allow preserving individually named symbols. So, we can "strip -u -r" to get rid of all dynamic exports, and use "-s symbol\_list.txt" to preserve a given list of symbols. But it's pretty unsatisfying and annoying that this use of strip is not behavior-preserving for the produced binary!


## Important `ld/strip`

_NB: These pertain specifically to Apple's cctools_

ld (ld64)


<table>
  <tr>
   <td>
<code>-x </code>
   </td>
   <td><code>Do not put non-global symbols in the output file's symbol table. Non-global symbols are useful when debugging and getting symbol names in back traces, but are not used at runtime.</code>
<p>
<code>If -x is used with -r non-global symbol names are not removed, but instead replaced with a unique, dummy name that will be automatically removed when linked into a final linked image.</code>
<p>
<code>This allows dead code stripping, which uses symbols to break up code and data, to work properly and provides the security of having source symbol names removed.</code>
   </td>
  </tr>
</table>

strip


<table>
  <tr>
   <td>

<code>-s </code>
   </td>
   <td><code>Completely strip the output, including removing the symbol table.</code>
<p>
<strong><code>This file format variant is no longer supported.</code></strong>
<strong><code>This option is obsolete.</code></strong>
   </td>
  </tr>
</table>



<!-- Footnotes themselves at the bottom. -->
## Notes

[^1]:
     https://en.wikipedia.org/wiki/Strip_(Unix)
