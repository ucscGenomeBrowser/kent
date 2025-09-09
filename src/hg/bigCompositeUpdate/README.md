# About bigComposite

For whomever is testing.

- `bigComposite` should be in the `src/hg/makefile` among targets, and in my
  mirror it gets installed to `apache/cgi-bin-${USER}` as expected.

- For the bigComposite controls page to work, the `src/hg/js/bigComposite.js`
  needs to be in the `apache/htdocs/js` dir, and the
  `src/hg/htdocs/style/bigComposite.css` needs to be in
  `apache/htdocs/style`. I don't know how to modify the build process for that
  to be automatic.

- Testing data for the trackDb entries can be found at:
  ```console
  https://smithlabresearch.org/data/mb2_trackDb.txt
  ```

  This is for hg38. It can be entered into a `hg38.trackDb` table like this:
  ```console
  mysql hg38 --local-infile=1 -e "
      LOAD DATA LOCAL INFILE 'mb2_trackDb.txt'
      REPLACE
      INTO TABLE trackDb
      FIELDS TERMINATED BY '\t'
      LINES TERMINATED BY '\n';
  "
  ```

- This file has one entry (the first) named `mb2`, which is the parent and has
  type bigComposite. Then it has many entries named like
  `mb2_dataElement_dataType`, where the `dataElement` is an SRA experiment
  accession (for a WGBS methylome), and `dataType` is in
  `{hmr,levels,reads}`. The bigBed/bigWig files are hosted on
  smithlab.usc.edu.

- The corresponding metadata can be found at:
  ```console
  https://smithlabresearch.org/data/mb2_metadata.json
  ```

- The metadata file includes some "settings" near the top, and a sub-dict
  named `metadata` which has entries for all the dataElements (SRA
  accessions). Note that not every dataElement from the `mb2_trackDb.txt` file
  has an entry in the metadata: some were poorly annotated.

- The files are large. For testing purposes you can use the script in this dir
  that downsamples these to at most a specified number of dataElements.

- In order to test the checkbox sets, and the paging of the data table, make
  sure to get enough data to populate those.

- The metadata in principle can be hosted anywhere, but CORS restrictions
  currently block all but the local server. So putting the `mb2_metadata.json`
  in the local `apache/htdocs` dir, and assigning
  ```console
  metaDataUrl /mb2_metadata.json\
  ```
  in the "settings" field of the trackDb entry for the mb2 bigComposite track
  works.
