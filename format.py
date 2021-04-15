import os
import subprocess


def clang_format(root, exts, excludes):
    def is_ignored(path):
        for prefix in excludes:
            if path.startswith(prefix):
                return True

    def is_source(source):
        for ext in exts:
            if source.endswith(ext):
                return True

    for root, _, files in os.walk('.'):
        if is_ignored(root):
            continue

        for source in files:
            if not is_source(source):
                continue
            subprocess.call(['clang-format', '-i', os.path.join(root, source)])


def git_submodule_pathes():
    modulePaths = []
    with open("./.gitmodules") as f:
        for line in f.readlines():
            ws = line.split('path = ')
            if len(ws) == 2:
                modulePaths.append(ws[1].strip('\n'))
    return modulePaths


def main():
    exts = ['cc', 'hpp']
    excludes = [
        './.',
        './build',
    ]
    [excludes.append('./'+p) for p in git_submodule_pathes()]
    clang_format('.', exts, excludes)


if __name__ == '__main__':
    main()
