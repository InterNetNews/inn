#!/bin/sh
#
# Author:       James A. Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Sat, 17 Feb 1996 22:05:07 +1100
# Project:      INN -- innfeed
# File:         makedepend.sh
# RCSId:        $Id$
# Description:  A replacement for 'gcc -MM'.
#

if [ "X$CPP" = X ]; then
	CPP="cc -E"
fi

MAKEFILE=Makefile
OBJSUFFIX=o
OBJPREFIX=''

# The field number (as awk numbers field) in a CPP-output line
# that has the file name. 
FILE_FIELD=3

IGNORE_BAD=false

USAGE="`basename $0` [ -c cpp-cmd -x field-num -s -a -f file -o obj-suffix -p obj-prefix -Ddefine -Iinclude -- ] files
 -c cpp-cmd	specifies the command to run the source files over
                to preprocess the files. Defaults to 'cc -E'.
 -x field-num   specifies the field number of the preprocessor tagged lines
                (lines with a leading '#') that has the file name. Field
                numbers start at 1. Default is 3.
 -s             Specifies to ignore system include files (actually any
                include file that CPP reports with an absolute pathname
 -a             specifies to append the info to the Makefile instread of
                replacing what is already there.
 -f file        Specifies the file to edit instead of Makefile.
 -o obj-suffix  Specifies the suffix to give to the object files instead of
                '.o'
 -p obj-prefix  Specifies the prefix to give to the object file names
                instead of the empty string (for adding directories to the
                name usually).
 --             Specifies that any unrecognised arguments after this are
                silently ignored.
 -Ddefine       A normal C or C++ compiler -D option.
 -Iinclude      A normal C or C++ compiler -I include file path option."

while [ $# != 0 ]; do
	case "$1" in
		-c*)	CPP=`expr "$1" : '-c\(.*\)'`
			if [ "X$CPP" = X ]; then
				CPP=$2
				shift
			fi ;;

		-D*)	DEFINES="$DEFINES $1" ;;

		-I*)	INCLUDES="$INCLUDES $1" ;;

		-a)	APPEND=1 ;;

		-f*)	MAKEFILE=`expr "$1" : '-f\(.*\)'` 
			if [ "X$MAKEFILE" = X ]; then 
				MAKEFILE=$2 
				shift 
			fi ;;

		-o*)	OBJSUFFIX=`expr "$1" : '-o\(.*\)'`
			if [ "X$OBJSUFFIX" = X ]; then
				OBJSUFFIX=$2
				shift
			fi ;;

		-p*)	OBJPREFIX=`expr "$1" : '-p\(.*\)'`
			if [ "X$OBJPREFIX" = X ]; then
				OBJPREFIX=$2
				shift
			fi ;;

		-w*)	;; # ignored

		-m*)	;; # ignored

		-x*)	FILE_FIELD=`expr "$1" : '-x\(.*\)'`
			if [ "X$FILE_FIELD" = X ]; then
				FILE_FIELD=$2
				shift 
			fi
			if [ `expr "$FILE_FIELD" : '^[0-9][0-9]*$'` = 0 ]; then
				echo 'Must give a field number (starting at 1) to "-x"'
				DIE=1
			elif [ 0 -ge $FILE_FIELD ]; then
				echo 'Must give a field number (starting at 1) to "-x"'
				DIE=1
			fi ;;

		-s)	NO_SYSTEM=1 ;;

		--)	if $IGNORE_BAD; then 
				IGNORE_BAD=false 
			else
				IGNORE_BAD=true 
			fi ;;

		-*)	if $IGNORE_BAD; then 
				true
			else 
				echo "Unknown option $1"
				DIE=1
			fi ;;

		*)	FILES="$FILES $1" ;;
	esac
	shift
done

if [ -n "$DIE" ]; then
	echo "$USAGE"
	exit 1
fi

ARGS="$DEFINES $INCLUDES"

WKFILE=/tmp/makedepend.$$
TAILFILE=/tmp/makedepend.tail.$$

if [ ! -f $MAKEFILE ]; then
	echo "No such file: $MAKEFILE"
	exit 1
fi

SYSTEM_FILES=cat
if [ -n "$NO_SYSTEM" ]; then
	SYSTEM_FILES="grep -v ^/"
fi

for file in $FILES; do
	OBJ=`expr "$file" : '\(.*\)\..*'`
	OBJ="${OBJPREFIX}$OBJ.$OBJSUFFIX"
	
	$CPP $ARGS $file 2> /dev/null |
		awk '/^#/ && NF >= '$FILE_FIELD'{print $'$FILE_FIELD'}' |
		sed -e 's/"//g' |
		sort |
		uniq |
		$SYSTEM_FILES |
		tr '\012' ' '  > $WKFILE 

	if [ -s $WKFILE ]; then 
		(echo -n "${OBJ}: " ; cat $WKFILE ; echo "") |
			fmt |
			sed	-e 's/$/ \\/' \
				-e '2,$s/^/   /' \
				-e '$s/ \\$//' >> $TAILFILE
	else 
		echo "Hmmmmm.... Not good. No dependencies for $file."
	fi

	rm -f $WKFILE
done

LINE="# DO NOT DELETE THIS LINE -- make depend depends on it."

if [ -s $TAILFILE ]; then
	mv $MAKEFILE $MAKEFILE.BAK || {
		echo "Can't move $MAKEFILE to $MAKEFILE.BAK"
		exit 
	}

	if [ "X$APPEND" = X ]; then
		sed -n -e '1,/^'"$LINE"'/p' $MAKEFILE.BAK > $MAKEFILE
		if grep -q '^'"$LINE" $MAKEFILE > /dev/null 2>&1; then 
			true
		else
			(echo "" ; echo "$LINE") >> $MAKEFILE
		fi
	else
		cat $MAKEFILE.BAK > $MAKEFILE
	fi

	( echo "" ; cat $TAILFILE ) >> $MAKEFILE
fi

rm -f $TAILFILE
