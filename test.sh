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

# Special characters

mkdir test/special-chars
mkfile test/special-chars/оживлённым/foo 100
mkfile test/special-chars/有朋自遠方來/foo 100
mkfile test/special-chars/♜♞♝♛♚♝♞♜/foo 100

# Create index

./duc index --debug --check-hard-links --bytes --verbose test > test.out 2>&1

cat test.out | grep -q "Indexed 20 files and 15 directories, (119509B apparent, 159744B actual)"

if [ "$?" = "0" ]; then
	echo "report ok"
else
	echo "report failed"
	cat test.out
	exit 1
fi

duc ls -aR test > test.out 2>&1
md5sum test.out | grep -q 23c9bd7e1dd1b4b966d656e8a78937cf

if [ "$?" = "0" ]; then
	echo "md5sum ok"
else
	echo "md5sum failed"
	cat test.out 
	exit 1
fi

# end

