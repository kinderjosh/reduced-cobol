import os
import subprocess

ESC_NORMAL = "\033[0m"
ESC_BOLD = "\033[1m"
ESC_RED = "\033[91m"
ESC_GREEN = "\033[92m"

ignored = ["MESSAGEBOX.COB"]

def run_examples() -> tuple[int, int]:
    passed = 0
    total = 0

    for example in os.listdir("examples"):
        if "." not in example or example.split(".")[1] != "COB" or example in ignored:
            continue

        try:
            subprocess.check_output(["cobc", "run", f"examples/{example}"])
            print(f"{ESC_BOLD}{example}: {ESC_GREEN}passed{ESC_NORMAL}")
            passed += 1
        except subprocess.CalledProcessError:
            print(f"{ESC_BOLD}{example}: {ESC_RED}failed{ESC_NORMAL}")

        total += 1

    return (passed, total)

def main():
    print(f"{ESC_BOLD}test: {ESC_NORMAL}compiling and running examples...")
    passes = run_examples()
    print(f"{ESC_BOLD}test: {ESC_NORMAL}{passes[0]}/{passes[1]} passed")

if __name__ == "__main__":
    main()