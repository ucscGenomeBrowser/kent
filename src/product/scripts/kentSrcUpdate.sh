#!/bin/bash
#
#	$Id: kentSrcUpdate.sh,v 1.1 2010/03/18 18:40:12 hiram Exp $
#
usage() {
    echo "usage: kentSrcUpdate.sh <browserEnvironment.txt>" 1>&2
    echo "  the browserEnvironment.txt file contains definitions of how" 1>&2
    echo "  these scripts behave in your local environment." 1>&2
    echo "There should be an example template to start with in the" 1>&2
    echo "directory with these scripts." 1>&2
    exit 255
}

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    echo "ERROR: kentSrcUpdate.sh: can not find ${includeFile}" 1>&2
    usage
fi

export errCount=0
if [ ! -d "${BROWSERHOME}" ]; then
    echo "ERROR: BROWSERHOME directory does not exist: ${BROWSERHOME}" 1>&2
    errCount=`echo $errCount | awk '{print $1+1}'`
fi
if [ ! -d "${CGI_BIN}" ]; then
    echo "ERROR: CGI_BIN directory does not exist: ${CGI_BIN}" 1>&2
    errCount=`echo $errCount | awk '{print $1+1}'`
fi

if [ $errCount -gt 0 ]; then
    echo "ERROR: check on the existence of the mentioned directories" 1>&2
    exit 255
fi

# this umask will allow all group members to write on files of other
# group members
umask 002

mkdir -p "${kentSrc}"

#	clean the source tree before updating it, this will lead
#	to a cleaner report from git pull
if [ -d "${kentSrc}/src" ]; then
    cd "${kentSrc}/src"
    make clean > /dev/null 2> /dev/null
    rm -f tags daily.log utils.log blatSuite.log
else
    #	first time, needs to establish the source tree
    cd "${kentSrc}"
    cd ..
    rmdir kent
    time git clone git://genome-source.soe.ucsc.edu/kent.git
    cd kent
    git checkout -t -b beta origin/beta
    cd ..
fi

# not necessary, but this is where it is happening
cd "${kentSrc}"

testHere=`pwd`
if [ "X${testHere}Y" != "X${kentSrc}Y" ]; then
    echo "ERROR: kentSrcUpdate.sh failed to chdir to ${kentSrc}" 1>&2
    exit 255;
fi
echo "git update report summary is in email to ${LOGNAME}" 1>&2
/usr/bin/git pull 2>&1 | mail -s 'GIT update report kent' "${LOGNAME}"

#	And then, let's build it all
cd "${kentSrc}/src"
testHere=`pwd`
if [ "X${testHere}Y" != "X${kentSrc}/srcY" ]; then
    echo "ERROR: kentSrcUpdate.sh failed to chdir to ${kentSrc}/src" 1>&2
    exit 255;
fi
MAKE="make -j 4" make -j 4 cgi-alpha > daily.log 2>&1
#	If there are any errors, they will come via separate email from cron
egrep -y "erro|warn" daily.log | grep -v "\-Werror" | \
	egrep -v "disabling jobserver mode|-o gbWarn.o |-o hgTracks |gbExtFile.o gbWarn.o gbMiscDiff.o"
make tags-all > /dev/null 2> /dev/null
# assuming $HOME/bin/$MACHTYPE is a symlink to /genome/browser/bin/$MACHTYPE
#	then all the utilities will end up there
make utils > utils.log 2>&1
make blatSuite > blatSuite.log 2>&1
# do not want to check for errors in these utils.log blat
