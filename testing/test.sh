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
mkfile test/tree/sub3/india 5000
mkfile test/tree/sub3/juliet 4000
mkfile test/tree/sub4/kilo 1000
mkfile test/tree/sub4/lima 2000
mkfile test/tree/sub4/mike 3000
mkfile test/tree/sub4/november 5000

# Hard link

mkdir test/hard-link
mkfile test/hard-link/one 10000
ln test/hard-link/one test/hard-link/two
ln test/hard-link/one test/hard-link/three

# Sparse file

mkdir test/sparse
dd if=/dev/zero of=test/sparse/first bs=1 count=1 seek=32K 2> /dev/null

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

cat test.out | grep -q "Indexed 37 files and 27 directories, (199661B apparent, 294912B actual)"

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

md5sum test.out | grep -q decda4c4f77b20a45aee5e90f7587603

if [ "$?" = "0" ]; then
	echo "md5sum ok"
else
	echo "md5sum failed"
	cat test.out 
	exit 1
fi

# end

