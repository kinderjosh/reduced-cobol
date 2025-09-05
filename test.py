import os
import sys
import subprocess
import platform

EXEC = "cobc"

ESC_NORMAL = "\033[0m"
ESC_BOLD = "\033[1m"
ESC_RED = "\033[91m"
ESC_GREEN = "\033[92m"

ignored = ["MESSAGEBOX.COB"]

def run_examples(test_dir: str, on_windows: bool) -> tuple[int, int]:
    delim = '/'
    exec = f"./{EXEC}"

    if on_windows:
        delim = "\\"
        exec = f".\\{EXEC}.exe"

    if test_dir[-1] != delim:
        test_dir += delim

    if not os.path.exists(test_dir):
        print(f"error: no such directory {test_dir}")
        exit(1)
    
    passed = 0
    total = 0

    for example in os.listdir(test_dir):
        if "." not in example or example.split(".")[1] != "COB" or example in ignored:
            continue

        try:
            subprocess.check_output([exec, "run", f".{delim}{test_dir}{example}"], shell=True)
            print(f"{ESC_BOLD}{example}: {ESC_GREEN}passed{ESC_NORMAL}")
            passed += 1
        except subprocess.CalledProcessError:
            print(f"{ESC_BOLD}{example}: {ESC_RED}failed{ESC_NORMAL}")

        total += 1

    return (passed, total)

def main():
    if len(sys.argv) < 2:
        print("error: missing examples directory")
        return
    elif sys.argv[1] == "--help":
        print(f"usage: python3 {sys.argv[0]} <examples directory>")
        return

    on_windows = False

    if platform.system() == "Windows":
        on_windows = True

    print(f"{ESC_BOLD}test: {ESC_NORMAL}compiling and running examples...")
    passes = run_examples(sys.argv[1], on_windows)
    print(f"{ESC_BOLD}test: {ESC_NORMAL}{passes[0]}/{passes[1]} passed")

if __name__ == "__main__":
    main()