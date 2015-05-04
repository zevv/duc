#!/bin/sh

mkfile()
{
	mkdir -p "`dirname \"$1\"`"
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

# Potentional problematic characters

mkfile "test/strange/cgi-space-%20-dir/file" 100
mkfile "test/strange/newline--dir/file" 100
mkfile "test/strange/tab-	-dir/file" 100
mkfile "test/strange/space- -dir/file" 100
mkfile "test/strange/carriage-return-
-dir/file" 100
mkfile "test/strange/question-mark-?-dir/file" 100
mkfile "test/strange/control-a--dir/file" 100
mkfile "test/strange/escape--dir/file" 100
mkfile "test/strange/less-then-<-dir/file" 100
mkfile "test/strange/more-then->-dir/file" 100
mkfile "test/strange/ampersand-&-dir/file" 100
mkfile "test/strange/single-quote-'-dir/file" 100
mkfile "test/strange/double-quote-\"-dir/file" 100
mkfile "test/strange/backslash-\\-dir/file" 100

# UTF-8 characters

mkfile test/utf-8/оживлённым/foo 100
mkfile test/utf-8/有朋自遠方來/foo 100
mkfile test/utf-8/♜♞♝♛♚♝♞♜/foo 100

# Create index

valgrind \
	--quiet \
	--suppressions=valgrind-suppressions \
	--leak-check=full \
	--leak-check=full \
	--show-leak-kinds=all \
	--num-callers=30 \
	--error-exitcode=1 \
	./duc index --debug --check-hard-links --bytes --verbose test > test.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat test.out
	exit 1
fi

cat test.out | grep -q "Indexed 30 files and 25 directories, (161469B apparent, 241664B actual)"

if [ "$?" = "0" ]; then
	echo "report ok"
else
	echo "report failed"
	cat test.out
	exit 1
fi

valgrind \
	--quiet \
	--suppressions=valgrind-suppressions \
	--leak-check=full \
	--leak-check=full \
	--show-leak-kinds=all \
	--num-callers=30 \
	--error-exitcode=1 \
	duc ls -aR test > test.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat test.out
	exit 1
fi

md5sum test.out | grep -q 240bb4b92ce47d5df0e0518afa06da47

if [ "$?" = "0" ]; then
	echo "md5sum ok"
else
	echo "md5sum failed"
	cat test.out 
	exit 1
fi

# end

