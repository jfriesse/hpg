#!/bin/sh

extract() {
    sed 's/^.*tag: \(v[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*$/\1/'
}

extract_gen() {
    sed "s/^.*tag: \(v[0-9)][^,)]*\).*\$/\1/"
}

echo "ref names: (HEAD -> master, tag: v1.12.12, tag: v1.1.0)" | extract
echo "ref names: (HEAD -> master, tag: v1.12.12, tag: v1.1.0)" | extract_gen
echo "ref names: (tag: v1.12.12, tag: v1.1.0)" | extract
echo "ref names: (tag: v1.12.12, tag: v1.1.0)" | extract_gen
echo "ref names: (tag: v1.12.12)" | extract
echo "ref names: (tag: v1.12.12)" | extract_gen
echo "ref names: (tag: v1.1.1)" | extract
echo "ref names: (tag: v1.1.1)" | extract_gen
echo "ref names: (tag: v1.0)" | extract
echo "ref names: (tag: v1.0)" | extract_gen
