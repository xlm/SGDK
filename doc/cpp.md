# C++ in this fork

The `cpp` branch adds a small freestanding C++ runtime and a sample. These
fork-only additions are separate from the public-header, makefile and startup
changes intended for upstream SGDK.

## Linux toolchain

Use GCC 10 or newer with an m68k C++ compiler. For example, on Ubuntu:

```sh
sudo apt-get update
sudo apt-get install g++-m68k-linux-gnu binutils-m68k-linux-gnu build-essential cmake default-jdk
export GDK="$HOME/SGDK" # Point this at your checkout of the cpp branch.
export PREFIX=m68k-linux-gnu-
export CC='m68k-linux-gnu-gcc -Wl,--build-id=none'
export LIBGCC="$GDK/lib/libgcc.a"
export LIB="$HOME/sgdk-build/lib"
mkdir -p "$LIB"
```

Ubuntu GCC enables ELF build IDs by default. `--build-id=none` prevents the
build-ID note from overlapping SGDK's ROM vectors. Ubuntu's cross compiler
does not provide a separate 68000 libgcc multilib. Its default libgcc may use
instructions for a later CPU, so use SGDK's bundled 68000-built archive.
Do not link Linux startup objects, libc or libstdc++ into a ROM.

Build the native tools outside the checkout and put them on `PATH`:

```sh
for tool in bintos sjasm xgmtool; do
    cmake -S "$GDK/tools/$tool" -B "$HOME/sgdk-build/$tool"
    cmake --build "$HOME/sgdk-build/$tool" -j2
    export PATH="$HOME/sgdk-build/$tool:$PATH"
done
make -C "$GDK/tools/convsym" BUILD_DIR="$HOME/sgdk-build/convsym"
export PATH="$HOME/sgdk-build/convsym:$PATH"
```

`convsym` requires a native C++20 compiler. Java runs rescomp and sizebnd.
Other m68k toolchains can use `PREFIX` and `TOOLCHAIN_DIR`, or explicit `CC`
and `CXX` overrides. They must support `-m68000` and provide compatible GCC
LTO tools. The bundled Windows toolchain has no `g++`, so it cannot build C++
projects without a separately installed compiler and matching binutils.

## Build the sample

With the environment above:

```sh
make -C "$GDK" -f makelib.gen -j2
make -C "$GDK" -f makelib.gen debug -j2
make -C "$GDK/sample/cpp/hello-cpp" -f "$GDK/makefile.gen" -j2
make -C "$GDK/sample/cpp/hello-cpp" -f "$GDK/makefile.gen" debug -j2
```

ROMs are in `sample/cpp/hello-cpp/out/release/rom.bin` and
`out/debug/rom.bin`. The screen should say `Hello from C++!` and
`C++ runtime OK`. The sample uses the normal `src/` and `res/` layout.
Unlike some existing samples, it does not commit generated ROMs or other
toolchain-specific build output.

Projects can mix `.c` and `.cpp` files. A project with any `.cpp` sources
automatically compiles `src/cpp/runtime.cpp` from the SDK into its own output
directory. The C-only `libmd` build never invokes `g++`, and pure C projects
do not link the runtime.

## Language and runtime

C++ compilation inherits the project's C flags, including release LTO or
debug information, and adds:

- `-fno-exceptions`: no exception unwinding or `try`/`catch`.
- `-fno-rtti`: no `dynamic_cast` or `typeid`. Virtual methods still work.
- `-fno-threadsafe-statics`: function-local statics initialize on first use
  without locking. Do not race their initialization from an interrupt.

The language version is the compiler's default. Set, for example,
`EXTRA_FLAGS=-std=gnu++17` for a C++-only project to select C++17 explicitly.
Do not pass that flag to a mixed C/C++ project. Instead override `CXXFLAGS`
with the complete project flags if an explicit dialect is needed there.

Classes, virtual functions, templates, `constexpr`, lambdas, global objects,
function-local statics and ordinary `new`/`delete` are supported. Include
`<new>` **before** `<genesis.h>` because SGDK defines macros such as `size_t`.
This does not make all standard headers compatible: Ubuntu's `<cstddef>`,
for example, conflicts with SGDK's own `size_t` typedef. The sample wraps its
resource header in
`extern "C"` so it also works with the older bundled rescomp.jar. Rescomp
built from this branch's Java sources emits these guards itself.

The runtime supplies scalar/array, sized and nothrow allocation functions,
using `MEM_alloc` and `MEM_free`. Zero-byte requests allocate at least one
byte. An allocation header and padding preserve the compiler's default new
alignment despite SGDK's word-aligned allocator. Requests that exceed the
16-bit allocator's limit, including this overhead, are rejected before
narrowing. With Ubuntu GCC's four-byte default alignment the maximum request
is 65,525 bytes. Available RAM usually imposes a lower limit.
Ordinary allocation failure calls `SYS_die`, since throwing `std::bad_alloc`
is not available. `new (std::nothrow)` returns null on failure. Null deletion
is safe. Over-aligned allocation is not provided.

Automatic objects and explicitly deleted objects run their destructors.
Global and function-local static destructors **never run**. A console game
does not exit, so `__cxa_atexit` ignores destructor registration and
`__dso_handle` is only an ABI token. Pure virtual calls stop via `SYS_die`.
Hard reset runs legacy `.ctors` in reverse order, then `.init_array` forward,
after SGDK initialization. Soft reset preserves RAM and does not rerun them.

There is no linked STL/libstdc++, exception runtime, OS, threads or hosted
I/O. Some compiler headers provide inline-only facilities, but arbitrary
standard-library use is not supported. Use SGDK APIs for memory, graphics,
input and sound.

## Fork CI

Run the host-side allocation and fatal-path checks with
`sh tests/cpp/runtime.sh "$HOME/sgdk-build"`. They use a word-aligned fake
SGDK allocator and do not replace the m68k sample or emulator checks.

`.github/workflows/cpp.yml` builds the libraries and sample in release and
debug on pushes and pull requests targeting `cpp`. Its weekly run checks
both `cpp` itself and a temporary rebase onto `Stephane-D/SGDK` master.
Conflicts or build errors fail CI. Nothing is pushed by the workflow.

GitHub only runs schedules from the repository's **default branch**. To
activate the weekly check, make `cpp` the fork's default branch, or put the
workflow on the fork's default branch too. A manual dispatch is also
available for a normal branch build.
