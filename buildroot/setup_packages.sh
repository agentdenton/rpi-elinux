#!/bin/sh

BUILDROOT_PACKAGES="$HOME/buildroot/package"
PACKAGES=$(pwd)

for d in packages/* ; do
    ln -s "$PWD/$d" $BUILDROOT_PACKAGES
done
