#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os

# excludes directory should contain trailing /
excludes = ["thirdparty/", "build-lint/", "build/", "memory/", "tools/FBE/"]
license_header = "header.txt"

def is_excluded(path) -> bool:
    def is_directory(excluded) -> bool:
        return excluded.endswith('/')

    cwd = os.getcwd()
    for excluded in excludes:
        abs_excluded = os.path.join(cwd, excluded)
        if is_directory(abs_excluded):
            if path.startswith(abs_excluded):
                return True
        else:
            if path == abs_excluded:
                return True
    return False


def main(path: str, dry_run: bool):
    def is_relative(path) -> bool:
        return path.startswith('/') == False

    abs_path = ""
    if is_relative(path):
        cwd = os.getcwd()
        abs_path = os.path.join(cwd, path)
    else:
        abs_path = path
    if os.path.exists(abs_path) == False:
        print(f"file: {abs_path} doesn't exits.")
        return
    if os.path.isfile(abs_path):
        file_license(abs_path, dry_run)
    else:
        # walk directory tree
        for root, dirs, files in os.walk(abs_path):
            for file in files:
                abs_file = os.path.join(root, file)
                if not is_excluded(abs_file):
                    file_license(abs_file, dry_run)

def file_license(file: str, dry_run: bool):
    # do file licensing
    if should_license(file):
        licensing(file, dry_run)

def licensing(file: str, dry_run: bool):
    if dry_run:
        print(f"[dry run] licensing: {file}")
    else:
        position = license_position(file)
        if position == (0, 0):
            insert_license(file)
        else:
            in_place_license(file, position)

# where is license? 
# return open and close range [begin, end]
# if no license return head postion [0, 0]
def license_position(file: str) -> tuple[int, int]:
    def peek_next(content, offset):
        return content[offset+1]
    def is_copy_right(content, offset) -> bool:
        right = 'Copyright'
        length = len(right)
        segment = content[offset:offset+length]
        return segment == right

    (begin, end) = (0, 0)
    content = load_content(file)
    inside_comment_area = False
    has_copy_right = False
    for offset, char in enumerate(content):
        if char == '/':
            next = peek_next(content, offset)
            if next == '*':
                begin = offset
                inside_comment_area = True
        if char == '*':
            next = peek_next(content, offset)
            if next == '/' and has_copy_right:
                end = offset+1
                inside_comment_area = False
                return (begin, end)
        if char == 'C' and inside_comment_area:
            if is_copy_right(content, offset):
                has_copy_right = True
    return (0, 0)

# should has license? 
def should_license(file: str) -> bool:
    license_suffixs = [".cpp", ".hpp", ".cc"]
    for suffix in license_suffixs:
        if file.endswith(suffix):
            return True
    return False

# load license template content
def license_template(strip: bool = True) -> str:
    base_dir = os.path.dirname(os.path.abspath(__file__))
    header = os.path.join(base_dir, license_header)
    return load_content(header, strip)

def load_content(path: str, strip: bool = False) -> str:
    with open(path, 'r') as f:
        if strip:
            return f.read().strip()
        else:
            return f.read()

# in-place license
# license divide into 3 parts.
# leading, middle, trailing
# replace middle part with license header template
def in_place_license(file: str, position: tuple[int, int]):
    print(f"licensing: {file}")
    content = load_content(file)
    length = len(content)
    (begin, end) = position
    if begin > end or begin > length or end > length:
        print(f"begin/end out of content range.")
        return
    leading = "".join(content[0:begin])
    middle = license_template()
    # end + 1 mean license close bucket next char.
    trailing = "".join(content[end+1:])
    new_content = leading + middle + trailing
    save(file, new_content)

# insert license into the begining of license.
def insert_license(file: str):
    content = load_content(file)
    leading = license_template(False)
    new_content = leading + content
    save(file, new_content)

def save(file: str, content: str):
    with open(file, 'w') as f:
        f.write(content)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='licensing stdb project')
    parser.add_argument('-f', '--file', type=str, required=True, help='license target file, support directory')
    parser.add_argument('-n', '--dry_run', help='check license only', action="store_true")
    args = parser.parse_args()
    main(args.file, args.dry_run)
