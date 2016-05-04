#!/bin/tcsh

####################
#  Script to clean, zip and move the browserbox
#  Max Haeussler
####################

if ( "$1" == "" ) then
    echo Please provide either alpha or beta as a parameter
    exit 0
endif

set runCount=`ps aux | grep VBoxHeadless | grep -w 'browserbox' | wc -l`
if ("$runCount" == "0") then
    VBoxHeadless -s browserbox &
    echo waiting for VM to boot...
    sleep 20
endif

echo cleaning the box
ssh box sudo /root/cleanVm.sh
echo one final check that dsa keys should not be found on browserbox root
ssh box sudo ls /root/.ssh/{id_dsa,id_dsa.pub}
if ( $status != 2) then
    echo "DSA SSH KEYS FOUND. THIS IS SHOULD NOT HAPPEN!"
    exit 1
endif
echo shutting down the box
VBoxManage  controlvm  browserbox acpipowerbutton
while (1) 
    set runCount=`ps aux | grep VBoxHeadless | grep -w 'browserbox' | wc -l`
    if ("$runCount" == "0") then
	 echo box has stopped
	 break
    else
	 echo box has not stopped yet, waiting...
	 sleep 3
    endif
end

cd ~/VirtualBox\ VMs/browserbox
echo removing shared folders
sed -i '/<SharedFolder name="/d' browserbox.vbox

echo switching off the RDP server of the box
VBoxManage modifyvm browserbox --vrde off

echo removing the old zip file
rm -f gbib.zip

echo zipping box
zip gbib.zip browserbox.vbox gbib-root.vdi gbib-data.vdi

echo switching RDP on again
VBoxManage modifyvm browserbox --vrde on

echo moving zip file to apache folder
#ll -lah gbib.zip
if ( "$1" == "beta" ) then
    rm -f /usr/local/apache/htdocs/gbib/gbibBeta.bak.zip
    mv /usr/local/apache/htdocs/gbib/gbibBeta.zip /usr/local/apache/htdocs/gbib/gbibBeta.bak.zip
    mv gbib.zip /usr/local/apache/htdocs/gbib/gbibBeta.zip
    ls -lah /usr/local/apache/htdocs/gbib/gbibBeta.zip
    echo Gbib beta can be downloaded now for testing from http://hgwdev.soe.ucsc.edu/gbib/gbibBeta.zip
else
    mv gbib.zip /usr/local/apache/htdocs/gbib/gbibAlpha.zip
    echo Gbib alpha can be downloaded now for testing from http://hgwdev.soe.ucsc.edu/gbib/gbibAlpha.zip
endif
