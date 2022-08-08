#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2021 hurricane <l@stdb.io>
#


"""

"""

import os
import multiprocessing
import queue
import re
import subprocess
import sys
import threading
import tempfile
import shutil


#test_dirs = ["tests", "test_benchmarks","example"]
test_dirs = ["tests"]


def make_absolute(f, directory):
    if os.path.isabs(f):
        return f
    return os.path.normpath(os.path.join(directory, f))


def get_files(path):
    result = []
    for root, sub,  files in os.walk(path):
        for item in files:
            if is_target_file(item):
                fname = make_absolute(item, root)
                result.append(fname)
                #fname = os.path.join(root, item)
    return result


def get_all_files(dirs):
    all_src_files = []
    for src in dirs:
        files = get_files(src)
        all_src_files += files
    return all_src_files


def is_target_file(file):
    return file.endswith(".cc") or file.endswith(".hpp")


def get_last_pr_hash():
    # github will generate "Merge pull request" commit when merge pr
    filter = "Merge pull request"
    command = ['git', 'log', '--all', '--grep={0}'.format(filter)]
    output = subprocess.check_output(command)
    # last pr commit will appear in the first line
    last_pr_commit = output.decode('utf-8').split('\n')[0]
    # commit hash in the second field
    commit_hash = last_pr_commit.strip().split(' ')[1]
    print("[INFO] last pr hash: %s" % (commit_hash))
    return commit_hash


""" only lint ADD/MODIFIED files"""


def file_changed_set():
    changed_files = []
    last_pr_commit = get_last_pr_hash()
    if last_pr_commit is None or last_pr_commit == "":
        return changed_files
    command = ['git', 'diff', '--name-only', last_pr_commit]
    output = subprocess.check_output(command)
    for line in output.decode('utf-8').split('\n'):
        file = line.strip()
        if is_target_file(file) and not deleted(file):
            changed_files.append(file)
    return changed_files


""" file is deleted when not exist in disk"""


def deleted(file):
    if os.path.isfile(file) and os.path.exists(file):
        return False
    return True


def get_tidy_invocation(f, clang_tidy_binary, checks, tmpdir, build_path,
                        header_filter, allow_enabling_alpha_checkers,
                        extra_arg, extra_arg_before, quiet, config):
    """Gets a command line for clang-tidy."""
    start = [clang_tidy_binary, '--use-color']
    if allow_enabling_alpha_checkers:
        start.append('-allow-enabling-analyzer-alpha-checkers')
    if header_filter is not None:
        start.append('-header-filter=' + header_filter)
    if checks:
        start.append('-checks=' + checks)
    if tmpdir is not None:
        start.append('-export-fixes')
        # Get a temporary file. We immediately close the handle so clang-tidy can
        # overwrite it.
        (handle, name) = tempfile.mkstemp(suffix='.yaml', dir=tmpdir)
        os.close(handle)
        start.append(name)
    for arg in extra_arg:
        start.append('-extra-arg=%s' % arg)
    for arg in extra_arg_before:
        start.append('-extra-arg-before=%s' % arg)
    start.append('-p=' + build_path)
    if quiet:
        start.append('-quiet')
    if config:
        start.append('--config-file=' + config)
    start.append(f)
    return start


def run_tidy(tmpdir, build_path, queue, lock, failed_files):
    """Takes filenames out of queue and runs clang-tidy on them."""
    while True:
        (name, args) = queue.get()
        invocation = get_tidy_invocation(name, args["clang_tidy_binary"], args["checks"],
                                         tmpdir, build_path, args["header_filter"],
                                         args["allow_enabling_alpha_checkers"],
                                         args["extra_arg"], args["extra_arg_before"],
                                         args["quiet"], args["config"])

        proc = subprocess.Popen(
            invocation, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output, err = proc.communicate()
        if proc.returncode != 0:
            print("clang-tidy geterror!\n")
            failed_files.append(name)
        with lock:
            sys.stdout.write(' '.join(invocation) +
                             '\n' + output.decode('utf-8'))
            if len(err) > 0:
                sys.stdout.flush()
                sys.stderr.write(err.decode('utf-8'))
        queue.task_done()


def prepare_args():
    args = {}
    args["allow_enabling_alpha_checkers"] = False
    args["clang_tidy_binary"] = "clang-tidy"
    args["checks"] = None
    #args["config"] = ".clang-tidy"
    args["config"] = None
    args["header_filter"] = None
    args["quiet"] = False
    args["extra_arg"] = ['-Wno-unknown-warning-option']
    args["extra_arg_before"] = []
    return args


def prepare_test_args():
    args = {}
    args["allow_enabling_alpha_checkers"] = False
    args["clang_tidy_binary"] = "clang-tidy"
    args["checks"] = None
    #args["config"] = ".clang-tidy-test"
    args["config"] = None
    args["header_filter"] = None
    args["quiet"] = False
    args["extra_arg"] = ['-Wno-unknown-warning-option']
    args["extra_arg_before"] = []
    return args


def parallel_run(jobs):
    max_task = multiprocessing.cpu_count()
    tmpdir = tempfile.mkdtemp()
    build_path = "./"
    #args = prepare_args()
    return_code = 0
    try:
        task_queue = queue.Queue(max_task)
        failed_files = []
        lock = threading.Lock()

        for _ in range(max_task):
            t = threading.Thread(target=run_tidy,
                                 args=(tmpdir, build_path, task_queue, lock, failed_files))
            t.daemon = True
            t.start()

        # Fill the queue with job (file name, args).
        for job in jobs:
            task_queue.put(job)

        # Wait for all threads to be done.
        task_queue.join()
        if len(failed_files):
            return_code = 1

    except KeyboardInterrupt:
        # This is a sad hack. Unfortunately subprocess goes
        # bonkers with ctrl-c and we start forking merrily.
        print('\nCtrl-C detected, goodbye.')
        if tmpdir:
            shutil.rmtree(tmpdir)
        os.kill(0, 9)

    if tmpdir:
        shutil.rmtree(tmpdir)
    if return_code != 0:
        sys.exit(return_code)


def construct_jobs(files):
    dirs = lookup_dirs()
    jobs = []
    for file in files:
        components = file.split('/')
        leading = components[0]
        # leading part must be directory
        if os.path.isfile(leading):
            continue
        if leading in dirs:
            jobs.append((file, prepare_args()))
        if leading in test_dirs:
            jobs.append((file, prepare_test_args()))
    return jobs


def lookup_dirs():
    def is_excluded(file):
        exclude_patterns = [
            r"^\..*", "build.*", "cmake", "doctest"
        ]
        for pattern in exclude_patterns:
            regexp = re.compile(pattern)
            if regexp.search(file):
                return True
        return False

    files = os.listdir()
    dirs = []

    for file in files:
        if os.path.isfile(file):
            continue
        if is_excluded(file):
            continue

        dirs.append(file)
    return dirs


def delta_lint(exclude_tests=False):
    lint_files = file_changed_set()
    if(exclude_tests):
        lint_files = list(
            filter(lambda x: (not str.startswith(x, "test")), lint_files))
    print("[INFO] lint files: %s" % (lint_files))
    jobs = construct_jobs(lint_files)
    parallel_run(jobs)


def all_files():
    dirs = lookup_dirs()
    src_files = get_all_files(dirs)
    test_files = get_all_files(test_dirs)
    full_files = src_files + test_files
    return full_files


def full_lint():
    fullfiles = all_files()
    jobs = construct_jobs(fullfiles)
    parallel_run(jobs)


def filter_lint(filter_str):
    print("[INFO] filter with: %s" % filter_str)
    print("all files to lint:\n")
    fullfiles = all_files()
    regexp = re.compile(filter_str)
    lint_files = []
    for f in fullfiles:
        if regexp.search(f):
            lint_files.append(f)
            print(f)
    jobs = construct_jobs(lint_files)
    parallel_run(jobs)


def main(mode):
    if mode == "delta":
        delta_lint()
    elif mode == "delta-exclude-tests":
        delta_lint(True)
    elif mode == "full":
        full_lint()
    else:
        raise Exception("wrong mode")
        sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "full":
            main("full")
        if sys.argv[1] == "delta":
            main("delta")
        if sys.argv[1] == "delta-exclude-tests":  # exclude test code
            main("delta-exclude-tests")
        else:
            filter_lint(sys.argv[1])
    else:
        main("delta")
