PREFIX=/hive/users/qateam/hubSpace

# the hgwdev copies, run from ${PREFIX} by the qateam crontab
HGWDEV_FILES=measure/measureHubSpace.dev.sh measure/measureHubSpaceWrapperHgwdev.bash \
	measure/hubSpaceReport.py measure/hubSpaceReport.css measure/hubSpaceReport.js \
	tusd/tusdTrashCleanerHgwdev.bash tusd/tusdTrashWrapperHgwdev.bash

# the hubspace machine copies, run from /home/qateam/hubSpace there
HUBSPACE_FILES=measure/measureHubSpace.sh measure/measureHubSpaceWrapperHubspace.bash \
	tusd/tusdTrashCleanerHubspace.bash tusd/tusdTrashWrapperHubspace.bash

install:
	cp ${HGWDEV_FILES} ${PREFIX}/

# only prints the command, so the hubspace copies are never overwritten by accident
hubspace:
	@echo "# NOT RUN: nothing was copied to hubspace."
	@echo "# To update the hubspace machine, run this command yourself:"
	@echo ""
	@echo "rsync -av $(addprefix ${CURDIR}/,${HUBSPACE_FILES}) qateam@hubspace:/home/qateam/hubSpace/"
