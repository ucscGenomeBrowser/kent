#!/bin/bash

# quit if something within the script fails
set -beEu -o pipefail
source `which qaConfig.bash`
umask 002

################################
#
#  12-20-2016
#  Matthew Speir
#
#  getPubHubInfo.sh
#  Collects contact information for
#  hubs listed in hgcentral.hubPublic.
#  Outputs contact information to HTML-
#  formatted file.
#
################################

usage="""
Collect contact information for hubs in hgcentral.hubPublic on the RR. Places all
contact information into an HTML-formatted page.\nDefault output location:\n
/usr/local/apache/htdocs-genecats/qa/test-results/publicHubContactInfo\n\n
`basename $0` go [outputLocation]\n\n\
outputLocation is a path to the directory where output should be placed. If not set,
script assumes default location above.\n
"""

runScript=""
base=""

# Print usage statement
if (( $# < 1 )) || (( $# > 2 ))
then
	echo -e $usage
	exit 1
elif (( $# == 1 ))
then
	runScript=$1
elif (( $# == 2 ))
then
	# Check if outputLocation exists
	if [[ -d $2 ]]
	then
		runScript="$1"
		base="$2"
	else
		echo -e "Sorry, directory \"$2\" does not exist. Check spelling or create this directory and try again.\n"
		exit 1
	fi
fi

# If no user provided base location, then assume it's being run on dev and output to default loc.
if [[ "$base" == "" ]]
then
	base="/usr/local/apache/htdocs-genecats/qa/test-results/publicHubContactInfo"
fi

# Create "hubFiles" directory in base if it doesn't exist
mkdir -p $base/hubFiles

if [[ "$runScript" == "go" ]]
then
	contactFile="$base/publicHubContact.html"

	# Check if old contact file exists and if it does move it to archive file
	if [ -e $contactFile ]
	then
		mv $contactFile $contactFile.old.temp
	fi

	# Make header for html file
	echo -e "
	<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"
		\"http://www.w3.org/TR/html4/loose.dtd\">
	" >> $contactFile

	while read url label
	do
		# Save hub shortLabel as a file so we can use it in a filename later
		label=$(echo $label | sed -e 's/ /_/g')

		# Make name for hub.txt output file that includes hub shortLabel
		hubFile="$base/hubFiles/$label.hub.txt"
		wget -t 5 -O $hubFile $url &> /dev/null || true

		# Extract email from hub.txt file we saved
		email=$(egrep "^email" $hubFile) || true

		# If email is empty, then wget failed or hub is down
		if [[ $email == "" ]]
		then
			# Attempt to get hub.txt file w/ curl
			curl --retry 5 $url -o $hubFile  &> /dev/null || true
			# Extract email from hub.txt file we saved
			email=$(grep "^email" $hubFile) || true

			# If email is still empty, hub is likely down and
			# we want to use the last email we have as contact email
			if [[ $email == "" ]] && [ -e $contactFile.old ]
			then
				email=$(grep "$url" $contactFile.old.temp | awk '{print $4" "$5}') || true
			fi
		fi

		# Create hyperlinks to hub.txt files
		hubLink="<a href=\"$url\" target=\"_blank\">$label</a>"

		# Write contact email + hubUrl to file
		echo -e "$hubLink $email<br>" >> $contactFile
	
	done<<<"$(hgsql -h genome-centdb -Ne "select hubUrl,shortLabel from hubPublic" hgcentral)"
	# Get line counts for current/old contact info files
	lineCountNew=$(wc -l $contactFile | cut -d " " -f 1)
	lineCountOld=$(wc -l $contactFile.old.temp | cut -d " " -f 1)
	lineDiff=$(expr $lineCountNew - $lineCountOld)
	# find absolute value of line diff
	absLineDiff=$([ $lineDiff -lt 0 ] && echo $((-$lineDiff)) || echo $lineDiff)
	# Check if line diff is >5 lines
	if [ $(echo "$absLineDiff >= 5" | bc) -ne 0 ]
	then
		echo "Error: New contact file seems to be missing many entries from old file."
		echo "       Something may have failed while running. Try running again to"
		echo "       see if that helps."
		exit 1
	else
		mv $contactFile.old.temp $contactFile.old
	fi
else
	echo -e "Error: \"$1\" is not a valid input. Please see the usage message.\n\n"
	echo -e $usage
	exit 1
fi
