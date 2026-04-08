#!/usr/bin/env python3
"""
doLiftOverTestRobot.py - Runs liftOver test cases from a CSV file and
reports results. Replaces the former doLiftOverTestRobot.csh + Java LiftOverTest.

Usage: Called as part of the weekly build process on hgwdev.
  doLiftOverTestRobot.py [--log logfile] [--tsv testcases.tsv]
Requires environment variables: WEEKLYBLD, BRANCHNN, BUILDMEISTEREMAIL
"""

import argparse
import csv
import os
import sys
import socket
import subprocess
import tempfile


LIFTOVER_BIN = "/cluster/bin/x86_64/liftOver"
DEFAULT_TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "liftOverTestCases.tsv")
INT_COLUMNS = {"origStart", "origEnd", "newStart", "newEnd"}


def get_test_cases(tsv_path):
    """Read test cases from a TSV file and return a list of dicts."""
    cases = []
    with open(tsv_path, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            for col in INT_COLUMNS:
                row[col] = int(row[col])
            cases.append(row)
    return cases


def check_output(testcase, success_file, error_file):
    """Validate liftOver output against expected results. Returns True if OK."""
    if testcase["status"] == "SUCCESS":
        if not os.path.exists(success_file):
            print("%s cannot find or open output file" % testcase["id"])
            return False
        with open(success_file) as f:
            line = f.readline().strip()
        if not line:
            print("%s empty output file" % testcase["id"])
            return False
        parts = line.split("\t")
        chrom = parts[0]
        start = int(parts[1])
        end = int(parts[2])
        if chrom != testcase["newChrom"] or start != testcase["newStart"] or end != testcase["newEnd"]:
            print("%s unexpected coords" % testcase["id"])
            print("%s:%d-%d" % (chrom, start, end))
            return False
        return True

    # Expected failure case
    if not os.path.exists(error_file):
        print("%s cannot find or open error file" % testcase["id"])
        return False
    with open(error_file) as f:
        line = f.readline()
    # Strip leading '#' comment character
    message = line[1:].rstrip("\n")
    if message != testcase["message"]:
        print("%s unexpected error message %s" % (testcase["id"], message))
        return False
    return True


def run_liftover_test(testcase, work_dir, verbose=False):
    """Run a single liftOver test case. Returns True on success."""
    input_file = os.path.join(work_dir, "input")
    success_file = os.path.join(work_dir, "success.out")
    error_file = os.path.join(work_dir, "failure.out")

    # Write input coordinates
    with open(input_file, "w") as f:
        f.write("%s %d %d\n" % (testcase["origChrom"], testcase["origStart"], testcase["origEnd"]))

    # Construct chain file path
    new_assembly = testcase["newAssembly"][0].upper() + testcase["newAssembly"][1:]
    chain = "/gbdb/%s/liftOver/%sTo%s.over.chain.gz" % (
        testcase["origAssembly"], testcase["origAssembly"], new_assembly)

    if not os.path.exists(chain):
        print("Cannot find or open %s" % chain)
        return False

    # Remove old output files if they exist
    for f in (success_file, error_file):
        if os.path.exists(f):
            os.remove(f)

    cmd = [LIFTOVER_BIN, input_file, chain, success_file, error_file]
    if verbose:
        print(" ".join(cmd))

    try:
        subprocess.run(cmd, check=False)
    except Exception as e:
        print(str(e))
        return False

    if not check_output(testcase, success_file, error_file):
        print("Mismatch detected")
        return False

    return True


def main():
    parser = argparse.ArgumentParser(description="Run liftOver QA test cases")
    parser.add_argument("--log", help="Override log file path")
    parser.add_argument("--tsv", default=DEFAULT_TSV, help="Override test cases TSV file")
    args = parser.parse_args()

    # Validate host
    hostname = socket.gethostname()
    if not hostname.startswith("hgwdev"):
        print("error: doLiftOverTestRobot.py must be executed from hgwdev!")
        sys.exit(1)

    # Read required environment variables
    weeklybld = os.environ.get("WEEKLYBLD")
    branchnn = os.environ.get("BRANCHNN")
    buildmeister_email = os.environ.get("BUILDMEISTEREMAIL")
    if not all([weeklybld, branchnn, buildmeister_email]):
        print("error: WEEKLYBLD, BRANCHNN, and BUILDMEISTEREMAIL must be set")
        sys.exit(1)

    log_file = args.log if args.log else os.path.join(weeklybld, "logs", "v%s.LiftOverTest.log" % branchnn)

    # Redirect stdout/stderr to log file
    log_fh = open(log_file, "w")
    old_stdout = sys.stdout
    old_stderr = sys.stderr
    sys.stdout = log_fh
    sys.stderr = log_fh

    try:
        cases = get_test_cases(args.tsv)
        err_count = 0
        with tempfile.TemporaryDirectory() as work_dir:
            for testcase in cases:
                success = run_liftover_test(testcase, work_dir)
                if not success:
                    print("Skipping testcase %s" % testcase["id"])
                    print("----------------------")
                    err_count += 1
        print("Total number of errors = %d" % err_count)
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr
        log_fh.close()

    # Report completion
    msg = "LiftOverTest robot done. Check to see if any errors in %s." % log_file
    print(msg)

    recipients = "%s browser-qa@soe.ucsc.edu" % buildmeister_email
    subject = "v%s LiftOverTest robot done." % branchnn
    subprocess.run(
        "echo %s | mail -s %s %s" % (
            repr(msg), repr(subject), recipients),
        shell=True, check=False)


if __name__ == "__main__":
    main()
