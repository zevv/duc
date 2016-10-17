#!/usr/bin/env bash

# http://stackoverflow.com/questions/13400312/linux-create-random-directory-file-hierarchy
# Decimal ASCII codes (see man ascii); added space

AARR=( {48..57} {65..90} {97..122} )
# Array count
aarrcount=${#AARR[@]}

if [ "$1" == "" ] ; then
    OUTDIR="./rndpath" ;
else
    OUTDIR="$1" ;
fi

ASCIIONLY=1 ;
DIRDEPTH=3 ;
MAXFIRSTLEVELDIRS=2 ;

MINDIRCHILDREN=2
MAXDIRCHILDREN=10 ;

MINDIRNAMELEN=4
MAXDIRNAMELEN=18 ;

MINFILECHILDREN=4
MAXFILECHILDREN=40 ;

MINFILENAMELEN=6
MAXFILENAMELEN=20 ;

MINFILESIZE=1024
MAXFILESIZE=2000000 ;

FILEEXT=".bin"
VERBOSE=0 #1

get_rand_dirname() {
    if [ "$ASCIIONLY" == "1" ]; then
	for ((i=0; i<$((MINDIRNAMELEN+RANDOM%MAXDIRNAMELEN)); i++)) {
		printf \\$(printf '%03o' ${AARR[RANDOM%aarrcount]});
	    }
	    else
		cat /dev/urandom | tr -dc '[ -~]' | tr -d '[$></~:`\\]' | head -c$((MINDIRNAMELEN + RANDOM % MAXDIRNAMELEN)) | sed 's/\(["]\)/\\\1/g'
	fi
	    #echo -e " " # debug last dirname space
}

get_rand_filename() {
    if [ "$ASCIIONLY" == "1" ]; then
	for ((i=0; i<$((MINFILENAMELEN+RANDOM%MAXFILENAMELEN)); i++)) {
		printf \\$(printf '%03o' ${AARR[RANDOM%aarrcount]});
	    }
	    else
		# no need to escape double quotes for filename
		cat /dev/urandom | tr -dc '[ -~]' | tr -d '[$></~:`\\]' | head -c$((MINFILENAMELEN + RANDOM % MAXFILENAMELEN)) #| sed 's/\(["]\)/\\\1/g'
	fi
	    printf "%s" $FILEEXT
}


echo "Warning: will create random tree at: $OUTDIR"
[ "$VERBOSE" == "1" ] && echo "  MAXFIRSTLEVELDIRS $MAXFIRSTLEVELDIRS ASCIIONLY $ASCIIONLY DIRDEPTH $DIRDEPTH MAXDIRCHILDREN $MAXDIRCHILDREN MAXDIRNAMELEN $MAXDIRNAMELEN MAXFILECHILDREN $MAXFILECHILDREN MAXFILENAMELEN $MAXFILENAMELEN MAXFILESIZE $MAXFILESIZE"

#read -p "Proceed (y/n)? " READANS
#if [ "$READANS" != "y" ]; then
#    exit
#fi

if [ -d "$OUTDIR" ]; then
    echo "Removing old outdir $OUTDIR"
    rm -rf "$OUTDIR"
fi

echo mkdir "$OUTDIR"
mkdir "$OUTDIR"

if [ $MAXFIRSTLEVELDIRS -gt 0 ]; then
    NUMFIRSTLEVELDIRS=$((1+RANDOM%MAXFIRSTLEVELDIRS))
else
    NUMFIRSTLEVELDIRS=0
fi

echo "Creating directories... $NUMFIRSTLEVELDIRS"
# create directories
for (( ifl=0;ifl<$((NUMFIRSTLEVELDIRS));ifl++ )) {
	FLDIR="$(get_rand_dirname)"
	FLCHILDREN="";
	for (( ird=0;ird<$((DIRDEPTH-1));ird++ )) {
		DIRCHILDREN=""; MOREDC=0;
		for ((idc=0; idc<$((MINDIRCHILDREN+RANDOM%MAXDIRCHILDREN)); idc++)) {
			CDIR="$(get_rand_dirname)" ;
			# make sure comma is last, so brace expansion works even for 1 element? that can mess with expansion math, though
			if [ "$DIRCHILDREN" == "" ]; then DIRCHILDREN="\"$CDIR\"" ;
			else DIRCHILDREN="$DIRCHILDREN,\"$CDIR\"" ; MOREDC=1 ; fi
		    }
		    if [ "$MOREDC" == "1" ] ; then
			if [ "$FLCHILDREN" == "" ]; then FLCHILDREN="{$DIRCHILDREN}" ;



			else FLCHILDREN="$FLCHILDREN/{$DIRCHILDREN}" ; fi
		    else
			if [ "$FLCHILDREN" == "" ]; then FLCHILDREN="$DIRCHILDREN" ;
			else FLCHILDREN="$FLCHILDREN/$DIRCHILDREN" ; fi
		    fi
	    }
	    DIRCMD="mkdir -p $OUTDIR/\"$FLDIR\"/$FLCHILDREN"
	    echo "$DIRCMD"
	    eval "$DIRCMD"
    }
    
    # now loop through all directories, create random files inside
    # note printf '%q' escapes to preserve spaces; also here
    # escape, and don't wrap path parts in double quotes (e.g. | sed 's_/_"/"_g');
    # note then we STILL have to eval to use it!
    # but now ls "$D" works, so noneed for QD
    # unfortunately backslashes can make '%q' barf - prevent them

    find "$OUTDIR" -type d | while IFS= read D ; do
				 QD="$(printf '%q' "$(echo "$D")" )" ;
				 [ "$VERBOSE" == "1" ] && echo "$D"; #echo "$QD"; ls -la "$D"; #eval "ls -la $QD";
				 for ((ifc=0; ifc<$((MINFILECHILDREN+RANDOM%MAXFILECHILDREN)); ifc++)) {
					 CFILE="$(get_rand_filename)" ;
					 echo -n '> '
					 [ "$VERBOSE" == "1" ] && echo "$D"/"$CFILE"
					 cat /dev/urandom \
					     | head -c$((MINFILESIZE + RANDOM % MAXFILESIZE)) \
						    > "$D"/"$CFILE"
				     }
				 done
				     
				     echo
				     #tree -a --dirsfirst -s "$OUTDIR"
				     echo "total bytes: $(du -bs $(echo "$OUTDIR"))"
				     
				     
