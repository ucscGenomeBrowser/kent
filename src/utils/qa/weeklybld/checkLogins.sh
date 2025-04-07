#!/bin/sh

set -eEu -o pipefail

(ssh qateam@genome-euro true > /dev/null 2>&1) && echo "genome-euro login successful." || echo "genome-euro login failed!"
echo "host hgdownload gives:"
host hgdownload
(ssh qateam@hgdownload1 true > /dev/null 2>&1) && echo "hgdownload1 login successful." || echo "hgdownload1 login failed!"
(ssh qateam@hgdownload2 true > /dev/null 2>&1) && echo "hgdownload2 login successful." || echo "hgdownload2 login failed!"
(ssh qateam@hgdownload3 true > /dev/null 2>&1) && echo "hgdownload3 login successful." || echo "hgdownload3 login failed!"
