#!/usr/bin/env python3

# Program Header
# Name:   Gerardo Perez
# Description: A program that compares hg38, hg19, mm39, and mm10 tables and metadata tables for hgw1 and hgwbeta discrepancies 
# check_Tables_Metadata.py
#
# Development Environment: VIM - Vi IMproved version 7.4.629

import subprocess

def bash(cmd):
    """Executes a shell command in Bash and returns the output as a list of lines."""
    rawOutput = subprocess.run(cmd, check=True, shell=True, stdout=subprocess.PIPE, universal_newlines=True)
    return rawOutput.stdout.split('\n')[0:-1]


def run_command(command):
    """Executes a shell command and returns its output as a string. Prints an error message if the command fails."""
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print("Error executing: {}".format(command))
        print("Error message: {}".format(result.stderr.strip()))
    return result.stdout.strip() # Remove leading/trailing whitespace.


def get_data(host, query, database):
    """Gets and sorts data from a database using a SQL query."""
    command = '{} -Ne "{}" {} 2>/dev/null | sort'.format(host, query, database)
    return run_command(command).splitlines()


def filter_blat_servers(data):
    """Splits blat*.soe.ucsc.edu after 'blat' and before '.soe.ucsc.edu', then re-formats for comparison."""
    filtered_entries = []
    for entry in data:
        fields = entry.split()
        if len(fields) > 2:
            server_name = fields[1]
            port = fields[2]
            if server_name.startswith("blat") and ".soe.ucsc.edu" in server_name:
                base_name = server_name.split(".soe.ucsc.edu")[0]  # Remove domain part
                new_entry = "{}\t{}\t{}\t{}".format(fields[0], base_name, port, "\t".join(fields[3:]) if len(fields) > 3 else "")
                filtered_entries.append(new_entry)
    return filtered_entries


def compare_table_lists(beta_tables, hgw1_tables, db):
    """Compares table lists from two database sources and prints differences.
    
    Tables that exist only in one of the sources are displayed.
    """
    ignored_beta_tables = {"hgFindSpec_public", "trackDb_public", "relatedTrack_public", "metaDb_public", "metaDb_back", "metaDb_public_back"}

    # Filter out ignored tables from beta_tables
    filtered_beta_tables = []
    for table in beta_tables:
        if table not in ignored_beta_tables:
            filtered_beta_tables.append(table)

    # Convert lists to sets for comparison
    set1, set2 = set(filtered_beta_tables), set(hgw1_tables)
    only_in_beta_tables = set1 - set2
    only_in_hgw1_tables = set2 - set1

    # Print differences only if there are discrepancies
    if only_in_beta_tables or only_in_hgw1_tables:
        print("\nDiscrepancies between hgw1 and beta tables for Database: {}".format(db))

    if only_in_beta_tables:
        print("Tables only in hgwbeta:")
        for table in sorted(only_in_beta_tables):
            print(table)
        print()

    if only_in_hgw1_tables:
        print("Tables only in hgw1:")
        for table in sorted(only_in_hgw1_tables):
            print(table)
        print()


def compare_metatables(db_list, metatables, host1, host2, centdb1, centdb2):
    """Compares metadata tables across two databases in db_list."""
    for db in db_list:
        for metatable in metatables:
            # Construct different SQL queries depending on the metatable type
            if metatable == "liftOverChain":
                query = "SELECT * FROM {} WHERE fromDb = '{}' OR toDb = '{}'".format(metatable, db, db)
            elif metatable == "blatServers":
                query = "SELECT * FROM {} WHERE db = '{}'".format(metatable, db)
            else:
                query = "SELECT * FROM {} WHERE name = '{}'".format(metatable, db)

            # Gets and sort data from both databases
            data1 = get_data(host1, query, centdb1)
            data2 = get_data(host2, query, centdb2)

            # Apply filtering to blatServers to remove domain part
            '''Commenting out these lines due to Hiram updating the blatServers.hgcentral table on
             hgnfs1 for all host names'''
            #if metatable == "blatServers":
                #data1 = filter_blat_servers(data1)
            
            # Convert lists to sets for comparison
            set1 = set(data1)
            set2 = set(data2)

            # Identify unique entries in each database
            only_in_centdb1 = set1 - set2
            only_in_centdb2 = set2 - set1

            # Print differences if any unique entries exist
            if only_in_centdb1:
                print("Discrepancies between hgw1 and beta metadata tables for Database: {}".format(db))
                print("{} entries only in {}:".format(metatable, centdb1))
                for line in sorted(only_in_centdb1):
                    print(line)
                print()  # Add a newline for better readability

            if only_in_centdb2:
                print("Discrepancies between hgw1 and beta metadata tables for Database: {}".format(db))
                print("{} entries only in {}:".format(metatable, centdb2))
                for line in sorted(only_in_centdb2):
                    print(line)
                print()  # Add a newline for better readability


def main():
    """Main function to compare database tables and metadata."""
    db_list = ["hg38", "hg19", "mm39", "mm10"]
    centdb1, centdb2 = "hgcentralbeta", "hgcentral"
    host1 = "/cluster/bin/x86_64/hgsql -h hgwbeta"
    host2 = "/cluster/bin/x86_64/hgsql -h genome-centdb"
    metatables = ["liftOverChain", "blatServers", "dbDb"]

    # Compare table lists for each database in db_list
    for db in db_list:
        beta_tables = bash("/cluster/bin/x86_64/hgsql -h hgwbeta -Ne 'show tables' {}".format(db))
        hgw1_tables = bash("ssh qateam@hgw1 'HGDB_CONF=`pwd`/.hg.local.conf /home/qateam/bin/x86_64/hgsql -N -e \"show tables\" \"{}\"'".format(db))
        compare_table_lists(beta_tables, hgw1_tables, db)

    # Compare metadata tables
    compare_metatables(db_list, metatables, host1, host2, centdb1, centdb2)


if __name__ == "__main__":
    main()

# Program Output (Commented out)
#Discrepancies between hgw1 and beta tables for Database: rn6
#Tables only in hgwbeta:
#refSeqSummary
#
#Discrepancies between hgw1 and beta metadata tables for Database: hg38
#blatServers entries only in hgcentralbeta:
#hg38    blat1c  17780   1       0       0
#hg38    blat1c  17781   0       1       0
