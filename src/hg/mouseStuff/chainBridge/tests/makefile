kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

chainBridge = ${DESTBINDIR}/chainBridge

all:

test: awful someChange noChange
	${MAKE} clean

awful: mkdirs
	${chainBridge} input/hg19ToHg38.$@.chain /gbdb/{hg19,hg38}/hg??.2bit output/$@.bridged.chain
	diff -u expected/$@.bridged.chain output/$@.bridged.chain

someChange: mkdirs
	${chainBridge} input/hg19ToHg38.$@.chain /gbdb/{hg19,hg38}/hg??.2bit output/$@.bridged.chain
	diff -u expected/$@.bridged.chain output/$@.bridged.chain

noChange: mkdirs
	${chainBridge} input/hg19ToHg38.$@.chain /gbdb/{hg19,hg38}/hg??.2bit output/$@.bridged.chain
	diff -u expected/$@.bridged.chain output/$@.bridged.chain

clean::
	rm -rf output

mkdirs:
	@${MKDIR} output
