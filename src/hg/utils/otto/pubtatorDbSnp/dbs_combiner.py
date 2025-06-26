import logging
import argparse
import sys
import pathlib

from sqlitedict import SqliteDict


def combine_cache_dbs_program() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--combined-cache-db", type=str, required=True)
    parser.add_argument("--new-cache-dbs-dir", type=str, required=True)
    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)

    with SqliteDict(args.combined_cache_db) as combined_cache_db:
        new_cache_dbs = list(pathlib.Path(args.new_cache_dbs_dir).glob("*.sqlite*"))
        for new_cache_db in new_cache_dbs:
            logging.info("Processing %s", new_cache_db)
            with SqliteDict(new_cache_db) as new_db:
                for key, value in new_db.items():
                    combined_cache_db[key] = value
            combined_cache_db.commit()


if __name__ == "__main__":
    combine_cache_dbs_program()
