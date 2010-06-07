
all:: rgbItemExamples.html

red-green-pantone.txt:	red-green.txt findPantone.pl pantone.txt
	./findPantone.pl red-green.txt > red-green-pantone.txt

gray-scale-pantone.txt:	gray-scale.txt findPantone.pl pantone.txt
	./findPantone.pl gray-scale.txt > gray-scale-pantone.txt

blue-green-pantone.txt:	blue-green.txt findPantone.pl pantone.txt
	./findPantone.pl blue-green.txt > blue-green-pantone.txt

blue-red-pantone.txt:	blue-red.txt findPantone.pl pantone.txt
	./findPantone.pl blue-red.txt > blue-red-pantone.txt

yellow-blue-pantone.txt:	yellow-blue.txt findPantone.pl pantone.txt
	./findPantone.pl yellow-blue.txt > yellow-blue-pantone.txt

black-red-pantone.txt:	black-red.txt findPantone.pl pantone.txt
	./findPantone.pl black-red.txt > black-red-pantone.txt

black-green-pantone.txt:	black-green.txt findPantone.pl pantone.txt
	./findPantone.pl black-green.txt > black-green-pantone.txt

black-blue-pantone.txt:	black-blue.txt findPantone.pl pantone.txt
	./findPantone.pl black-blue.txt > black-blue-pantone.txt

black-yellow-pantone.txt:	black-yellow.txt findPantone.pl pantone.txt
	./findPantone.pl black-yellow.txt > black-yellow-pantone.txt

cyan-magenta-pantone.txt:	cyan-magenta.txt findPantone.pl pantone.txt
	./findPantone.pl cyan-magenta.txt > cyan-magenta-pantone.txt

rgbItemExamples.html: hexToHtml.pl gray-scale-pantone.txt \
	black-red-pantone.txt black-green-pantone.txt black-blue-pantone.txt \
	blue-green-pantone.txt \
	red-green-pantone.txt blue-red-pantone.txt yellow-blue-pantone.txt \
	black-yellow-pantone.txt rainbow.txt \
	cyan-magenta-pantone.txt
	./hexToHtml.pl gray-scale.txt gray-scale-pantone.txt \
	black-red.txt black-red-pantone.txt black-green.txt \
	black-green-pantone.txt black-blue.txt black-blue-pantone.txt \
	blue-green.txt blue-green-pantone.txt red-green.txt \
	red-green-pantone.txt blue-red.txt blue-red-pantone.txt \
	yellow-blue.txt yellow-blue-pantone.txt \
	black-yellow.txt \
	black-yellow-pantone.txt cyan-magenta.txt cyan-magenta-pantone.txt \
		> rgbItemExamples.html

install::	rgbItemExamples.html
	rm -f /usr/local/apache/htdocs/admin/rgbItemExamples.html
	cp -p rgbItemExamples.html \
		/usr/local/apache/htdocs/admin/rgbItemExamples.html

clean::
	rm -f rgbItemExamples.html black-red-pantone.txt \
		black-green-pantone.txt black-blue-pantone.txt \
		black-yellow-pantone.txt blue-green-pantone.txt \
		blue-red-pantone.txt cyan-magenta-pantone.txt \
		gray-scale-pantone.txt red-green-pantone.txt \
		yellow-blue-pantone.txt
