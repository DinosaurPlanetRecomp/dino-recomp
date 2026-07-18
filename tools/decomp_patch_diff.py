#!/usr/bin/env python3
from __future__ import annotations
import argparse
import os
from pathlib import Path
from typing import TextIO
import re

SCRIPT_DIR = Path(os.path.dirname(os.path.realpath(__file__)))
PROJECT_DIR = SCRIPT_DIR.parent

PATCH_SRC_DIR = PROJECT_DIR.joinpath("patches/src")
DECOMP_SRC_DIR = PROJECT_DIR.joinpath("lib/dino-recomp-decomp-bridge/dinosaur-planet/src")

func_start_regex = re.compile(r"\s*(\w*[ *\n]+?(\w+)\s*\([\w *,\[\]]*\)\s*{)")
patch_func_start_regex = re.compile(r"\s*RECOMP_PATCH\s+(\w*[ *\n]+?(\w+)\s*\([\w *,\[\]]*\)\s*{)")

def find_end_brace(contents: str, offset: int):
    depth = 1
    while depth > 0 and offset < len(contents):
        c = contents[offset]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        offset += 1

    return offset

def get_funcs_from_c(file: TextIO, funcs: dict[str, str], pattern: re.Pattern[str]):
    found_funcs: list[str] = []
    contents = file.read()

    start = 0
    while start < len(contents):
        func_start = pattern.match(contents, pos=start)
        if func_start == None:
            start += 1
            continue

        func_name = func_start.group(2)
        end = find_end_brace(contents, func_start.end(1))

        func_contents = contents[func_start.start(1):end]
        start = end

        funcs[func_name] = func_contents
        found_funcs.append(func_name)
    
    return found_funcs

def output_single_diff_files(output_dir: Path, patch_funcs: dict[str, str], decomp_funcs: dict[str, str]):
    with open(output_dir.joinpath("patches.c"), "w", encoding="utf-8") as patches_file, \
         open(output_dir.joinpath("decomp.c"), "w", encoding="utf-8") as decomp_file:
        for func_name, patch_func_contents in patch_funcs.items():
            decomp_func_contents = decomp_funcs.get(func_name)
            if decomp_func_contents == None:
                print(f"WARN: Failed to find '{func_name}' in decomp")
                decomp_func_contents = ""

            patches_file.write(f"// ------ {func_name} ------------------------\n")
            patches_file.write(patch_func_contents)
            patches_file.write("\n")
            patches_file.write(f"// -------------------------------------------\n\n")

            decomp_file.write(f"// ------ {func_name} ------------------------\n")
            decomp_file.write(decomp_func_contents)
            decomp_file.write("\n")
            decomp_file.write(f"// -------------------------------------------\n\n")

def output_diff_dirs(output_dir: Path, patch_funcs: dict[str, str], decomp_funcs: dict[str, str], decomp_func_files: dict[str, Path]):
    patches_dir = output_dir.joinpath("patches")
    patches_dir.mkdir(parents=True, exist_ok=True)
    decomp_dir = output_dir.joinpath("decomp")
    decomp_dir.mkdir(parents=True, exist_ok=True)

    written_files: set[Path] = set()

    for func_name, patch_func_contents in patch_funcs.items():
        decomp_func_contents = decomp_funcs.get(func_name)
        if decomp_func_contents == None:
            print(f"WARN: Failed to find '{func_name}' in decomp")
            continue

        relative_file_path = decomp_func_files[func_name]
        
        patches_file_path = patches_dir.joinpath(relative_file_path)
        patches_file_path.parent.mkdir(parents=True, exist_ok=True)
        decomp_file_path = decomp_dir.joinpath(relative_file_path)
        decomp_file_path.parent.mkdir(parents=True, exist_ok=True)

        with open(patches_file_path, "a" if patches_file_path in written_files else "w", encoding="utf-8") as patches_file:
            patches_file.write(f"// ------ {func_name} ------------------------\n")
            patches_file.write(patch_func_contents)
            patches_file.write("\n")
            patches_file.write(f"// -------------------------------------------\n\n")
        
        with open(decomp_file_path, "a" if decomp_file_path in written_files else "w", encoding="utf-8") as decomp_file:
            decomp_file.write(f"// ------ {func_name} ------------------------\n")
            decomp_file.write(decomp_func_contents)
            decomp_file.write("\n")
            decomp_file.write(f"// -------------------------------------------\n\n")
        
        written_files.add(patches_file_path)
        written_files.add(decomp_file_path)

def main():
    parser = argparse.ArgumentParser(description="Sets up files containing the decomp and patched version of each patched function, such that the differences can be compared with a diff program.")
    parser.add_argument("-o", "--output", type=str, help="Output directory. Must not already exist.", required=True)
    parser.add_argument("--patches", type=str, help="Patches src directory.", default=PATCH_SRC_DIR.as_posix())
    parser.add_argument("--decomp", type=str, help="Decomp src directory.", default=DECOMP_SRC_DIR.as_posix())
    parser.add_argument("--flatten", action="store_true", help="Flatten sources into a single file for the patches and decomp.", default=False)
    parser.add_argument("-f", "--force", action="store_true", help="Write files even if the output directory already exists.", default=False)
    
    args = parser.parse_args()

    output_dir = Path(args.output)
    patches_src_dir = Path(args.patches)
    decomp_src_dir = Path(args.decomp)

    if output_dir.exists() and not args.force:
        print(f"ERR: Output directory already exists at: {output_dir.absolute()}")
        exit(1)
        return

    patch_funcs: dict[str, str] = {}
    for path in patches_src_dir.glob("**/*.c"):
        with open(path, "r", encoding="utf-8") as file:
            get_funcs_from_c(file, patch_funcs, patch_func_start_regex)
    
    decomp_funcs: dict[str, str] = {}
    decomp_func_files: dict[str, Path] = {}
    for path in decomp_src_dir.glob("**/*.c"):
        with open(path, "r", encoding="utf-8") as file:
            in_file = get_funcs_from_c(file, decomp_funcs, func_start_regex)
            relative_path = path.relative_to(decomp_src_dir)
            for func_name in in_file:
                decomp_func_files[func_name] = relative_path
    
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.flatten:
        output_single_diff_files(output_dir, patch_funcs, decomp_funcs)
    else:
        output_diff_dirs(output_dir, patch_funcs, decomp_funcs, decomp_func_files)

if __name__ == "__main__":
    main()
