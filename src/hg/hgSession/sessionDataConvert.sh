#!/usr/bin/env bash

# Load and re-save a session, so that trash files and/or customTrash tables can be moved
# to the locations specified by hg.conf's sessionDataDir and sessionDataDbPrefix.

HELP_STR="usage: sessionDataConvert.sh userName sessionName
Both userName and sessionName must be %-encoded as they are in namedSessionDb"

cgiDir=/usr/local/apache/cgi-bin
hgSessionUrl=https://$(hostname -f)/cgi-bin/hgSession

cookieJar=./.sessionDataConvert.cookieJar

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

# Get the hex string md5sum for input without adding a newline at the end.
function getMd5Sum()
{
    echo -n "$*" | md5sum | awk '{print $1;}'
}

# Decode %xx encodings into original characters
function urlDecode()
{
    echo -e "$(echo $* | sed 'y/+/ /; s/%/\\x/g')"
}

# Check args
if [[ $# -eq 0 ]] ; then
    echo "$HELP_STR"
    exit 0
elif [[ $# -ne 2 ]] ; then
    echo "Wrong number of arguments ($#; need 2)"
    echo "$HELP_STR"
    exit 1
fi
encUserName=$1
encSessionName=$2

# We need to forge cookies in order to save session as a particular user.
# Find wiki login cookies and login.cookieSalt in hg.conf.private or hg.conf.
loginUserName=$(getSetting "wiki.userNameCookie")
loginUserId=$(getSetting "wiki.loggedInCookie")
cookieSalt=$(getSetting "login.cookieSalt")
decUserName=$(urlDecode $encUserName)
saltedUserName=$(getMd5Sum "$cookieSalt-"$(getMd5Sum "$decUserName"))
loginCookieString="$loginUserName=$encUserName; $loginUserId=$saltedUserName"

# Make a random dayOfMonth customData suffix, so all sessions' db tables don't end up
# in the same directory because they were all saved by this script on the same day.
randomDay=$(printf "%02u" $((($RANDOM % 31) + 1)))

# Save the cart for hgsid as the same session.
params=( "hgS_doReSaveSession=submit"
         "hgS_newSessionName=$encSessionName"
         "hgS_sessionDataDbSuffix=$randomDay"
       )
cgiString=$(IFS=\& ; echo "${params[*]}")

updated=$(curl -k -s -S -c $cookieJar -b $cookieJar -b "$loginCookieString" \
            "$hgSessionUrl?$cgiString" \
          | grep 'be shared with others' \
          | grep "$encSessionName")
if [ $? -ne 0 ]; then
    echo "hgSession request failed to save ($encUserName, $encSessionName)."
    echo curl -s -S -c $cookieJar -b $cookieJar -b "'$loginCookieString'" "'$hgSessionUrl?$cgiString'"
    exit 1
fi
if [ "$updated" == "" ]; then
    echo "hgSession response missing session update message"
    exit 1
fi
