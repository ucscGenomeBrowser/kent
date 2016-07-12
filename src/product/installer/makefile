kentSrc = ../..
include ../../inc/common.mk

user:
	rm $(DOCUMENTROOT)-$(USER)/goldenPath/help/gbic.html
	pandoc -t html5 -o $(DOCUMENTROOT)-$(USER)/goldenPath/help/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
	chmod a+x $(DOCUMENTROOT)-$(USER)/goldenPath/help/gbic.html
alpha:
	rm $(DOCUMENTROOT)/goldenPath/help/gbic.html
	pandoc -t html5 -o $(DOCUMENTROOT)/goldenPath/help/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
	chmod a+x $(DOCUMENTROOT)/goldenPath/help/gbic.html
beta:
	rm $(DOCUMENTROOT)-beta/goldenPath/help/gbic.html
	pandoc -t html5 -o $(DOCUMENTROOT)-beta/goldenPath/help/gbic.html README.md --template staticPage.html -t ../mirrorDocs/staticPage.lua
	chmod a+x $(DOCUMENTROOT)-beta/goldenPath/help/gbic.html
	rsync ${rsyncOpts} $(DOCUMENTROOT)-beta/goldenPath/help/gbic.html qateam@hgwbeta:$(DOCUMENTROOT)/goldenPath/help/

# Run rsync.  Care is taking to preserve times and mode on files.  rsync is
# smart about recreating file under the current users so that times and
# permissions maybe modified.  However, problems happen when a directory
# is not owned by the current users.  These options attemp to work around it.
# use care in changing
rsyncOpts = --times --recursive --omit-dir-times --cvs-exclude --no-relative
