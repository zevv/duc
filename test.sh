#!/bin/sh

mkfile()
{
	mkdir -p `dirname $1`
	dd if=/dev/zero of=$1 bs=1 count=$2 2> /dev/null
}

export DUC_DATABASE=test.db
rm -rf test test.db
mkdir test

# Regular files

mkfile test/tree/one 100
mkfile test/tree/two 100
mkfile test/tree/three 100
mkfile test/tree/four 100
mkfile test/tree/sub1/alpha 100
mkfile test/tree/sub1/bravo 1000
mkfile test/tree/sub1/charlie 1000
mkfile test/tree/sub1/delta 10000
mkfile test/tree/sub2/echo 100
mkfile test/tree/sub2/foxtrot 1000
mkfile test/tree/sub2/golf 1000
mkfile test/tree/sub2/hotel 10000

# Hard link

mkdir test/tree/sub3
ln test/tree/sub2/hotel test/tree/sub3

# Sparse file

mkdir test/tree/sub4
dd if=/dev/zero of=test/tree/sub3/sparse bs=1 count=1 seek=32K 2> /dev/null

duc index --check-hard-links --bytes --verbose test > test.out 2>&1

grep -q "Indexed 13 files and 5 directories, (77849B apparent, 90112B actual)" test.out

if [ "$?" = "0" ]; then
	echo "ok"
else
	echo "failed:"
	cat test.out
fi

