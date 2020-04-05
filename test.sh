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


cat ${DUC_TEST_DIR}.out | grep -q "Indexed 77 files and 47 directories, (91869B apparent, 540672B actual)"

if [ "$?" = "0" ]; then
	echo "report ok"
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

testsum="78dbf880ef6917ea665fddb5ebb44428"
md5sum ${DUC_TEST_DIR}.out > /tmp/.duc.md5sum
grep -q $testsum /tmp/.duc.md5sum

if [ "$?" = "0" ]; then
	echo "md5sum ok"
else
	echo "md5sum failed"
	echo -n "expected $testsum, got: "
	cat /tmp/.duc.md5sum
	exit 1
fi

$valgrind ./duc info -d test/dbs/

# end

