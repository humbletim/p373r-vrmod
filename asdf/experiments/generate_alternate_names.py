#!/usr/bin/env python3
"""
Generate /ALTERNATENAME linker flags by fuzzy-matching undefined symbols
against defined symbols in provided object files.

Usage:
    linker_command 2> link_errors.txt
    python3 generate_alternate_names.py --objects library.obj < link_errors.txt > polyfills.rsp
"""

import argparse
import sys
import re
import subprocess
import shutil
import os
from typing import Set, List, Optional

def get_defined_symbols(nm_path: str, obj_files: List[str]) -> Set[str]:
    """
    Returns a set of defined symbol names from the object files.
    """
    defined_symbols = set()

    # Check if nm executable exists (either in PATH or at absolute path)
    if not shutil.which(nm_path) and not os.path.exists(nm_path):
         sys.stderr.write(f"Error: llvm-nm executable not found at '{nm_path}'.\n")
         sys.exit(1)

    for obj_file in obj_files:
        if not os.path.exists(obj_file):
            sys.stderr.write(f"Warning: Object file '{obj_file}' does not exist.\n")
            continue

        # -j: print just the symbol name
        # --defined-only: show only defined symbols
        cmd = [nm_path, "--defined-only", "-j", obj_file]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=False)
            if result.returncode != 0:
                sys.stderr.write(f"Warning: llvm-nm failed for {obj_file}: {result.stderr}\n")
                continue

            for line in result.stdout.splitlines():
                line = line.strip()
                if line:
                    defined_symbols.add(line)

        except OSError as e:
             sys.stderr.write(f"Error executing {nm_path}: {e}\n")
             sys.exit(1)

    return defined_symbols

def extract_core_name(mangled_name: str) -> Optional[str]:
    """
    Extracts the core function name from an MSVC mangled symbol.
    Example: ?funcName@Namespace@... -> funcName
    """
    # Matches the name between the leading ? and the first @
    # This assumes standard MSVC mangling for C++ symbols.
    match = re.search(r"^\?([^@]+)@", mangled_name)
    if match:
        return match.group(1)
    return None

def main():
    parser = argparse.ArgumentParser(
        description="Generate /ALTERNATENAME linker flags to resolve undefined symbols."
    )
    parser.add_argument(
        "--objects", "-o",
        nargs="+",
        required=True,
        help="Object files or libraries to search for symbol definitions."
    )
    parser.add_argument(
        "--nm",
        default="llvm-nm",
        help="Path to llvm-nm executable (default: llvm-nm)."
    )
    parser.add_argument(
        "--input", "-i",
        type=argparse.FileType('r'),
        default=sys.stdin,
        help="Input file containing linker errors (default: stdin)."
    )

    args = parser.parse_args()

    # 1. Parse undefined symbols from input
    undefined_symbols = []
    # Regex for lld-link error
    # Matches: lld-link: error: undefined symbol: <symbol>
    error_regex = re.compile(r"lld-link: error: undefined symbol: (\S+)")

    try:
        input_lines = args.input.readlines()
    except Exception as e:
        sys.stderr.write(f"Error reading input: {e}\n")
        return 1

    for line in input_lines:
        match = error_regex.search(line)
        if match:
            undefined_symbols.append(match.group(1))

    if not undefined_symbols:
        sys.stderr.write("No undefined symbols found in input.\n")
        return 0

    # 2. Get defined symbols from objects
    defined_symbols = get_defined_symbols(args.nm, args.objects)

    if not defined_symbols:
        sys.stderr.write("No defined symbols found in provided objects.\n")
        return 1

    # 3. Match
    matches_found = 0
    for missing_sym in undefined_symbols:
        core_name = extract_core_name(missing_sym)
        if not core_name:
            continue

        # Find candidates in defined_symbols that match the core name
        candidates = []
        for d_sym in defined_symbols:
            d_core = extract_core_name(d_sym)
            if d_core == core_name:
                candidates.append(d_sym)

        if len(candidates) == 1:
            found_sym = candidates[0]
            # Output the link flag
            # Format: -Wl,/ALTERNATENAME:missing=found
            print(f"-Wl,/ALTERNATENAME:{missing_sym}={found_sym}")
            matches_found += 1
        elif len(candidates) > 1:
            # sys.stderr.write(f"Ambiguous match for {core_name}: found {len(candidates)} candidates.\n")
            pass

    if matches_found == 0:
        sys.stderr.write("No aliases generated.\n")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
