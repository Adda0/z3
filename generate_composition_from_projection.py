#!/usr/bin/env python3


import sys
import os
import glob
import itertools
import subprocess
from copy import deepcopy


def main(argv):
    for file_name in os.listdir(argv[1]):
        finished_files = []

        file = os.path.join(argv[1], file_name)
        # checking if it is a file
        if os.path.isfile(file) and file not in finished_files:
            last_underscore_symbol_index = file.rfind("_")
            benchmark_instance_name = file[:last_underscore_symbol_index]
            print(benchmark_instance_name)

            files_in_benchmark_instance = glob.glob(f"{benchmark_instance_name}_*")
            finished_files.extend(files_in_benchmark_instance)
            print(len(files_in_benchmark_instance))

            for permutation in itertools.permutations(files_in_benchmark_instance, r=2):
                last_underscore_symbol_index = permutation[0].rfind("_")
                last_dot_symbol_index = permutation[0].rfind(".")
                base_name = permutation[0][:last_underscore_symbol_index]
                base_name = base_name[base_name.find('non-incremental'):]
                first_number = permutation[0][last_underscore_symbol_index + 1 : last_dot_symbol_index]
                last_underscore_symbol_index = permutation[1].rfind("_")
                last_dot_symbol_index = permutation[1].rfind(".")
                second_number = permutation[1][last_underscore_symbol_index + 1 : last_dot_symbol_index]


                target = f"../generated_benchmarks/composition/pairs/{base_name}_{first_number}_{second_number}.mata"
                subprocess.run(["cp", f"{permutation[0]}", f"{target}"])
                with open(target, "w") as outfile:
                    subprocess.run(['cat', permutation[1]], stdout=outfile)



    print(finished_files)
    print(len(finished_files))


if __name__ == "__main__":
    main(sys.argv)
