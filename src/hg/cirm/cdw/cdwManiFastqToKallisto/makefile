normal::
	 cp -p cdwManiFastqToKallisto ${HOME}/bin
	 cp -p cdwManiFastqToKallisto ${HOME}/kent/src/pyLib/scripts
test::
	@if test -d tests -a -s tests/makefile; then (cd tests && ${MAKE} test); \ 
	else echo "# no tests directory (or perhaps no tests/makefile) in $(CURDIR)"; fi
