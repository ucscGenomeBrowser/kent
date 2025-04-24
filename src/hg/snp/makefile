
SNP_DIRS=snpLoad snpMask snpValid dbSnpJsonToTab checkBigDbSnp

all:: ${SNP_DIRS:%=%_all}
%_all:
	${MAKE} -C $*

clean:: ${SNP_DIRS:%=%_clean}
%_clean:
	${MAKE} -C $* clean

compile:: ${SNP_DIRS:%=%_compile}
%_compile:
	${MAKE} -C $* compile
