#!/bin/tcsh
source `which qaConfig.csh`
if ( "$HOST" != "hgwdev" ) then
 echo "Error: this script must be run from hgwdev."
 exit 1
endif

cd ~

#update sandbox 
cd ~/cron-sandbox/RL/
cvs update -dP releaseLog.html

#fetch releaseLog
#set output = "/usr/local/apache/htdocs/goldenPath/releaseLog.html"
set output = "~/cron-sandbox/RL/releaseLog.html"
echo "Fetching releaseLog.html from hgwbeta:qaPushQ"
wget 'http://hgwbeta.soe.ucsc.edu/cgi-bin/qaPushQ?action=releaseLog' -q -O $output
if ( $status ) then
 echo "wget failed for releaseLog.html on $HOST"
 exit 1
endif

#commit to cvs (old, replicates donnak's manual methods, probably could drop now)
set temp = '"'"Release log update"'"'
cvs commit -m "$temp" releaseLog.html
if ( $status ) then
 echo "cvs commit failed for releaseLog.html on $HOST"
 exit 1
endif
echo "cvs commit done for releaseLog.html on $HOST"

# ---

#update sandbox 
cd ~/cron-sandbox/RLencode/
cvs update -dP releaseLog.html

#fetch ENCODE releaseLog
#set output = "/usr/local/apache/htdocs/encode/releaseLog.html"
set output = "~/cron-sandbox/RLencode/releaseLog.html"
echo "Fetching encode releaseLog.html from hgwbeta:qaPushQ"
wget 'http://hgwbeta.soe.ucsc.edu/cgi-bin/qaPushQ?action=encodeReleaseLog' -q -O $output
if ( $status ) then
 echo "wget failed for encode releaseLog.html on $HOST"
 exit 1
endif

#commit to cvs (old, imitates donnak's manual methods, probably could drop now)
set temp = '"'"Encode release log update"'"'
cvs commit -m "$temp" releaseLog.html
if ( $status ) then
 echo "cvs commit failed for encode releaseLog.html on $HOST"
 exit 1
endif
echo "cvs commit done for encode releaseLog.html on $HOST"

# ---

echo "now copying regular and encode releaseLog.html to /goldenPath and /encode..."
cp ~/cron-sandbox/RL/releaseLog.html /usr/local/apache/htdocs/goldenPath/
cp ~/cron-sandbox/RLencode/releaseLog.html /usr/local/apache/htdocs/ENCODE/


# email push request unless -noemail specified, e.g. the cron job
if ( $1 == "-noemail" ) then
    echo "no email being sent"
else
    #use something like this when ready to automate more:
    set subject = "'Please push release logs'"
    set body = "Please push from hgwdev to hgwbeta and RR: \n /goldenPath/releaseLog.html \n /ENCODE/releaseLog.html \n\n Thanks! " 
    echo "$body" | mail -s "$subject" push-request
endif


echo "Release Log update done."

exit 0

