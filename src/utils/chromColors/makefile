
all:	palette.txt palette.html palette.png new_colorchrom.gif

palette.txt: ../../hg/inc/chromColors.h hToTxt.pl
	./hToTxt.pl ../../hg/inc/chromColors.h > palette.txt

palette.html:	palette.txt paletteToHtml.pl
	./paletteToHtml.pl palette.txt > palette.html

palette.png:	hexToPng.pl palette.txt
	./hexToPng.pl palette.txt | sh

new_colorchrom.gif:	palette.png
	convert palette.png new_colorchrom.gif

clean:
	rm -f palette.png chromColor.png palette.txt palette.html \
		new_colorchrom.gif

