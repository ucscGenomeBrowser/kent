#!/bin/tcsh
cd $WEEKLYBLD

echo "Make zip [${0}: `date`]"
./makeZip.csh
set err = $status
if ( $err ) then
    echo "error running makezip.csh: $err [${0}: `date`]" 
    exit 1
endif 
./buildZip.csh
set err = $status
if ( $err ) then
    echo "error running buildzip.csh: $err [${0}: `date`]"  
    exit 1
endif

echo "removing old jksrc zip and symlink [${0}: `date`]"
ssh -n qateam@hgdownload "rm /mirrordata/apache/htdocs/admin/jksrc.zip"
#ssh -n qateam@hgdownload2 "rm /mirrordata/apache/htdocs/admin/jksrc.zip"
ssh -n qateam@hgdownload3 "rm /mirrordata/apache/htdocs/admin/jksrc.zip"
ssh -n qateam@genome-euro "rm /mirrordata/apache/htdocs/admin/jksrc.zip"

echo "scp-ing jksrc.v${BRANCHNN}.zip to hgdownload(s) [${0}: `date`]"
scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@hgdownload:/mirrordata/apache/htdocs/admin/
#scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@hgdownload2:/mirrordata/apache/htdocs/admin/
scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@hgdownload3:/mirrordata/apache/htdocs/admin/
scp -p $BUILDDIR/zips/"jksrc.v"$BRANCHNN".zip" qateam@genome-euro:/mirrordata/apache/htdocs/admin/

echo "updating jksrc.zip symlink [${0}: `date`]"
ssh -n qateam@hgdownload "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"
#ssh -n qateam@hgdownload2 "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"
ssh -n qateam@hgdownload3 "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"
ssh -n qateam@genome-euro "cd /mirrordata/apache/htdocs/admin/;ln jksrc.v${BRANCHNN}.zip jksrc.zip"

# hgdocs/js/*.js files are not apparently needed on hgdownload as there is no directory there
#echo "scp-ing js/*.js files to hgdownload [${0}: `date`]"
#scp -p $BUILDDIR/v${BRANCHNN}_branch/kent/src/hg/js/*.js qateam@hgdownload:/mirrordata/apache/htdocs/js

# Attach the submodule-complete source archives to the GitHub release. GitHub's
# auto-generated "Source code" tar.gz/zip are produced by `git archive`, which
# omits submodule contents (src/submodules/htslib), so users who download those
# can not build (GitHub issue #102, refs #37741). The archives makeZip.csh built
# include the submodule, so we upload them as release assets. Failures here only
# warn: the zip is already on hgdownload, and autoBuild.sh treats a nonzero exit
# from this script as fatal, which we do not want for a GitHub API hiccup.
cd $WEEKLYBLD
git fetch --tags >& /dev/null
# release tag is the highest v${BRANCHNN}_branch.N (same logic as autoBuild.sh)
set tag = `git tag | grep "v${BRANCHNN}_branch" | sort -t. -k2 -n | tail -1`
# The GitHub asset filename is the basename of the uploaded file, so hardlink the
# versioned build products to version-less names for the release (the release/tag
# already carries the version). This mirrors the jksrc.zip hardlink on hgdownload.
set zipFile = "$BUILDDIR/zips/kent.src.zip"
set tgzFile = "$BUILDDIR/zips/kent.src.tar.gz"
ln -f "$BUILDDIR/zips/jksrc.v"$BRANCHNN".zip" "$zipFile"
ln -f "$BUILDDIR/zips/jksrc.v"$BRANCHNN".tar.gz" "$tgzFile"
if ( "$tag" == "" ) then
 echo "WARNING: no v${BRANCHNN}_branch release tag found; skipping GitHub upload [${0}: `date`]"
else
 # GitHub is a MIRROR of the internal origin (build@hgwdev:/data/git/kent.git);
 # the release tag we just pushed to origin has to reach github -- at THIS commit --
 # before we make a release from it. If we ran `gh release create` before the tag
 # mirrored, gh would create the tag at github's default-branch (master) HEAD: the
 # WRONG commit. So resolve the tag to its commit locally and confirm github agrees
 # before proceeding; and pass --target that commit so `create` pins the right SHA
 # even if the mirror is momentarily behind. refs #37741.
 set relSha = `git rev-list -n 1 "$tag"`
 echo "Release tag $tag -> $relSha (local); verifying GitHub has mirrored it [${0}: `date`]"
 set ghSha = ""
 @ tries = 0
 while ( $tries < 20 )
  set ghSha = `gh api "repos/ucscGenomeBrowser/kent/commits/$tag" --jq .sha`
  if ( "$ghSha" == "$relSha" ) break
  @ tries++
  echo "  GitHub not current yet (has '$ghSha', want '$relSha'); waiting 30s [try $tries/20]"
  sleep 30
 end
 if ( "$ghSha" != "$relSha" ) then
  echo "WARNING: GitHub tag $tag not confirmed at $relSha after ~10 min; proceeding with --target to force the correct commit [${0}: `date`]"
 endif

 echo "Attaching source archives to GitHub release $tag [${0}: `date`]"
 gh release view "$tag" --repo ucscGenomeBrowser/kent >& /dev/null
 if ( $status == 0 ) then
  # release already exists (e.g. re-run, or created by hand); replace the assets
  gh release upload "$tag" "$zipFile" "$tgzFile" --repo ucscGenomeBrowser/kent --clobber
 else
  # release not created yet; create it now so the assets have a home. Use the
  # curated markdown notes if they have been generated, else GitHub's auto notes.
  # --target "$relSha" pins the tag/release to the correct commit regardless of
  # mirror timing (ignored when the tag already exists on github).
  set notes = "$WEEKLYBLD/markdownReleaseNotes/v${BRANCHNN}_markdown.txt"
  if ( -e "$notes" ) then
   gh release create "$tag" "$zipFile" "$tgzFile" --repo ucscGenomeBrowser/kent \
     --target "$relSha" --title "v${BRANCHNN} Release" --notes-file "$notes"
  else
   gh release create "$tag" "$zipFile" "$tgzFile" --repo ucscGenomeBrowser/kent \
     --target "$relSha" --title "v${BRANCHNN} Release" --generate-notes
  endif
 endif
 if ( $status != 0 ) then
  echo "WARNING: gh release upload failed (status $status); source archives not attached to $tag [${0}: `date`]"
 endif
endif

echo "Done. [${0}: `date`]"
