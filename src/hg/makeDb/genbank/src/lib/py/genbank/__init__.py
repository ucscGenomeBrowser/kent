import sys

# require 2.4 or newer
if (sys.version_info[0] <= 2) and (sys.version_info[1] < 4):
    raise Exception("python 2.4 or newer required, using " + sys.version)
