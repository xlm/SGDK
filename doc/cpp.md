# C++ in this fork

The `cpp` branch adds a small freestanding C++ runtime and a sample. These
fork-only additions are separate from the public-header, makefile and startup
changes intended for upstream SGDK.

## Linux toolchain

Use GCC 10 or newer with an m68k C++ compiler. For example, on Ubuntu:

```sh
sudo apt-get update
sudo apt-get install g++-m68k-linux-gnu binutils-m68k-linux-gnu build-essential cmake default-jdk
export GDK="${GDK:-$HOME/SGDK}" # Point this at your checkout of the cpp branch.
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

After building the native tools, select the Ubuntu cross toolchain:

```sh
export PREFIX=m68k-linux-gnu-
export CC='m68k-linux-gnu-gcc -Wl,--build-id=none'
export LIBGCC="$GDK/lib/libgcc.a"
```

### Windows (untested)

The bundled Windows toolchain has no `g++`. We recommend WSL2 with Ubuntu
and following the Linux setup and build steps above and below. This WSL2
approach has not been tested.

### macOS

Use Docker with Ubuntu 24.04. On Apple Silicon, its arm64 image and
`g++-m68k-linux-gnu` package do not require amd64 emulation. With Docker
Desktop or another Docker runtime running, start a shell from the root of
your `cpp` checkout:

```sh
docker run --rm -it -v "$PWD:/sgdk" -w /sgdk -e GDK=/sgdk ubuntu:24.04 bash
```

If Docker Hub rate-limits the pull, use `mirror.gcr.io/library/ubuntu:24.04`
instead. In the container, follow the Linux setup and build steps above and
below, omitting `sudo` because the container runs as root. The ROMs stay in
the mounted checkout after the container exits.

For a native build on Apple Silicon, use Xcode Command Line Tools and the
Homebrew `dciabrin/ngdevkit` tap. Its `m68k-neogeo-elf-g++` has the C++
headers, `-m68000` and LTO support needed to build both configurations.
From the checkout root:

```sh
brew tap dciabrin/ngdevkit
brew trust dciabrin/ngdevkit
brew install cmake openjdk@17 dciabrin/ngdevkit/ngdevkit-toolchain
export PATH="$(brew --prefix openjdk@17)/bin:$PATH"
export GDK="$PWD"
export LIB="$HOME/sgdk-build/lib"
mkdir -p "$LIB"
export PREFIX=m68k-neogeo-elf-
export LIBGCC="$GDK/lib/libgcc.a"
```

Build the native tools as above, then build the sample as below. Skip the
Ubuntu cross toolchain settings. Homebrew's core `m68k-elf-gcc` builds both
`libmd` variants but does not ship `<new>`, so it cannot build the sample.
Use separate checkouts for different toolchains to avoid reusing old objects.

Open the sample's `out/release/rom.bin` in a Mega Drive / Genesis emulator,
such as RetroArch with Genesis Plus GX, to check for `C++ runtime OK`.

## Build the sample

With the environment above:

```sh
make -C "$GDK" -f makelib.gen -j2 LIB="$LIB"
make -C "$GDK" -f makelib.gen debug -j2 LIB="$LIB"
make -C "$GDK/sample/cpp/hello-cpp" -f "$GDK/makefile.gen" -j2 LIB="$LIB"
make -C "$GDK/sample/cpp/hello-cpp" -f "$GDK/makefile.gen" debug -j2 LIB="$LIB"
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
- `-faligned-new=2`: GCC's default `new` alignment matches SGDK's two-byte
  allocator alignment.

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
byte. Allocation returns `MEM_alloc`'s pointer directly, without an extra
header or padding. Requests above 65,532 bytes are rejected before narrowing
to the 16-bit allocator's argument. Available RAM usually imposes a lower
limit.
Ordinary allocation failure calls `SYS_die`, since throwing `std::bad_alloc`
is not available. `new (std::nothrow)` returns null on failure. Null deletion
is safe. Over-aligned allocation is not provided.

Alignment means an address must be a multiple of a number of bytes. With
`-m68000`, ordinary longs, long longs, floats, doubles, pointers and structs
of longs need only two-byte alignment. Explicit `alignas` or
`__attribute__((aligned(...)))` above two bytes needs an aligned allocation
overload that this runtime does not provide. For example, `new` on
`struct alignas(4) X` fails to link with:

```text
undefined reference to `operator new(unsigned int, std::align_val_t)'
```

Drop the `alignas` or `aligned` attribute if it is unnecessary, or allocate
the object another way that guarantees its requested alignment. The GCC
flag requires C++17 aligned-new support. Do not disable it with
`-fno-aligned-new` or replace the project's C++ flags without preserving it.

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
debug on pushes and pull requests targeting `cpp`, or on manual dispatch.
It shares `.github/actions/cpp-build` with the upstream sync workflow. The
action installs the toolchain, builds native tools, tests the runtime and
builds both libraries and both sample configurations.

## Keeping the fork up to date

### Manual sync

A rebase replays your fork's commits on the new upstream version, like
reapplying a series of patches. Start with a clean working tree and Git
2.38 or newer for `--update-refs`. Add the upstream remote once:

```sh
git remote add upstream https://github.com/Stephane-D/SGDK.git
```

Then sync:

```sh
git fetch origin
git fetch upstream master
git switch master
git merge --ff-only upstream/master
git switch cpp
git rebase --update-refs upstream/master
```

`--update-refs` also moves the local stacked branches `rename-memory-header`,
`cpp-extern-c` and `cpp-build-support` to their replayed commits. It does not
move branches checked out in another worktree. When upstream merges a
branch's changes, Git normally detects equivalent patches and drops those
commits from the rebase automatically.

If there is a conflict, Git pauses. Edit the marked files, `git add` the
resolved files, then run `git rebase --continue`. Repeat as needed. Use
`git rebase --abort` to return to the state before the rebase if unsure.
Do not push until the rebase and all checks succeed.

Build the libraries and sample in release and debug using the commands
above, and run the runtime tests. Then publish:

```sh
git push origin master
git push --force-with-lease origin rename-memory-header cpp-extern-c cpp-build-support cpp
```

Master must fast-forward. The other branches have rewritten commit IDs, so
they need `--force-with-lease`. Unlike a plain force push, the lease refuses
to overwrite remote work you have not fetched. Do not fetch again between
starting the sync and pushing without checking for someone else's changes.

### Automatic sync

`.github/workflows/cpp-sync.yml` runs Mondays at 06:00 UTC, or manually from
the Actions tab. It checks out `cpp` and fetches upstream master. If `cpp`
already contains that version, it stops with a note in the run summary.
Otherwise it rebases in the CI workspace, keeping xlm as committer, and
runs the shared build action.

After successful checks, it fast-forwards the fork's `master` and updates
`cpp` with a lease tied to the exact SHA checked out before the rebase.
It does not update the remote stacked branches. If `SYNC_TOKEN` is absent,
it still rebases and tests, but skips both pushes and explains this in the
summary. A failed rebase, build, test or push opens an `Upstream sync failed`
issue with the run link, or comments on the existing open issue.

### One-time GitHub setup

1. Make `cpp` the fork's default branch in repository settings. GitHub only
   runs scheduled workflows from the default branch.
2. Create a fine-grained personal access token limited to **xlm/SGDK only**.
   Give it **Contents: read and write** and **Workflows: read and write**.
3. Save it as the repository Actions secret **`SYNC_TOKEN`**. Set an expiry
   and renew the secret before it expires.
4. Turn on **Issues** in repository settings. Forks start with Issues off,
   and the sync workflow reports failures as an issue.
5. In the Actions tab, disable **publish-doxygen**. It comes from upstream,
   publishes upstream's docs site, and would start running daily once `cpp`
   is the default branch.

The workflow itself has only contents-read and issues-write permissions.
Its normal `GITHUB_TOKEN` can report failures, but may not push commits that
touch `.github/workflows`. The PAT supplies that permission and allows
push-triggered builds after a successful sync. Never put the PAT in a file
or commit.
