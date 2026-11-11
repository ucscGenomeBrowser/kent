#!/bin/tcsh
# Nightly build of the kent utilities into /cluster/bin/$MACHTYPE, so the
# programs on hgwdev follow the tip of the tree the way the CGIs do.
# Run from the build user's crontab, after the nightly CGI build is done:
#
#   30 7 * * * flock -n $HOME/nightlyUtils.lock /hive/groups/browser/newBuild/kent/src/utils/qa/weeklybld/nightlyUtils.csh
#
# It builds the commit that last passed the nightly build and make test.
# runBuildWithUseCases.sh writes that commit to $BUILDSTATE/lastCleanNightly,
# so a night that fails installs nothing new.  It builds in its own checkout
# and passes BINDIR to make, so it leaves alone the nightly CGI tree and
# ~build/bin.
#
# To turn it off without editing the crontab:  touch $HOME/kent/noNightlyUtils
#
# It prints nothing.  On a failure it mails the buildmeister.  refs #38676

if ( -e $HOME/kent/noNightlyUtils ) exit 0

set lastClean = /hive/groups/browser/newBuild/state/lastCleanNightly
if ( ! -s $lastClean ) exit 0
set commit = `cat $lastClean`

set base = $BUILDDIR/nightlyUtils
set installed = $base/installed
set log = $base/nightlyUtils.log
# NIGHTLY_UTILS_DEST moves the install for a test run; normally /cluster/bin
set dest = /cluster/bin
if ( $?NIGHTLY_UTILS_DEST ) then
    set dest = $NIGHTLY_UTILS_DEST
endif
set binDir = $dest/$MACHTYPE
set scriptDir = $dest/scripts
set buildmeister = braney@ucsc.edu
if ( $?BUILDMEISTEREMAIL ) then
    set buildmeister = $BUILDMEISTEREMAIL
endif

# nothing new passed since the last install
if ( -s $installed ) then
    if ( "`cat $installed`" == "$commit" ) exit 0
endif

mkdir -p $base
echo "nightlyUtils: building $commit into $binDir [`date`]" >& $log

if ( ! -d $base/kent ) then
    git clone -q /data/git/kent.git $base/kent >>& $log
    if ( $status ) goto failed
endif
cd $base/kent
git fetch -q origin >>& $log
if ( $status ) goto failed
# lastCleanNightly is HEAD of the build tree, which may hold local commits
# (unpushed work, the merge from the 03:07 git pull) not yet in /data/git
git fetch -q $HOME/kent HEAD >>& $log
if ( $status ) goto failed
git checkout -q --detach $commit >>& $log
if ( $status ) goto failed
# the submodule URLs are relative, so they resolve to /data/git as well
git -c protocol.file.allow=always submodule update --init -q >>& $log
if ( $status ) goto failed

cd $base/kent/src
make clean >& /dev/null
echo "make utils [`date`]" >>& $log
make -j 32 BINDIR=$binDir SCRIPTS=$scriptDir utils >>& $log
if ( $status ) goto failed
# parasol installs with rsync into ${DESTDIR}/${BINDIR}, not into BINDIR alone
echo "make parasol userApps [`date`]" >>& $log
make -C parasol -j 32 DESTDIR=$dest BINDIR=$MACHTYPE userApps >>& $log
if ( $status ) goto failed
echo "make blatSuite [`date`]" >>& $log
make BINDIR=$binDir SCRIPTS=$scriptDir blatSuite >>& $log
if ( $status ) goto failed

echo $commit > $installed
echo "nightlyUtils: done [`date`]" >>& $log
exit 0

failed:
    tail -60 $log | mail -s "nightlyUtils: utils build FAILED on $HOST at $commit" $buildmeister
    exit 1
