import subprocess
import sys
import re
import os

def run_command(command):
    try:
        result = subprocess.run(
            command,
            shell=True,
            check=False,  # Don't throw exception on error, we want to parse it
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True
        )
        return result.returncode, result.stdout, result.stderr
    except Exception as e:
        return -1, "", str(e)

def find_symbols(obj_file, pattern):
    # Use llvm-nm to find symbols in the object file
    # We look for defined external symbols (T)
    cmd = f"../llvm/bin/llvm-nm --defined-only {obj_file}"
    code, stdout, stderr = run_command(cmd)
    if code != 0:
        print(f"Error running llvm-nm on {obj_file}: {stderr}")
        return []

    symbols = []
    for line in stdout.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            sym_type = parts[1]
            sym_name = parts[2]
            # Simple fuzzy matching: check if the core function name is present
            if pattern in sym_name:
                 symbols.append(sym_name)
    return symbols

def main():
    # 1. The failing link command (minimal repro)
    link_cmd = "../llvm/bin/clang++ @env/link.rsp llappviewerwin32.cpp.obj fsdata.cpp.obj -o repro.exe"

    print(f"Running link command: {link_cmd}")
    code, stdout, stderr = run_command(link_cmd)

    if code == 0:
        print("Link successful! No fixes needed.")
        return 0

    print("Link failed. Analyzing errors...")

    # 2. Parse undefined symbols
    undefined_symbols = []
    # Regex to capture the undefined symbol.
    # lld-link: error: undefined symbol: ?trigger_signal@@...
    regex = re.compile(r"lld-link: error: undefined symbol: (\S+)")

    for line in stderr.splitlines():
        match = regex.search(line)
        if match:
            undefined_symbols.append(match.group(1))

    if not undefined_symbols:
        print("No specific undefined symbols found to fix.")
        print(stderr)
        return 1

    print(f"Found undefined symbols: {undefined_symbols}")

    aliases = []

    # 3. Find candidates in fsdata.cpp.obj (the lib/snapshot)
    lib_obj = "fsdata.cpp.obj"

    for missing_sym in undefined_symbols:
        if "wWinMainCRTStartup" in missing_sym:
            continue # Ignore entry point

        # Extract the likely function name (between ? and @@)
        func_name_match = re.search(r"\?(\w+)@@", missing_sym)
        if func_name_match:
            func_name = func_name_match.group(1)
            print(f"Searching for candidates for function: {func_name} in {lib_obj}")

            candidates = find_symbols(lib_obj, func_name)
            print(f"Candidates found: {candidates}")

            if len(candidates) == 1:
                found_sym = candidates[0]
                print(f"Match found! Mapping {missing_sym} -> {found_sym}")
                aliases.append(f"/ALTERNATENAME:{missing_sym}={found_sym}")
            elif len(candidates) > 1:
                print("Multiple candidates found. Skipping ambiguous match.")
            else:
                print("No candidates found.")
        else:
             print(f"Could not parse function name from {missing_sym}")

    if not aliases:
        print("No aliases generated.")
        return 1

    # 4. Generate response file
    # Ensure directory exists
    os.makedirs("polyfills", exist_ok=True)
    rsp_file = "polyfills/autogen.link.rsp"
    with open(rsp_file, "w") as f:
        for alias in aliases:
            f.write(f"-Wl,{alias}\n")

    print(f"Generated aliases in {rsp_file}")

    # 5. Retry link
    retry_cmd = f"{link_cmd} @{rsp_file}"
    print(f"Retrying link: {retry_cmd}")
    code, stdout, stderr = run_command(retry_cmd)

    if code == 0:
        print("Link successful with aliases!")
        return 0
    else:
        print("Link failed again.")
        print(stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
