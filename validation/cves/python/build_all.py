import os
import subprocess
from multiprocessing import Process, Pool


def build_cve(cve_program, cve_target):
    print(f'Building CVE for {cve_program}/{cve_target} ...')
    install_script = os.path.join(cve_dir, cve_target, "install.sh")

    # change working directory
    os.chdir(os.path.join(cve_dir, cve_target))

    print(install_script)

    # check if install script exists
    if not os.path.isfile(install_script):
        raise ModuleNotFoundError(f'CVE {cve_program}/{cve_target}/install.sh NOT EXISTS')

    # run install.sh
    subprocess.call([install_script], shell=True)

    print(f'Building CVE for {cve_program}/{cve_target} COMPLETED')


if __name__ == "__main__":
    cve_dir = os.path.dirname(os.path.abspath(__file__))
    cve_program = os.path.basename(cve_dir)

    # get cve_targets from current directory
    cve_targets = []
    for cve_target in os.listdir(cve_dir):
        if os.path.isdir(os.path.join(cve_dir, cve_target)):
            cve_targets.append(cve_target)

    # multi processing for building
    # limit the pool size as 4
    cpu = 4
    pool = Pool(cpu)

    args_list = []
    for cve_target in cve_targets:
        args = (cve_program, cve_target,)
        args_list.append(args)
    print(args_list)

    # create pool of process
    pool.starmap(build_cve, args_list)
