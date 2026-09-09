#!/bin/sh
# Build the python environment that uniprotToTab needs, in venv/ next to this script.
# Run this whenever the preflight check in doUpdate.sh says the parser cannot start,
# e.g. after hgwdev gets a new python. Takes about a minute.
#
# Run it as a user who can write to the otto directory, not as otto.
#   cd /hive/data/outside/otto/uniprot && ./makeVenv.sh

set -e

cd `dirname $0`
venvDir=venv

# uniprotToTab needs lxml, which is not in the python standard library and is not
# installed system-wide on hgwdev. A per-user "pip install --user" is not enough either:
# cron runs this pipeline as otto, which does not see anyone else's ~/.local.
modules="lxml"

# --copies gives the venv its own copy of the python binary instead of a symlink to
# /usr/bin/python3. A symlink silently follows a system python upgrade while the
# compiled modules in the venv stay behind, which is how this environment broke before.
if [ -d $venvDir ] ; then
    echo "Removing the old $venvDir"
    rm -rf $venvDir
fi

echo "Building $venvDir with `/usr/bin/python3 -V 2>&1`"
/usr/bin/python3 -m venv --copies $venvDir

$venvDir/bin/pip install --quiet --upgrade pip
$venvDir/bin/pip install --quiet $modules

# cron runs as otto, so everything has to be readable and executable by everyone
chmod -R a+rX $venvDir

echo
echo "Installed:"
$venvDir/bin/pip list 2>/dev/null | grep -i -E "lxml|^Package|^---"
echo
# Check with an empty environment, so we know the venv stands on its own and is not
# quietly borrowing a module from whoever happens to run it.
if env -i $venvDir/bin/python -c "import lxml.etree; print('lxml', lxml.etree.__version__, 'from', lxml.__file__)" ; then
    echo "$venvDir is ready"
else
    echo "$venvDir is broken, lxml does not import" >&2
    exit 1
fi
