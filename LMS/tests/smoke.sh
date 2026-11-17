#!/bin/sh
set -eu
APP=$(realpath "${1:-build/library_app}")
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"
printf 'admin\nadminpassword123\nadmin\nadminpassword123\n3\n\n3\n101\nC++ Book\nAuthor\nProgramming\n2\n1\n0\nEXIT\n' | "$APP" > output.txt
if ! grep -q 'Error: Book ID is not entered.' output.txt; then
    echo 'FAIL: missing ID error was not specific';exit 1
fi
if ! grep -q '101 | C++ Book' output.txt; then
    echo 'FAIL: book was not added';exit 1
fi
if ! test -f data/library.records; then
    echo 'FAIL: persistence file missing';exit 1
fi
echo 'PASS: CLI missing-ID error, book addition, listing and persistence'
