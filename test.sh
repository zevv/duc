#!/bin/sh

export DUC_DATABASE=test.db
rm -rf test test.db
mkdir test

# file types

mkdir test/file_types
touch test/file_types/regular
mkfifo test/file_types/fifo
mknod test/file_types/chrdev c 1 1
mknod test/file_types/blkdev b 1 1
mkdir test/file_types/dir
ln -s /etc/services test/file_types/symlink

# sparse file

mkdir test/sparse
dd if=/dev/zero of=test/sparse/file bs=1 count=1 seek=32K

# hard links

mkdir test/hardlink
dd if=/dev/zero of=test/hardlink/001 bs=1k count=1
for i in `seq 2 32`; do ln test/hardlink/001 test/hardlink/$i; done

duc/duc index test

duc/duc ls -FR test
duc/duc ls -F  test/sparse
duc/duc ls -Fa test/sparse
duc/duc gui test 


