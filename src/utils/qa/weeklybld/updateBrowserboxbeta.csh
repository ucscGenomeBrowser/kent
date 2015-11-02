#!/bin/tcsh
#cd $WEEKLYBLD
cd $HOME

# updates browserboxbeta from browserbox zip
# stops and unregisters it,
# renames old for backup,
# patches vm description
# registers new vm
# restarts new vm

if ( "$USER" != "qateam" ) then
    echo "must run this script as qateam"
    exit 1
endif

set BRANCHNN=$1
if ( $BRANCHNN < 300 ) then
    echo "must specify the plain version number on the commandline as a number, e.g. 324"
    exit 1
endif
echo "BRANCHNN=$BRANCHNN"

# stop the box
echo "checking if browserboxbeta is running"
set runCount=`VBoxManage list runningvms | grep -c '"browserboxbeta"'`
#echo "runCount=$runCount"
if ( "$runCount" == "0" ) then
    echo "browserboxbeta was not running"
else
    echo "stopping browserboxbeta"
    VBoxManage controlvm browserboxbeta acpipowerbutton
    sleep 15
    while ( 1 )
        set runCount=`VBoxManage list runningvms | grep -c '"browserboxbeta"'`
        #echo "runCount=$runCount"
        if ( "$runCount" == "0" ) then
            echo "vm browserboxbeta stopped"
            break
        endif
        echo "waiting for vm browserboxbeta to stop"
        sleep 15
    end
endif

echo "un-registering the old browserboxbeta"
VBoxManage unregistervm browserboxbeta
sleep 5

cd "VirtualBox VMs"


# Find the next unused backup name, either the previous version, or a patched a,b,c
@ prevNN = ( $BRANCHNN - 1 )
echo "prevNN=$prevNN"
if ( -d "browserboxbeta.v${prevNN}" ) then
    set x = 97   # "a"
    while ( 1 )
        # convert number x to ascii character suf
        set suf = `echo "$x" | awk '{printf "%c",$0 }'`
        echo "suf=$suf"
        if ( -d "browserboxbeta.v${BRANCHNN}${suf}" ) then
            @ x = ( $x + 1 )
        else
             echo "backup: mv browserboxbeta browserboxbeta.v${BRANCHNN}${suf}"
             mv browserboxbeta browserboxbeta.v${BRANCHNN}${suf}
             break
        endif
    end
else
    echo "backup: mv browserboxbeta browserboxbeta.v${prevNN}"
    mv browserboxbeta browserboxbeta.v${prevNN}
endif


# unzip the new version
echo "unzipping gbibBeta.zip, which will probably take about 4 minutes."
mkdir browserboxbeta
cd browserboxbeta
unzip /usr/local/apache/htdocs/gbib/gbibBeta.zip

echo "patching the config to rename it to browserboxbeta"
mv browserbox.vbox browserboxbeta.vbox
sed -i -e 's/browserbox/browserboxbeta/; s/1234/1236/; s/1235/1237/;' browserboxbeta.vbox

cd $HOME
echo "registering the new browserboxbeta"
VBoxManage registervm $HOME/"VirtualBox VMs/browserboxbeta/browserboxbeta.vbox"
sleep 5

# start the box
echo "check if the box is running"
set runCount=`VBoxManage list runningvms | grep -c '"browserboxbeta"'`
#echo "runCount=$runCount"
if ( "$runCount" == "0" ) then
    echo "starting browserboxbeta"
    #nice +19 VBoxHeadless -s browserboxbeta &
    VBoxManage startvm "browserboxbeta" --type headless
    #when I switched to using VBoxManage instead of VBoxHeadless,
    # I had no problems with returning to the prompt after the script runs.
    echo "waiting for browserboxbeta"
    sleep 15
    while ( 1 )
        set runCount=`VBoxManage list runningvms | grep -c '"browserboxbeta"'`
        #echo "runCount=$runCount"
        if ( "$runCount" == "1" ) then
            break
        endif
        echo "waiting for vm browserboxbeta to start"
        sleep 15
    end
endif

echo "browserboxbeta vm updated and running"

