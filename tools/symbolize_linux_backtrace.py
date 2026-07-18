import argparse
import os
from pathlib import Path
import re
import subprocess

TRACE_LINE_RE = re.compile(r"([\/\w\- \.]+)\(([\w]*[+\-]0x[a-fA-F0-9]+)\)\s*\[0x[a-fA-F0-9]+\]")

def run_addr2line(filepath: str, addr: str):
    if addr.startswith("+") or addr.startswith("-"):
        addr = addr[1:]

    subprocess.check_call(["addr2line", "-e", filepath, "-a", "-p", "-f", "-C", addr])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", type=str, help="File containing the backtrace.")
    parser.add_argument("-C", type=str, help="Change to this directory before running addr2line.")
    args = parser.parse_args()

    if args.C:
        os.chdir(Path(args.C).resolve())

    with open(args.file, "r", encoding="utf-8") as file:
        for line in file.readlines():
            match = TRACE_LINE_RE.match(line)
            if match != None:
                run_addr2line(match.group(1), match.group(2))

if __name__ == "__main__":
    main()
