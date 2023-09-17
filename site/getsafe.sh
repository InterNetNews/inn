#! /bin/sh
##
##  Safely get a file from the samples directory.  Usage:
##      getsafe <sample> <localfile>
case $# in
2) ;;
*)
    echo "Can't get INN sample file: wrong number of arguments." 1>&2
    exit 1
    ;;
esac

SRC=$1
DEST=$2

##  Try RCS.
if [ -f RCS/${DEST},v ]; then
    echo "Note: ${SRC} has changed; please compare."
    test -f ${DEST} && exit 0
    exec co -q ${DEST}
fi

##  Try SCCS.
if [ -f SCCS/s.${DEST} ]; then
    echo "Note: ${SRC} has changed; please compare."
    test -f ${DEST} && exit 0
    exec sccs get -s ${DEST}
fi

##  Does the file exist locally?
if [ -f ${DEST} ]; then
    cmp ${SRC} ${DEST}
    if [ $? -eq 0 ]; then
        touch ${DEST}
        exit 0
    fi
    diff -u ${SRC} ${DEST}
    echo "${SRC} has changed since the last build."
    echo "Please ensure the above changes wouldn't be worth adding"
    echo "to your ${DEST} production file."
    echo "  Once taken into account or if you don't mind the proposed changes,"
    echo "  run \"rm site/${DEST}\" and continue the build."
    echo "  Note that you can silent all these warnings by running"
    echo '  "cd site && make distclean".'
    exit 1
fi

echo Using sample version of ${DEST}
cp ${SRC} ${DEST}

exit 0
