#!/bin/tcsh
# Build utils into /cluster/bin/$MACHTYPE on hgwdev

cd $WEEKLYBLD

# for cron job
#source .cshrc
#setenv SHELL /bin/tcsh
#setenv PATH /bin:/usr/bin:/usr/local/bin:/cluster/bin/${MACHTYPE}:~/bin/${MACHTYPE}
#setenv HOSTNAME $HOST

if ( "$MACHTYPE" == "i386" ) then
    if ( "$HOST" != "hgwdev" ) then
	echo "error: you must run this script on dev!"
	exit 1
    endif
endif
if ( "$MACHTYPE" == "x86_64" ) then
    if (( "$HOST" != "kolossus" ) && ( "$HOST" != "kkr1u00" )) then
	echo "error: you must run this script on kolossus or kkr1u00!"
	exit 1
    endif
endif


if ( -d ~/bin/${MACHTYPE}.orig ) then
 echo "restoring from last failed symlink."
 ./unsymtrick.csh
endif
if ( ! -d ~/bin/${MACHTYPE}.cluster ) then
 echo "something messed up in symlink"
 exit 1
endif

# Symlink Trick safe now
echo "Symlink Trick."
./symtrick.csh

if ( "$MACHTYPE" == "x86_64" ) then
    echo
    echo "Doing make clean on src."
    cd $WEEKLYBLD/kent/src
    make clean>& make.clean.log
endif

echo
echo "Building src utils."
cd $WEEKLYBLD/kent/src
make utils >& make.utils.log
sed -i -e "s/-DJK_WARN//" make.utils.log
sed -i -e "s/-Werror//" make.utils.log
#-- to check for errors: 
set res = `/bin/egrep -y "error|warn" make.utils.log`
set wc = `echo "$res" | wc -w` 
if ( "$wc" != "0" ) then
 echo "errs found:"
 echo "$res"
 $WEEKLYBLD/unsymtrick.csh
 exit 1
endif

#this is redundant now as make utils in /kent/src does it automatically now
#echo
#echo "Building src/blat."
#cd $WEEKLYBLD/kent/src/blat
#make >& make.log
#sed -i -e "s/-DJK_WARN//" make.log
#sed -i -e "s/-Werror//" make.log
##-- to check for errors: 
#set res = `/bin/egrep -y "error|warn" make.log`
#set wc = `echo "$res" | wc -w` 
#if ( "$wc" != "0" ) then
# echo "errs found:"
# echo "$res"
# $WEEKLYBLD/unsymtrick.csh
# exit 1
#endif

#this is redundant now as make utils in /kent/src does it automatically now
#echo
#echo "Building src/hg utils."
#cd $WEEKLYBLD/kent/src/hg
#make utils >& make.utils.log
#sed -i -e "s/-DJK_WARN//" make.utils.log
#sed -i -e "s/-Werror//" make.utils.log
##-- to check for errors: 
#set res = `/bin/egrep -y "error|warn" make.utils.log`
#set wc = `echo "$res" | wc -w` 
#if ( "$wc" != "0" ) then
# echo "errs found:"
# echo "$res"
# $WEEKLYBLD/unsymtrick.csh
# exit 1
#endif

#this is redundant now as make utils in /kent/src does it automatically now
#echo
#echo "Building src/utils."
#cd $WEEKLYBLD/kent/src/utils
#make >& make.log
#sed -i -e "s/-DJK_WARN//" make.log
#sed -i -e "s/-Werror//" make.log
##-- to check for errors: 
#set res = `/bin/egrep -y "error|warn" make.log`
#set wc = `echo "$res" | wc -w` 
#if ( "$wc" != "0" ) then
# echo "errs found:"
# echo "$res"
# $WEEKLYBLD/unsymtrick.csh
# exit 1
#endif


# Undo Symlink trick
$WEEKLYBLD/unsymtrick.csh
echo "Restore: undoing Symlink Trick."

if ( "$MACHTYPE" == "x86_64" ) then
    echo
    echo "Doing make clean on src."
    cd $WEEKLYBLD/kent/src
    make clean>& make.clean.log
endif

echo
echo "Build of Utils on $HOST complete."
echo

exit 0

