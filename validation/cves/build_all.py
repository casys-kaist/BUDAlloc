import os
import subprocess
from multiprocessing import Process, Pool
import time

if __name__ == "__main__":
    print(f'Building CVE STARTED ...')
    start_time = time.time()
    cve_root = os.path.dirname(os.path.abspath(__file__))

    # get cve_targets from current directory
    cve_programs = []
    for cve_program in os.listdir(cve_root):
        if cve_program == "results":
            continue
        if os.path.isdir(os.path.join(cve_root, cve_program)):
            cve_programs.append(cve_program)

    for cve_program in cve_programs:
        install_script = os.path.join(cve_root, cve_program, "build_all.py")

        #change working directory
        os.chdir(os.path.join(cve_root, cve_program))

        if not os.path.isfile(install_script):
            raise ModuleNotFoundError(f'CVE {install_script} NOT EXISTS')
        subprocess.call(["python3 ./build_all.py"], shell=True)

    print(f'{time.time() - start_time} seconds taken...')
    print(f'Building CVE COMPLETED')

