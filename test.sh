#!/bin/sh

export DUC_DATABASE=/tmp/duc-test.db
DUC_TEST_DIR=/tmp/duc-test

# Set to 0 to allow valgrind use
DEF_USE_VALGRIND=0

# Run with USE_VALGRIND=1 ./test.sh
DO_VALGRIND=${USE_VALGRIND:=$DEF_USE_VALGRIND}

valargs="--quiet \
  --suppressions=valgrind-suppressions \
  --leak-check=full \
  --leak-check=full \
  --show-leak-kinds=all \
  --num-callers=30 \
  --error-exitcode=1 \
"
# Check for valgrind only if asked to use it.
valgrind=""
if [ "${DO_VALGRIND}" = "1" ]; then
    hash valgrind 2>/dev/null

    if [ $? -eq 1 ]; then
	echo "Error!  Valgrind not installed, can't run with \$DO_VALGRIND set."
	exit 1
    else
	echo "Using valgrind with : $valargs"
	echo ""
	valgrind="valgrind $valargs"
    fi
fi

mkfile()
{
	mkdir -p "`dirname \"$1\"`"
	dd if=/dev/zero of="$1" bs=1 count=$2 2> /dev/null
}

rm -rf $DUC_DATABASE $DUC_TEST_DIR
mkdir $DUC_TEST_DIR

# Regular files

mkfile ${DUC_TEST_DIR}/tree/one 100
mkfile ${DUC_TEST_DIR}/tree/two 100
mkfile ${DUC_TEST_DIR}/tree/three 100
mkfile ${DUC_TEST_DIR}/tree/four 100
mkfile ${DUC_TEST_DIR}/tree/sub1/alpha 100
mkfile ${DUC_TEST_DIR}/tree/sub1/bravo 1000
mkfile ${DUC_TEST_DIR}/tree/sub1/charlie 1000
mkfile ${DUC_TEST_DIR}/tree/sub1/delta 10000
mkfile ${DUC_TEST_DIR}/tree/sub2/echo 100
mkfile ${DUC_TEST_DIR}/tree/sub2/foxtrot 1000
mkfile ${DUC_TEST_DIR}/tree/sub2/golf 1000
mkfile ${DUC_TEST_DIR}/tree/sub2/hotel 10000
mkfile ${DUC_TEST_DIR}/tree/sub3/india 5000
mkfile ${DUC_TEST_DIR}/tree/sub3/juliet 4000
mkfile ${DUC_TEST_DIR}/tree/sub4/kilo 1000
mkfile ${DUC_TEST_DIR}/tree/sub4/lima 2000
mkfile ${DUC_TEST_DIR}/tree/sub4/mike 3000
mkfile ${DUC_TEST_DIR}/tree/sub4/november 5000

# Hard link

mkdir ${DUC_TEST_DIR}/hard-link
mkfile ${DUC_TEST_DIR}/hard-link/one 10000
ln ${DUC_TEST_DIR}/hard-link/one ${DUC_TEST_DIR}/hard-link/two
ln ${DUC_TEST_DIR}/hard-link/one ${DUC_TEST_DIR}/hard-link/three

# Sparse file

mkdir ${DUC_TEST_DIR}/sparse
dd if=/dev/zero of=${DUC_TEST_DIR}/sparse/first bs=1 count=1 seek=32K 2> /dev/null

# Potentional problematic characters

mkfile "${DUC_TEST_DIR}/strange/cgi-space-%20-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/newline--dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/tab-	-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/space- -dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/carriage-return-
-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/question-mark-?-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/control-a--dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/escape--dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/less-then-<-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/more-then->-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/ampersand-&-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/single-quote-'-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/double-quote-\"-dir/file" 100
mkfile "${DUC_TEST_DIR}/strange/backslash-\\-dir/file" 100

# UTF-8 characters

mkfile ${DUC_TEST_DIR}/utf-8/оживлённым/foo 100
mkfile ${DUC_TEST_DIR}/utf-8/有朋自遠方來/foo 100
mkfile ${DUC_TEST_DIR}/utf-8/♜♞♝♛♚♝♞♜/foo 100

# Deep and long named directories
x=1
long="some_stupidly_long_directory_name_I_just_made_up"
current=`pwd`
cd $DUC_TEST_DIR
while [ $x -le 20 ]; do
    #echo mkdir ${long}${x}
    mkdir ${long}${x}
    #echo cd ${long}${x}
    cd ${long}${x}
    dd if=/dev/zero of=longone bs=1 count=40 2> /dev/null 
    dd if=/dev/zero of=longtwo bs=1 count=100 2> /dev/null
    x=`expr $x + 1`
done
cd $current
  

# Create index
    
	$valgrind ./duc index --debug --check-hard-links --bytes --verbose ${DUC_TEST_DIR} > ${DUC_TEST_DIR}.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat ${DUC_TEST_DIR}.out
	exit 1
fi


#---------------------------------------------------------------------------------
# Actual tests are below.  If you add test cases above, these need to be tweaked.
#---------------------------------------------------------------------------------

# An exact match is expected on the apparent size; the actual size may vary.
cat ${DUC_TEST_DIR}.out | grep -q "Indexed 77 files and 47 directories, (91869B apparent, [0-9]*B actual)"

if [ "$?" = "0" ]; then
	echo "report: ok"
else
        echo "-----------------------------"
	echo "report failed"
        echo "-----------------------------"
        echo ""
	cat ${DUC_TEST_DIR}.out
        echo ""
	exit 1
fi

$valgrind ./duc ls -aR ${DUC_TEST_DIR} > ${DUC_TEST_DIR}.out 2>&1

if [ "$?" != "0" ]; then
	echo "valgrind error"
	cat ${DUC_TEST_DIR}.out
	exit 1
fi

# When two or more hard links point to the same file and when running duc with
# the `--check-hard-links' option, only one of the hard links will be
# counted. However, duc may pick up and display a different hard link depending
# on the system it is being run on. Since our tests include three hard links to
# the same file, we should be expecting three possible outcomes, all equally
# valid, each corresponding to one of the following MD5 checksums.
testsum0="78dbf880ef6917ea665fddb5ebb44428"
testsum1="38ab7b7d1ec6ac57d672c5618371386d"
testsum2="33e2be27a9e70e81d4006a2d7b555948"
md5sum ${DUC_TEST_DIR}.out > /tmp/.duc.md5sum
grep -q "$testsum0\|$testsum1\|$testsum2" /tmp/.duc.md5sum

if [ "$?" = "0" ]; then
	echo "md5sum: ok"
else
	echo "md5sum: failed"
	echo "expected: "
	echo "$testsum  ${DUC_TEST_DIR}.out"
	echo "got: "
	cat /tmp/.duc.md5sum
	exit 1
fi


# Test backend checking.
ductype=`./duc --version | tail -1 | awk '{print $NF}'`
typemax=5
typecount=0
for type in leveldb lmdb tokyocabinet kyotocabinet sqlite; do
    if [ "$ductype" != "$type" ]; then
	#echo "./duc info -d test/dbs/${type}.db"
	./duc info -d test/dbs/${type}.db > ${DUC_TEST_DIR}.out 2>&1 
	if [ "$?" = "1" ]; then
	    typecount=`expr $typecount + 1`
	fi
    else
	echo "skipping DB type $ductype, we are compiled with that type." > ${DUC_TEST_DIR}.out 2>&1 
    fi
done

if [ "$typecount" = "4" ]; then
    echo "DB Types: ok"
else
    echo "Type checks: failed - got $typecount failures instead of expected 4 out of $typemax."
fi

# end

