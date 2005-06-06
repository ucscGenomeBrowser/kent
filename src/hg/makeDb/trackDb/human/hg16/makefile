# Make symlinks to reused doc files, to simplify maintenance

all: UCSD

clean::
	rm -f ${UCSD_LINKS}

UCSD_LINKS = \
	encodeUcsdChipRnap.html \
	encodeUcsdChipTaf250.html \
	encodeUcsdChipAch3Imr90.html \
	encodeUcsdChipMeh3k4Imr90.html \

UCSD:    ${UCSD_LINKS}

encodeUcsdChip%.html: encodeUcsdChip.html
	rm -f $@
	ln -s $< $@
