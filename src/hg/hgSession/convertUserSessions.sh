#!/usr/bin/env bash

HELP_STR="usage: convertUserSessions.sh userName
userName must be %-encoded as it is in namedSessionDb"

# cgiDir for hg.conf{,.private} settings
cgiDir=/usr/local/apache/cgi-bin

# delay (in seconds) between calls to sessionDataConvert.sh to avoid hammering server & hgcentral
delay=3

# Look for a setting in hg.conf (and hg.conf.private if not in hg.conf).
function getSetting()
{
    local settingName=$1
    local settingVal=$(grep ^$settingName $cgiDir/hg.conf | sed -e 's/.*=//;')
    if [ "$settingVal" == "" -a -f $cgiDir/hg.conf.private ] ; then
        settingVal=$(grep ^$settingName $cgiDir/hg.conf.private | sed -e 's/.*=//;')
    fi
    if [ "$settingVal" == "" ]; then
        echo "Unable to find $settingName setting in cgi-bin hg.conf or hg.conf.private"
        exit 1
    fi
    echo $settingVal
}

# Check args
if [[ $# -eq 0 ]] ; then
    echo "$HELP_STR"
    exit 0
elif [[ $# -ne 1 ]] ; then
    echo "Wrong number of arguments ($#; need 1)"
    echo "$HELP_STR"
    exit 1
fi
encUserName=$1

# Get sessions for user
centralHost=$(getSetting "central.host")
centralUser=$(getSetting "central.user")
centralPwd=$(getSetting "central.password")
centralDb=$(getSetting "central.db")
sessionNames=$(mysql -h $centralHost -u $centralUser -p"$centralPwd" $centralDb \
                 -NBe "select sessionName from namedSessionDb where userName = '$encUserName'")

# Save each one; quit if we encounter an error
for encSessionName in $sessionNames ; do
    if sessionDataConvert.sh "$encUserName" "$encSessionName"; then
        echo "Converted $encUserName $encSessionName"
    else
        exit 1
    fi
    echo -n "sleeping... "
    sleep $delay
done
