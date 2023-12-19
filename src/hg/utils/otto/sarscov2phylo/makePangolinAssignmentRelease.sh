#!/bin/bash
set -beEu -o pipefail

usage() {
    echo "usage: $0 cacheFile versionTag"
    echo "cacheFile should be .csv.gz and versionTag should be like v1.3"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

set -x

cacheFile=$1
versionTag=$2

releaseDir=/hive/users/angie/pangolin-assignment
releaseFile=$releaseDir/pangolin-assignment-$versionTag.tar.gz

tmpDir=$(mktemp -d)
pushd $tmpDir

tarDir=pangolin-assignment-$versionTag
mkdir -p $tarDir/pangolin_assignment

cat > $tarDir/setup.py <<EOF
from setuptools import setup, find_packages
import glob
import os
import pkg_resources
# Note: the _program variable is set in __init__.py.
# it determines the name of the package/final command line tool.
from pangolin_assignment import __version__, _program

setup(name='pangolin_assignment',
      version=__version__,
      packages=find_packages(),
      scripts=[],
      package_data={'pangolin_assignment':['usher_assignments.cache.csv.gz']},
      description='cached pangolin assignments',
      url='https://hgdownload.gi.ucsc.edu/goldenPath/wuhCor1/pangolin-assignment',
      author='cov-lineages group',
      entry_points="""
      [console_scripts]
      {program} = pangolin_assignment.command:main
      """.format(program = _program),
      include_package_data=True,
      keywords=[],
      zip_safe=False)
EOF

cat > $tarDir/pangolin_assignment/__init__.py <<EOF
_program = "pangolin-assignment"
__version__ = "$versionTag"
EOF

cp -p $cacheFile $tarDir/pangolin_assignment/usher_assignments.cache.csv.gz

tar cvzf $releaseFile $tarDir

popd
rm -rf $tmpDir
