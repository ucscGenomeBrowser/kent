#!/bin/sh
#
#	Run this script periodically to clean browser trash directory
#
#	As a safety check and to get this script started correctly
#	you need to touch a CleanUp file in the trash directory:
#	touch ${WEBROOT}/CleanUp
#
#	Set WEBROOT - usually /var/www or /usr/local/apache
WEBROOT=/var/www
export WEBROOT

#	Go to the trash directory

cd ${WEBROOT}/trash

#	As a safety check to make sure we are in the correct location
#	before doing a potentially dangerous 'rm -f' check to ensure
#	there is a CleanUp file here.  This file will also be used as a
#	date/time stamp.  Only files older than it will be removed.
#	Thus a set of trash files will always exist that were created
#	between periods of this script running.
#
#	This sequence also verifies it is in a directory called trash

if [ -f CleanUp ]; then
        p=`pwd`
        bn=`basename $p`
        if [ "$bn" = "trash" ]; then
                find . ! -newer CleanUp | xargs rm -f
                touch CleanUp
        else
                echo "ERROR: not in ${WEBROOT}/trash ?"
        fi
else
	echo "ERROR: no CleanUp file found in ${WEBROOT}/trash ?"
fi

