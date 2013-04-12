kentSrc = ../../..
A = txgToXml
include ${kentSrc}/inc/userApp.mk

test::
	txgToXml -verbose=0 input.txg output.xml
	diff expected.xml output.xml

clean::
	rm -f output.xml
