#!/bin/tcsh
cd $WEEKLYBLD

# this scripts starts the main gbib VM on hgwdev, creates an ssh key on it, adds this key to the build account
# the triggers an update on the box using the build account
# it finally cleans the VM, stops and zips it into gbibBeta.zip

# make sure that the keys are always removed if this script somehow breaks
echo cleaning keys; 
sed -i '/browserbox/d' ~/.ssh/authorized_keys

# start the box
echo "start the box"
set runCount=`VBoxManage list runningvms | grep -c '"browserbox"'`
#echo "runCount=$runCount"
if ( "$runCount" == "0" ) then
    #VBoxHeadless -s browserbox &
    VBoxManage startvm "browserbox" --type headless
    sleep 15
    while ( 1 )
        set runCount=`VBoxManage list runningvms | grep -c '"browserbox"'`
        #echo "runCount=$runCount"
        if ( "$runCount" == "1" ) then
            break
        endif
        echo "waiting for vm browserbox to start"
        sleep 15
    end
endif

echo logging into box and creating new public key
ssh box 'sudo shred -n 1 -zu /root/.ssh/{id_dsa,id_dsa.pub}; cat /dev/zero | sudo ssh-keygen -t dsa -f /root/.ssh/id_dsa -N ""'

echo adding the new key to the build key file on hgwdev
ssh box sudo cat /root/.ssh/id_dsa.pub >> ~/.ssh/authorized_keys

#check if an rsync update is still running
# because after starting up the box it 
# may automatically rsync if it has not been run for weeks.
while ( 1 )
    set rsyncCount=`ssh box ps -ef | grep -c rsync`
    #echo "rsyncCount=$rsyncCount"
    if ( "$rsyncCount" == "0" ) then
        break
    endif
    sleep 15
end

echo logging into box and starting the update process via the build account
ssh box sudo /root/updateBrowser.sh hgwdev build beta

echo removing key from build key file and from box
ssh box 'sudo shred -n 1 -zu /root/.ssh/{id_dsa,id_dsa.pub}'
sed -i '/browserbox/d' ~/.ssh/authorized_keys

echo running boxRelease
./boxRelease.csh beta

echo copying gbibBeta.zip to gbibV${BRANCHNN}.zip
cp -p /usr/local/apache/htdocs/gbib/gbibBeta.zip /hive/groups/browser/vBox/gbibV${BRANCHNN}.zip
