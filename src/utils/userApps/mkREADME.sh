#!/bin/sh
# script to create a FOOTER (or README) file from all usage messages
#	of binaries found in specified directory
#
#========================================================================
#	$Id: mkREADME.sh,v 1.3 2009/07/31 21:14:03 ann Exp $
#========================================================================
#
usage() {
    echo "usage: mkREADME.sh DESTDIR/BINDIR FOOTER"
    echo "where DESTDIR/BINDIR is where all the binaries are"
    echo "and FOOTER is the name of the result file"
    exit 255
}

if [ "$#" -ne 2 ]; then
    usage
fi

DESTDIR=$1
RESULT=$2

if [ ! -d "${DESTDIR}" ]; then
    echo "ERROR: specified destination directory \"${DESTDIR}\""
    echo "	does not exist."
    usage
fi

cd "${DESTDIR}"

echo "================================================================
to download all of the files from one of these admin/exe/ directories,
  for example: admin/exe/linux.x86_64/
    using the rsync command to your current directory:

  rsync -aP rsync://hgdownload.soe.ucsc.edu/genome/admin/exe/linux.x86_64/ ./

================================================================" > "${RESULT}"

# a few of the commands do not understand just a -verbose=2 argument
# some of those could be fixed TBD

find . -mindepth 1 -type f | grep -v -w hgsql | sed -e "s/^.\///; /mkREADME.sh/d" | sort | \
while read F
do
    if [ -x "${F}" ]; then
	echo "========   ${F}   ===================================="
	echo "================================================================"
        case "${F}" in
           ameme|aveCols|catUncomment|fetchChromSizes|gmtime|localtime|hgsqldump|pslPairs|qaToQac|bedJoinTabOffset|expMatrixToBarchartBed|webSync|vai.pl)
	     ./${F}
             ;;
           *)
	     ./${F} -verbose=2
        esac
	echo "================================================================"
	fi
done 2>&1 | grep -v "^ERROR:" >> "${RESULT}"

