#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
binary="${1:?Pass an existing build directory}/cpp-runtime-test"
"${HOST_CXX:-c++}" -std=c++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics \
    -I"$root/tests/cpp/include" "$root/tests/cpp/runtime.cpp" -o "$binary"
"$binary"
for mode in oom pure; do
    status=0
    "$binary" "$mode" || status=$?
    test "$status" -eq 77
done
echo 'PASS: ordinary allocation failure and pure virtual calls stop via SYS_die'
