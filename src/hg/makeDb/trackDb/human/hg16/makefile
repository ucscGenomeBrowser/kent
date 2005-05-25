# Make symlinks to reused doc files, to simplify maintenance

all: UCSD GENCODE

clean::
	rm -f ${UCSD_LINKS}

GENCODE_LINKS = \
        encodeGencodeGene.html \
        encodeGencodeIntron.html

GENCODE: ${GENCODE_LINKS}
     
encodeGencode%.html: encodeGencode.html
	rm -f $@
	ln -s $< $@

UCSD_LINKS = \
	encodeUcsdChipRnap.html \
	encodeUcsdChipTaf250.html \
	encodeUcsdChipAch3Imr90.html \
	encodeUcsdChipMeh3k4Imr90.html \

UCSD:    ${UCSD_LINKS}

encodeUcsdChip%.html: encodeUcsdChip.html
	rm -f $@
	ln -s $< $@
