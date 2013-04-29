kentSrc = ../..
A = bedRemoveOverlap
include $(kentSrc)/inc/userApp.mk

clean::
	rm -f empty.out one.out test4.out threeLast.out twoFirst.out \
            twoSecond.out

test::
	bedRemoveOverlap empty.bed empty.out
	diff empty.out empty.expected
	rm empty.out
	bedRemoveOverlap one.bed one.out
	diff one.out one.expected
	rm one.out
	bedRemoveOverlap test4.bed test4.out
	diff test4.out test4.expected
	rm test4.out
	bedRemoveOverlap threeLast.bed threeLast.out
	diff threeLast.out threeLast.expected
	rm threeLast.out
	bedRemoveOverlap twoFirst.bed twoFirst.out
	diff twoFirst.out twoFirst.expected
	rm twoFirst.out
	bedRemoveOverlap twoSecond.bed twoSecond.out
	diff twoSecond.out twoSecond.expected
	rm twoSecond.out

