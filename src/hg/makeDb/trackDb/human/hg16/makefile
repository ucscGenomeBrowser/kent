# Make symlinks to reused doc files, to simplify maintenance

all: UVA UCSD

clean::
	rm -f ${UVA_LINKS} ${UCSD_LINKS}
     
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
	encode_UCSD_ChIP_RNAP_HCT116.html \
	encode_UCSD_ChIP_RNAP_HELA.html \
	encode_UCSD_ChIP_RNAP_IMR90.html \
	encode_UCSD_ChIP_RNAP_THP1.html \
	encode_UCSD_ChIP_TAF250_HCT116.html \
	encode_UCSD_ChIP_TAF250_HELA.html \
	encode_UCSD_ChIP_TAF250_IMR90.html \
	encode_UCSD_ChIP_TAF250_THP1.html\

UCSD:    ${UCSD_LINKS}

encode_UCSD_ChIP_%.html: encode_UCSD_ChIP.html
	rm -f $@
	ln -s $< $@
