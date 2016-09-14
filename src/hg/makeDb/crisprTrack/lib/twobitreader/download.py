#!/usr/bin/env python
"""
downloads a .2bit genome from UCSC
.. note:: please comply with the restrictions of use at
http://hgdownload.cse.ucsc.edu/downloads.html

and do not over-use this module

By default, genomes are saved to the current directory
"""
import sys

if sys.version_info > (3,):
    from urllib.request import urlopen
else:
    from urllib2 import urlopen

from shutil import copyfileobj
from os.path import exists, join
from os import getcwd


def save_genome(name, destdir=None, mode='ftp'):
    """
    tries to download a genome from UCSC by name

    for example, 'hg19' is at
    ftp://hgdownload.cse.ucsc.edu/goldenPath/hg19/bigZips/hg19.2bit
    """
    urlpath = "%s://hgdownload.cse.ucsc.edu/goldenPath/%s/bigZips/%s.2bit" % \
              (mode, name, name)
    if destdir is None:
        destdir = getcwd()
    remotefile = urlopen(urlpath)
    assert exists(destdir), 'Desination directory %s does not exist' % destdir
    with open(join(destdir, "%s.2bit" % name), 'wb') as destfile:
        copyfileobj(remotefile, destfile)
    return


def main():
    import sys
    if len(sys.argv) != 2:
        sys.exit('Example: python -m twobitreader.download hg19')
    else:
        save_genome(sys.argv[1])


if __name__ == '__main__':
    main()
