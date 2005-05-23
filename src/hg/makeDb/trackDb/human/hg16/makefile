# Make symlinks to reused doc files, to simplify maintenance

all: UVA UCSD GENCODE

clean::
	rm -f ${UVA_LINKS} ${UCSD_LINKS}

GENCODE_LINKS = \
        encodeGencodeGene.html \
        encodeGencodeIntron.html

GENCODE: ${GENCODE_LINKS}
     
encodeGencode%.html: encodeGencode.html
	rm -f $@
	ln -s $< $@

UVA_LINKS = \
	encode_UVA_DNARep_0_2.html \
	encode_UVA_DNARep_0_2.html \
	encode_UVA_DNARep_2_4.html \
	encode_UVA_DNARep_4_6.html \
	encode_UVA_DNARep_6_8.html \
	encode_UVA_DNARep_8_10.html 

UVA:    ${UVA_LINKS}

encode_UVA_DNARep_%.html: encode_UVA_DNARep.html
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
