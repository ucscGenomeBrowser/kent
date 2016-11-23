#!/bin/bash

base="/usr/local/apache/htdocs-genecats/qa/test-results/publicHubContactInfo"
contactFile="$base/publicHubContact.html"

# Check if old contact file exists and if it does move it to archive file
if [ -e $contactFile ]
then
	mv $contactFile $contactFile.old
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
	wget -t 5 -O $hubFile $url &> /dev/null

	# Extract email from hub.txt file we saved
	email=$(egrep "^email" $hubFile)

	# If email is empty, then wget failed or hub is down
	if [[ $email == "" ]]
	then
		# Attempt to get hub.txt file w/ curl
		curl --retry 5 $url -o $hubFile  &> /dev/null
		# Extract email from hub.txt file we saved
		email=$(grep "^email" $hubFile)

		# If email is still empty, hub is likely down and
		# we want to use the last email we have as contact email
		if [[ $email == "" ]] && [ -e $contactFile.old ]
		then
			email=$(grep "$url" $contactFile.old | awk '{print $4" "$5}')
		fi
	fi

	# Create hyperlinks to hub.txt files
	hubLink="<a href=\"$url\" target=\"_blank\">$label</a>"

	# Write contact email + hubUrl to file
	echo -e "$hubLink $email<br>" >> $contactFile
	
done<<<"$(hgsql -h genome-centdb -Ne "select hubUrl,shortLabel from hubPublic" hgcentral)"
