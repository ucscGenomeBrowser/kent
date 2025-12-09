#!/usr/bin/env python3

import subprocess
import argparse

# Run a command on a remote host via SSH
def get_checksums_via_ssh(host, command):
    """Executes a checksum command on a remote host via SSH."""
    result = subprocess.run(
        ["ssh", "qateam@" + host, command],
        capture_output=True, text=True
    )
    return result.stdout

# Parse md5sum output into a dict. Example line: "d41d8cd98f00b204e9800998ecf8427e  /path/to/file.html"
# Produces: { "/path/to/file.html": "d41d8cd..." }
def parse_checksums(output):
    """Parses md5sum output into a dictionary mapping filename -> checksum."""
    checksums = {}
    for line in output.splitlines():
        parts = line.split(None, 1)
        if len(parts) == 2:
            md5sum, file = parts
            checksums[file] = md5sum
    return checksums

# Format output as a URL (optional - controlled by --links)
# hgwdev uses ".gi.ucsc.edu". Others use ".soe.ucsc.edu"
def format_file_link(host, file, use_links):
    """Formats the file as a clickable URL when --links is enabled."""
    if use_links:
        if host == "hgwdev":
            domain = "hgwdev.gi.ucsc.edu"
        else:
            domain = host + ".soe.ucsc.edu"
        return "https://" + domain + file.replace("/usr/local/apache/htdocs", "")
    return file

# Ignore unwanted files and directories
def should_ignore(file, ignore_list):
    """Returns True if the file path should be ignored."""
    return any(term in file for term in ignore_list)


# MAIN PROGRAM
def main():

    # Argument Parsing
    parser = argparse.ArgumentParser(
        description="Compare static HTML docs across hgwdev, hgwbeta, and hgw0."
    )
    parser.add_argument(
        "-l", "--links", action="store_true",
        help="Format output as clickable https:// URLs"
    )
    args = parser.parse_args()
    
    
    
    # Files/directories to ignore during comparison
    ignore_files = [
        "/index.html",
        "admin/",
        "thumbNailLinks.html",
        "gbPageStartHardcoded.html",
        "ENCODE/",
        "/docs/",
        "allTipsRaw.html",
        "/mirrorDocs/staticPage.html",
        "googleAnalytics.html",
        "google09a546cf57abf548.html",
        "googlef42dd384148a3435.html",
        "/tipOfDay.html",
    ]
    
    # Commands to list HTML files and calculate md5sum
    # grep -rl + xargs md5sum is used because find does not work on hgwbeta
    command = (
        "grep -rl --include='*.html' '' /usr/local/apache/htdocs 2>/dev/null | "
        "xargs -d '\n' md5sum | sort -k 2"
    )

    # Fetch checksums hgwdev, hgwbeta, and hgw0
    checksums_hgwdev  = parse_checksums(get_checksums_via_ssh('hgwdev', command))
    checksums_hgwbeta = parse_checksums(get_checksums_via_ssh('hgwbeta', command))
    checksums_hgw0    = parse_checksums(get_checksums_via_ssh('hgw0', command))
    
    # 1. Missing on hgwdev but present on hgwbeta
    missing_from_hgwdev = [
        file for file in checksums_hgwbeta.keys() - checksums_hgwdev.keys()
        if not should_ignore(file, ignore_files)
    ]

    if missing_from_hgwdev:
        print("Files present on hgwbeta but missing from hgwdev:")
        for file in missing_from_hgwdev:
            print(format_file_link("hgwbeta", file, args.links))
        print()

    # 2. Files on both hgwdev & hgwbeta but with different md5sum
    diff_hgwbeta_hgwdev = [
        file for file in (checksums_hgwdev.keys() & checksums_hgwbeta.keys())
        if checksums_hgwdev[file] != checksums_hgwbeta[file]
        and not should_ignore(file, ignore_files)
    ]

    if diff_hgwbeta_hgwdev:
        print("Files present on both hgwbeta and hgwdev but different md5sum:")
        for file in diff_hgwbeta_hgwdev:
            print(format_file_link("hgwbeta", file, args.links), checksums_hgwbeta[file])
            print(format_file_link("hgwdev",  file, args.links), checksums_hgwdev[file], "\n")
        print()

    # 3. Present only on hgwbeta (missing on hgw0)
    missing_from_hgw0 = [
        file for file in checksums_hgwbeta.keys() - checksums_hgw0.keys()
        if not should_ignore(file, ignore_files)
    ]

    if missing_from_hgw0:
        print("Files present only on hgwbeta and missing on hgw0:")
        for file in missing_from_hgw0:
            print(format_file_link("hgwbeta", file, args.links))
        print()

    # 4. Present only on hgw0 (missing on hgwbeta)
    missing_from_hgwbeta = [
        file for file in checksums_hgw0.keys() - checksums_hgwbeta.keys()
        if not should_ignore(file, ignore_files)
    ]

    if missing_from_hgwbeta:
        print("Files present only on hgw0 and missing on hgwbeta:")
        for file in missing_from_hgwbeta:
            print(format_file_link("hgw0", file, args.links))
        print()

    # 5. Different md5sum between hgwbeta and hgw0
    diff_hgw0_hgwbeta = [
        file for file in (checksums_hgw0.keys() & checksums_hgwbeta.keys())
        if checksums_hgw0[file] != checksums_hgwbeta[file]
        and not should_ignore(file, ignore_files)
    ]

    if diff_hgw0_hgwbeta:
        print("Files present on both hgw0 and hgwbeta but different md5sum:")
        for file in diff_hgw0_hgwbeta:
            print(format_file_link("hgwbeta", file, args.links), checksums_hgwbeta[file])
            print(format_file_link("hgw0",    file, args.links), checksums_hgw0[file], "\n")
        print()

# Run the program
if __name__ == "__main__":
    main()

# Program Output (Commented out)
#Files present on hgwbeta but missing from hgwdev:
#/usr/local/apache/htdocs/admin/maintenance.html
#/usr/local/apache/htdocs/ancestors/index.html
#
#Files present on both hgwbeta and hgwdev but different md5sum:
#/usr/local/apache/htdocs/docs/index.html 9c1340981564d95cabb906eb15415476
#/usr/local/apache/htdocs/docs/index.html 234def442a25eead78ccf7caf09cce4c
#
#/usr/local/apache/htdocs/docs/tableBrowserTutorial.html c6f36e979edc4912c1d4a80f0aa6eba4
#/usr/local/apache/htdocs/docs/tableBrowserTutorial.html 5c08659421aea9dc1408b0ac7cc266f3
#
#Files present only on hgwbeta and missing on hgw0:
#/usr/local/apache/htdocs/admin/maintenance.html
#/usr/local/apache/htdocs/ENCODE/controlledVocabulary.html
#
#Files present only on hgw0 and missing on hgwbeta:
#/usr/local/apache/htdocs/admin/stats/Report.html
#
#Files present on both hgw0 and hgwbeta but different md5sum:
#/usr/local/apache/htdocs/admin/jk-install.html 9a561100775c6c730342e2d94a02fa08
#/usr/local/apache/htdocs/admin/jk-install.html c97be8e421ce0ad9159e87cda40345d4
#
#/usr/local/apache/htdocs/admin/404.html 95c7561df8631b27f9fa1e76d75e7270
#/usr/local/apache/htdocs/admin/404.html 3a1049daecd22bff590ebf0f2e6c2602
