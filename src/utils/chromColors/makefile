
all:	palette.txt palette.html palette.png

palette.txt: ../../hg/inc/chromColors.h hToTxt.pl
	./hToTxt.pl ../../hg/inc/chromColors.h > palette.txt

palette.html:	palette.txt paletteToHtml.pl
	./paletteToHtml.pl palette.txt > palette.html

palette.png:	hexToPng.pl palette.txt
	./hexToPng.pl palette.txt | sh

clean:
	rm -f palette.png chromColor.png palette.txt palette.html

