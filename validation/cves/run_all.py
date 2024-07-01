import os
import subprocess
import csv

if __name__ == "__main__":
    cve_root = os.path.dirname(os.path.abspath(__file__))

    # define bench target
    BENCH_TARGETS=["glibc", "BUDAlloc", "ffmalloc"]

    # get list of cve programs
    # ex. mruby, php, python, uafbench, exploit_database
    cve_programs = []
    for cve_program in os.listdir(cve_root):
        if os.path.isdir(os.path.join(cve_root, cve_program)):
            cve_programs.append(cve_program)

    # get cve target paths
    # ex. blahblah../mruby/cve2020-6383, blahblah../mruby/issue3515
    cve_targets_path = []
    cve_targets_name = []
    for cve_program in cve_programs:
        for cve_target in os.listdir(os.path.join(cve_root, cve_program)):
            if os.path.isdir(os.path.join(cve_root, cve_program, cve_target)):
                cve_targets_path.append(os.path.join(cve_root, cve_program, cve_target))
                cve_targets_name.append(os.path.join(cve_program, cve_target))

    # print(cve_targets_path)

    # create result.csv file\
    # if already exists, then remove sand
    output_dir = os.path.join(cve_root, 'results')
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if os.path.isfile(os.path.join(output_dir, "results.csv")):
        os.remove(os.path.join(output_dir, "results.csv"))
    
    with open(os.path.join(output_dir, "results.csv"), "w") as results_csv:
        results_writer = csv.writer(results_csv)

        # col name
        results_top_row = []
        results_top_row.append("CVE")

        for BENCH_TARGET in BENCH_TARGETS:
            results_top_row.append(BENCH_TARGET)

        # write the top row
        results_writer.writerow(results_top_row)

        for cve_target_path, cve_target_name in zip(cve_targets_path, cve_targets_name):
            
            run_script = os.path.join(cve_target_path, "run_poc.sh")
            # change working directory
            os.chdir(cve_target_path)

            print(f'Running {run_script} ...')

            # check if run_poc.sh exists
            if not os.path.isfile(run_script):
                raise ModuleNotFoundError(f'CVE: {cve_target_path}/run_poc.sh NOT FOUND!')
            
            results_row = []            

            results_row.append(cve_target_name)

            # run run_poc.sh for glibc, mbpf, ffmalloc
            for BENCH_TARGET in BENCH_TARGETS:
                # before running, clear the dmesg part
                subprocess.run([f'sudo dmesg -C'], shell=True)
                
                print(f'./run_poc.sh {BENCH_TARGET}')
                result = subprocess.check_output([f'{run_script} {BENCH_TARGET}'], shell=True)
                output = result.rstrip().decode()
                results_row.append(output)

            results_writer.writerow(results_row)
                
            print(f'Running {run_script} COMPLETED')