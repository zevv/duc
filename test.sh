#!/bin/sh

mkfile()
{
	mkdir -p `dirname $1`
	dd if=/dev/zero of="$1" bs=1 count=$2 2> /dev/null
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

# Strange names

mkdir test/strange
mkfile "test/strange/<h1>html/file" 100
mkfile "test/strange/no%20space/file" 100
mkfile "test/strange/embbeddednewline/file" 100
mkfile "test/strange/this?might=break&the=cgi/file" 100

# Create index

./duc index --debug --check-hard-links --bytes --verbose test > test.out 2>&1

grep -q "Indexed 17 files and 10 directories, (98729B apparent, 126976B actual) in 0.00 secs." test.out

if [ "$?" = "0" ]; then
	echo "report ok"
else
	echo "report failed"
	cat test.out
fi

duc ls -aR test > test.out 2>&1
md5sum test.out | grep -q 8d824ccf8f50f961a0f66c1c44c611ef

if [ "$?" = "0" ]; then
	echo "md5sum ok"
else
	echo "md5sum failed"
	cat test.out
fi

# end

