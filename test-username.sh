#!/bin/bash

# Configure this to what you need
testdir=/var/tmp/duc-username


#----------------------------------------
# Shouldn't need to edit under here.  Hah!
#----------------------------------------

if [[ $EUID -ne 0 ]]; then
    echo "Please run this test as root"
    exit
fi

#----------------------------------------
# Find two free, unallocated UIDs for testing...
user1=11111
getent passwd $user1 > /dev/null 2>&1
while [ $? -ne 2 ] ; do
    user1=$(( $user1 + 1 ))
    getent passwd $user1 > /dev/null 2>&1
done

user2=$(( $user1 + 1 ))
getent passwd $user1 > /dev/null 2>&1
while [ $? -ne 2 ] ; do
    user2=$(( $user2 + 1 ))
    getent passwd $user2 > /dev/null 2>&1
done

echo "Test UIDS:  user1: $user1, user2: $user2"

#----------------------------------------
# Check for valgrind, use if found
valgrind=""
valopts=""
if hash valgrind 2>/dev/null; then
    valgrind="valgrind"
    valopts="--quiet \
	--suppressions=valgrind-suppressions \
	--leak-check=full \
	--leak-check=full \
	--show-leak-kinds=all \
	--num-callers=30 \
	--error-exitcode=1"
fi

echo "   valgrind:  $valgrind $valopts"
echo ""

mkfile()
{
	mkdir -p "`dirname \"$1\"`"
	dd if=/dev/zero of="$1" bs=1 count=$2 2> /dev/null
}

export DUC_DATABASE=/tmp/test-username.db
rm -rf $testdir $DUC_DATABASE
mkdir -p $testdir

# Regular files

mkfile ${testdir}/tree/one 100
mkfile ${testdir}/tree/two 100
mkfile ${testdir}/tree/three 100
mkfile ${testdir}/tree/four 100
mkfile ${testdir}/tree/sub1/alpha 100
mkfile ${testdir}/tree/sub1/bravo 1000
mkfile ${testdir}/tree/sub1/charlie 1000
mkfile ${testdir}/tree/sub1/delta 10000
mkfile ${testdir}/tree/sub2/echo 100
mkfile ${testdir}/tree/sub2/foxtrot 1000
mkfile ${testdir}/tree/sub2/golf 1000
mkfile ${testdir}/tree/sub2/hotel 10000
mkfile ${testdir}/tree/sub3/india 5000
mkfile ${testdir}/tree/sub3/juliet 4000
mkfile ${testdir}/tree/sub4/kilo 1000
mkfile ${testdir}/tree/sub4/lima 2000
mkfile ${testdir}/tree/sub4/mike 3000
mkfile ${testdir}/tree/sub4/november 5000


# testing for searching by user...
chown -R $user1 ${testdir}/tree/sub[123]
chown -R $user2 ${testdir}/tree/sub4
chown $user2 ${testdir}/tree/sub1/bravo
chown $user2 ${testdir}/tree/sub1/bravo

# Hard link

mkdir ${testdir}/hard-link
mkfile ${testdir}/hard-link/one 10000
ln ${testdir}/hard-link/one ${testdir}/hard-link/two
ln ${testdir}/hard-link/one ${testdir}/hard-link/three

# Sparse file

mkdir ${testdir}/sparse
dd if=/dev/zero of=${testdir}/sparse/first bs=1 count=1 seek=32K 2> /dev/null

# Potentional problematic characters

mkfile "${testdir}/strange/cgi-space-%20-dir/file" 100
mkfile "${testdir}/strange/newline--dir/file" 100
mkfile "${testdir}/strange/tab-	-dir/file" 100
mkfile "${testdir}/strange/space- -dir/file" 100
mkfile "${testdir}/strange/carriage-return-
-dir/file" 100
mkfile "${testdir}/strange/question-mark-?-dir/file" 100
mkfile "${testdir}/strange/control-a--dir/file" 100
mkfile "${testdir}/strange/escape--dir/file" 100
mkfile "${testdir}/strange/less-then-<-dir/file" 100
mkfile "${testdir}/strange/more-then->-dir/file" 100
mkfile "${testdir}/strange/ampersand-&-dir/file" 100
mkfile "${testdir}/strange/single-quote-'-dir/file" 100
mkfile "${testdir}/strange/double-quote-\"-dir/file" 100
mkfile "${testdir}/strange/backslash-\\-dir/file" 100

# UTF-8 characters

mkfile ${testdir}/utf-8/оживлённым/foo 100
mkfile ${testdir}/utf-8/有朋自遠方來/foo 100
mkfile ${testdir}/utf-8/♜♞♝♛♚♝♞♜/foo 100

# Create index

$valgrind $valopts \
	./duc index --check-hard-links --bytes --verbose $testdir > test.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat test.out
	exit 1
else
    echo "Basic test done."
fi

cat test.out | grep -q "Indexed 37 files and 27 directories, (199661B apparent, 294912B actual)"

if [ "$?" = "0" ]; then
	echo "report ok"
else
	echo "report failed"
	cat test.out
	#exit 1
fi

$valgrind $valopts ./duc ls -aR $testdir > test.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat test.out
	#exit 1
fi

md5sum test.out | grep -q decda4c4f77b20a45aee5e90f7587603

if [ "$?" = "0" ]; then
	echo "md5sum ok"
else
	echo "md5sum failed"
	cat test.out 
	#exit 1
fi

# Test indexing by UID of user1.

$valgrind $valopts \
	./duc index --check-hard-links --bytes -U $user1 --verbose $testdir > test.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat test.out
	#exit 1
fi

cat test.out | grep -q "Indexed 37 files and 27 directories, (199661B apparent, 294912B actual)"

if [ "$?" = "0" ]; then
	echo "report ok"
else
	echo "report failed"
	cat test.out
	#exit 1
fi
