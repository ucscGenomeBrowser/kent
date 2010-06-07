#!/bin/csh -f

# $Header: /projects/compbio/cvsroot/kent/src/hg/encode/encodeTrackDb.csh,v 1.4 2005/11/29 22:08:07 kate Exp $

set schema = $1
set tempfile = /tmp/encodeTdb.$$
set newtable = trackDb_encodedev_new

echo "DROP TABLE IF EXISTS $newtable" | hgsql hg16
sed "s/trackDb/$newtable/" $schema | hgsql hg16
echo "SELECT * FROM trackDb WHERE grp <> 'encode'" | \
            hgsql hg16 -h hgwbeta -N > $tempfile
echo "SELECT * FROM trackDb WHERE grp = 'encode'" | hgsql -N hg16 >> $tempfile
echo "LOAD DATA LOCAL INFILE '"$tempfile"' INTO TABLE $newtable" | \
            hgsql hg16
echo "DROP TABLE IF EXISTS trackDb_encodedev_old" | hgsql hg16
echo "ALTER TABLE trackDb_encodedev RENAME TO trackDb_encodedev_old" | \
            hgsql hg16
echo "UPDATE $newtable SET visibility = 3 WHERE grp = 'encode'" | \
            hgsql hg16
echo "ALTER TABLE $newtable RENAME TO trackDb_encodedev" | \
            hgsql hg16
set rows = `hgsql hg16 -N -s -e "SELECT count(*) from trackDb_encodedev"`
echo "$rows rows in trackDb_encodedev"
rm $tempfile
exit 0
