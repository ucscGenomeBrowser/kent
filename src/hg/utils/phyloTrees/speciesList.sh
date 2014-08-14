#!/bin/sh

if [ $# -ne 1 ]; then
    echo "usage: ./speciesList.sh <Nway.nh>"
    echo "will extract the species names from the nh file"
    exit 255
fi

export F=$1

sed 's/:[0-9\.][0-9\.]*//g; s/;//; /^ *$/d' ${F} | sed -e "s/'/xXx/g"  \
    | xargs echo | sed "s/ //g; s/,/ /g; s/xXx/'/g" | sed 's/[()]//g; s/,/ /g' \
    | tr '[ ]' '[\n]'


exit $?

 sed 's/[a-z][a-z]*_//g; s/:[0-9\.][0-9\.]*//g; s/;//; /^ *$/d' \
        gorGor3.11way.nh > tmp.nh
    echo `cat tmp.nh` | sed 's/ //g; s/,/ /g' > tree.nh
    sed 's/[()]//g; s/,/ /g' tree.nh > species.list
