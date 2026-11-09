# multiRegionSpaceKey.sql:  the conditions of a multi-region view, and the region
# list they produce.  Lives in hgcentral.  refs #37788
#
# A synthetic multi-region coordinate space is named by the SHA1 of its region list,
# and its chain file is that SHA1 in the trash, so nothing has to remember where a
# chain is.  What cannot be derived is the SHA1 itself:  computing it means building
# the region list, and in exon or gene view that is a scan of the whole gene table.
#
# This table is the way around that.  cacheKey describes the view in the terms the
# browser already has in hand before it reads any regions (assembly, sequence, mode,
# gene table, its filters and search matches, and when that table was last loaded;
# for a single transcript, the track, item, chrom and coding-only flag), and sha1 is
# the region list those conditions produce.  A hit skips the scan entirely.
#
# The browser never removes a row.  Old rows are left for a trash cleaner to remove
# by lastAccess.
#
# keyHash, not cacheKey, carries the unique index:  a cacheKey is free-form text with
# no useful length limit, and an index on a prefix of it would treat two different
# views that share a prefix as the same view.
#
# Nothing here is authoritative.  Every row can be recomputed from the conditions it
# names, and a row whose chain the trash cleaner has taken is still correct: the chain
# is rebuilt under the same name.  The table can be emptied at any time; the only cost
# is that the next request for each view pays for its region list again.

CREATE TABLE multiRegionSpaceKey (
    id int unsigned not null auto_increment,	# Auto-incrementing ID
    keyHash char(40) not null,	# SHA1 of cacheKey, which is what is looked up
    cacheKey longblob not null,	# The conditions, in the clear, for reading by a person
    sha1 char(40) not null,	# SHA1 of the assembly name and the sorted region list
    firstAccess varchar(255) not null,	# Time these conditions were first recorded
    lastAccess varchar(255) not null,	# Time these conditions were last used
              #Indices
    PRIMARY KEY(id),
    UNIQUE(keyHash)
);
