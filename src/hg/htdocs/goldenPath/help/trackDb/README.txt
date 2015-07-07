To add a new statement to the trackDb documentation:

1. Add your statement documentation to trackDbLibrary.shtml

2. Add references to your statement to trackDbDoc.html

3. If intended for hubs:
        * Add to the latest trackDbHub doc (e.g. trackDbHub.v1.html), with support level 'new'.
                (see intro of the doc for explanation of support levels).
        * Use hubCheck to verify the doc entry is properly formatted:
            % make
            % hubCheck -settings \
                -version=http://hgwdev-<user>/goldenPath/help/trackDb/trackDbHub.v1.html
          OR
            % make alpha
            % hubCheck -settings -test
          
- See the long comment at the top of trackDbLibrary.shtml for details
