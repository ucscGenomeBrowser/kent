#!/bin/sh

./bin/$MACHTYPE/binTest -oldStyle
R0=$?
./bin/$MACHTYPE/binTest -newStyle
R1=$?

if [ "$R0" -ne 0 ]; then
    exit 255
fi

if [ "$R1" -ne 0 ]; then
    exit 255
fi

exit 0

New scheme, top level 0 bin is 4Gb in size and numbered 4681

next level 1, 8 bins, each bin is 512M in size, numbered 4682-4689
and 4682 is not possible because anything in that range
is in the old scheme.  And bins 4686-4689 are not possible due
to the int size limit

next level 2, 64 bins, each bin is 64M in size, numbered 4690-4753
bins 4690-4697 are not possible because they are in the old scheme.
And bins 4722-4753 are not possible due to the 2Gb int size limit

next level 3, 512 bins, each bin is 8M in size, numbered 4754-5265
bins 4754-4817 are not possilbe because they are in the old scheme.
And bins 5010-5265 are not possible due to the 2Gb int size limit

next level 4, 4096 bins, each bin is 1M in size, numbered 5266-9361
bins 5266-5777 are not possible because they are in the old scheme.
And bins 7314-9361 are not possible due to the 2Gb int size limit

next level 5, 32768 bins, each bin is 128K in size, numbered 9362-42129
bins 9362-13457 are not possible because they are in the old scheme.
And bins 25746-42129 are not possible due to the 2Gb int size limit

level   start   end     old   scheme    past  2Gb       Allowed
  # bins                start    end    start   end     start   end
0     1 4691    4691                                    4691	4691
1     8 4682	4689	4682	4682	4686	4689	4683	4685
2    64 4690	4753	4690	4697	4722	4753	4698	4721
3   512 4754	5265	4754	4817	5010	5265	4818	5009
4  4096 5266	9361	5266	5777	7314	9361	5778	7313
5 32768 9362	42129	9362	13457	25746	42129	13458	25745

old scheme       bin numbers  maxEnd 536,870,912
level  #bins   start   end
0	1	0	0
1	8	1	8
2	64	9	72
3	512	73	584
4	4096	585	4680
