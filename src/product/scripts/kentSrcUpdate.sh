#!/bin/sh
#
#	$Id: kentSrcUpdate.sh,v 1.1 2010/03/18 18:40:12 hiram Exp $
#
usage() {
    echo "usage: kentSrcUpdate.sh <browserEnvironment.txt>"
    echo "  the browserEnvironment.txt file contains definitions of how"
    echo "  these scripts behave in your local environment."
    echo "There should be an example template to start with in the"
    echo "directory with these scripts."
    exit 255
}

export includeFile=$1
if [ "X${includeFile}Y" = "XY" ]; then
    usage
fi

if [ -f "${includeFile}" ]; then
    . "${includeFile}"
else
    echo "ERROR: kentSrcUpdate.sh: can not find ${includeFile}"
    usage
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
    time git clone git://genome-source.cse.ucsc.edu/kent.git
    cd kent
    git checkout -t -b beta origin/beta
    cd ..
fi

# not necessary, but this is where it is happening
cd "${kentSrc}"

testHere=`pwd`
if [ "X${testHere}Y" != "X${kentSrc}Y" ]; then
    echo "ERROR: kentSrcUpdate.sh failed to chdir to ${kentSrc}"
    exit 255;
fi
/usr/bin/git pull 2>&1 | mail -s 'GIT update report kent' "${LOGNAME}"

#	And then, let's build it all
cd "${kentSrc}/src"
testHere=`pwd`
if [ "X${testHere}Y" != "X${kentSrc}/srcY" ]; then
    echo "ERROR: kentSrcUpdate.sh failed to chdir to ${kentSrc}/src"
    exit 255;
fi
MAKE="make -j 4" make -j 4 cgi-alpha > daily.log 2>&1
#	If there are any errors, they will come via separate email from cron
egrep -y "erro|warn" daily.log | grep -v "\-Werror" | \
	egrep -v "disabling jobserver mode|-o hgTracks |gbExtFile.o gbWarn.o gbMiscDiff.o"
make tags-all > /dev/null 2> /dev/null
# assuming $HOME/bin/$MACHTYPE is a symlink to /genome/browser/bin/$MACHTYPE
#	then all the utilities will end up there
make utils > utils.log 2>&1
make blatSuite > blatSuite.log 2>&1
# do not want to check for errors in these utils.log blat
