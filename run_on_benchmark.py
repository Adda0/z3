#!/usr/bin/env python

import subprocess
import argparse
import sys
import os


autoresolved_instances = []


def walk(path):
    if os.path.isdir(path):
        for current_path, _, files in os.walk(path):
            for file in files:
                file_path = os.path.join(current_path, file)
                print(f"Running on: {file_path}")
                res = subprocess.run(["./run_z3_noodler.sh", "r", f"{file_path}"])
                if res.returncode != 42:
                    autoresolved_instances.append(file_path)
    elif os.path.isfile(path):
        print(f"Running on: {path}")
        res = subprocess.run(["./run_z3_noodler.sh", "r", f"{path}"])
        if res.returncode != 42:
            autoresolved_instances.append(path)
    else:
        raise ValueError(f"Invalid path {path}.")


def main():
    parser = argparse.ArgumentParser(
        prog='Run on benchmark',
        description='Run Z3-Noodler on a benchmark'
    )
    parser.add_argument('path', action="store", nargs="+", metavar="PATH",
                        help="Path to the files or directories to run Z3-Noodler on.")

    args = parser.parse_args(sys.argv[1:])

    for path in args.path:
        walk(path)

    print(autoresolved_instances)


if __name__ == "__main__":
    main()
