#!/usr/bin/env python3
import argparse
# Support file for analyzing non-canonical addresses instead of guessing
# ANSI Color Constants
RESET = "\x1b[0m"
BOLD = "\x1b[1m"
GREEN = "\x1b[32m"
RED = "\x1b[31m"
BOLD_RED = "\x1b[1;31m"
BOLD_GREEN = "\x1b[1;32m"
CYAN = "\x1b[36m"


class X86AddressAnalyzer:
    """Analyzes x86-64 virtual addresses for canonical form compliance."""

    def __init__(self, mode: int = 4):
        if mode not in (4, 5):
            raise ValueError("Unsupported paging mode. Use 4 or 5.")
        self.mode = mode
        self.sign_bit_idx = 47 if mode == 4 else 56

    def is_canonical(self, addr: int) -> bool:
        """Checks if a given integer address is canonical."""
        shifted = addr >> self.sign_bit_idx
        max_val = (1 << (64 - self.sign_bit_idx)) - 1
        return shifted == 0 or shifted == max_val

    def print_ranges(self) -> None:
        """Displays canonical and non-canonical ranges for the active mode."""
        if self.mode == 4:
            print(f"{CYAN}x86-64 4-Level Paging (48-bit Virtual Addressing){RESET}")
            print("Canonical Ranges:")
            print("  Lower Half : 0x0000000000000000 to 0x00007FFFFFFFFFFF")
            print("  Upper Half : 0xFFFF800000000000 to 0xFFFFFFFFFFFFFFFF")
            print(f"\n{RED}Non-Canonical Range:{RESET}")
            print("  Range      : 0x0000800000000000 to 0xFFFF7FFFFFFFFFFF")
        elif self.mode == 5:
            print(f"{CYAN}x86-64 5-Level Paging (57-bit Virtual Addressing){RESET}")
            print("Canonical Ranges:")
            print("  Lower Half : 0x0000000000000000 to 0x00FFFFFFFFFFFFFF")
            print("  Upper Half : 0xFF00000000000000 to 0xFFFFFFFFFFFFFFFF")
            print(f"\n{RED}Non-Canonical Range:{RESET}")
            print("  Range      : 0x0100000000000000 to 0xFEFFFFFFFFFFFFFF")

    def analyze(self, address_str: str) -> None:
        """Parses and validates a hexadecimal address string."""
        try:
            # Clean up optional '0x' prefix if desired, int() handles it automatically
            addr = int(address_str, 16)
            is_canon = self.is_canonical(addr)

            status = (
                f"{BOLD_GREEN}CANONICAL{RESET}"
                if is_canon
                else f"{BOLD_RED}NON-CANONICAL{RESET}"
            )
            print(
                f"Address {BOLD_GREEN}{hex(addr)}{RESET} (Mode {self.mode}) is: {status}"
            )
        except ValueError:
            print(
                f"{RED}Error: '{address_str}' is not a valid hexadecimal address.{RESET}"
            )


def main():
    parser = argparse.ArgumentParser(
        description="Identify canonical vs. non-canonical x86-64 virtual addresses."
    )
    parser.add_argument(
        "--mode", type=int, default=4, choices=[4, 5], help="Paging mode (4 or 5)"
    )
    parser.add_argument("--address", type=str, help="Hexadecimal address to check")
    args = parser.parse_args()

    # Instantiate the analyzer class
    analyzer = X86AddressAnalyzer(mode=args.mode)

    # Print mode information
    analyzer.print_ranges()
    print()

    # Check address if provided
    if args.address:
        analyzer.analyze(args.address)
    else:
        print("Tip: Use '--address <hex>' to check a specific virtual address.")


if __name__ == "__main__":
    main()
