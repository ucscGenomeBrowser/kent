kentSrc = ../..
A = stringify
include $(kentSrc)/inc/userApp.mk
L += -lm

# add bash version
installedStringifyEz = ${DESTDIR}${BINDIR}/stringifyEz

install:: ${installedStringifyEz}

${installedStringifyEz}: stringifyEz
	@mkdir -p $(dir $@)
	cp -p $< $@
