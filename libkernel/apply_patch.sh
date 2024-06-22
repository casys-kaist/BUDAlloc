#!/bin/bash

LIBMALLOC_DIR=$(realpath ./libmalloc/mimalloc/)
PATCH_FILE=$(realpath ./libmalloc/patches/mimalloc.patch)

cd $LIBMALLOC_DIR

git reset --hard
git apply $PATCH_FILE
