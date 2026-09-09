#!/bin/bash
# Put the bTaeGut7 tracks into a GenArk assembly's contrib/ directory.
#
# A contributed track reaches an assembly hub in two pieces: a trackDb fragment that
# mkGenomes.pl concatenates into the generated hub.txt, and a directory of symlinks that
# the fragment's paths point at.  Both live under the assembly's build directory:
#
#   <buildDir>/contrib/bTaeGut7/bTaeGut7.trackDb.txt   <- the name mkGenomes.pl globs for
#   <buildDir>/contrib/bTaeGut7/<data and html>
#
# The fragment is not the same file as the hub's trackDb.  A hub's trackDb sits next to its
# data, so its paths are bare file names; a contrib fragment is read from the hub root, so
# every bigDataUrl and html has to be prefixed with contrib/bTaeGut7/.  This script derives
# the fragment from the master trackDb rather than keeping a second copy by hand.
#
# The data comes from the split hub, whose sequences are already named the way the assembly
# names them, so the browser needs no chromAlias indirection and no paternal features are
# carried on a maternal assembly.
#
# Once the links are in place, alpha.hub.txt picks the tracks up on the next
#   mkGenomes.pl dynablat-01 4040 <one line asmId tsv>
# with no entry in any control list.  betaGenArk.txt and publicGenArk.txt are what promote
# it beyond alpha, and those are Hiram's to edit.
#
# Usage: mkContribLinks.sh <accession> <buildDir>
set -e

if [ $# -ne 2 ]; then
    echo "usage: mkContribLinks.sh <accession> <buildDir>" >&2
    echo "  e.g. mkContribLinks.sh GCF_048771995.1 \\" >&2
    echo "       /hive/data/genomes/asmHubs/refseqBuild/GCF/048/771/995/GCF_048771995.1_bTaeGut7.mat" >&2
    exit 255
fi
acc="$1"
buildDir="$2"
name=bTaeGut7
scriptDir="$(cd "$(dirname "$0")" && pwd)"
master="$scriptDir/../../../trackDb/contrib/$name/$name.trackDb.txt"
src="/hive/data/outside/genark/$name/split/$acc"
staging="/hive/data/outside/genark/$name/contrib/$acc"

[ -s "$master" ] || { echo "no master trackDb at $master" >&2; exit 1; }
[ -d "$src" ]    || { echo "no split hub data at $src" >&2; exit 1; }
[ -d "$buildDir" ] || { echo "no build directory at $buildDir" >&2; exit 1; }
# contrib/ in a build directory is a real directory; refuse to follow a symlink into
# somewhere unintended, which is easy to do by accident here
if [ -L "$buildDir/contrib" ]; then
    echo "$buildDir/contrib is a symlink, refusing to write through it" >&2
    exit 1
fi

# the fragment, kept in our own area so the build tree holds only symlinks
mkdir -p "$staging"
sed -E 's#^([[:space:]]*)(bigDataUrl|html) #\1\2 contrib/'"$name"'/#' \
    "$master" > "$staging/$name.trackDb.txt"
echo "wrote $staging/$name.trackDb.txt"

dest="$buildDir/contrib/$name"
mkdir -p "$dest"
ln -sfn "$staging/$name.trackDb.txt" "$dest/$name.trackDb.txt"
n=0
for f in "$src"/*.bb "$src"/*.bw "$src"/*.html; do
    [ -e "$f" ] || continue
    ln -sfn "$f" "$dest/$(basename "$f")"
    n=$((n+1))
done
echo "linked $n files into $dest"
