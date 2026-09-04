"""Find simple absolute-address materializations in Aurora Rev1655.

Usage:
  python research/rev1655_xrefs.py <hex-address> [context-count]
  python research/rev1655_xrefs.py --range <start-address> <end-address>
  python research/rev1655_xrefs.py --branches <target-address> [context-count]
  python research/rev1655_xrefs.py --grep <regex> [context-count]
  python research/rev1655_xrefs.py --function <address>

The Xbox 360 compiler commonly materializes an address with lis/addi or
lis/ori. This helper finds those pairs in Aurora.exe and prints nearby PPC
instructions. It is a research aid, not part of the plugin build.
"""

from __future__ import annotations

import re
import sys

import capstone
import pefile


IMAGE = "original/Aurora.exe"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__.strip())
        return 2

    pe = pefile.PE(IMAGE)
    text_section = next(
        section for section in pe.sections
        if section.Name.rstrip(b"\0") == b".text"
    )
    code = text_section.get_data()
    base = pe.OPTIONAL_HEADER.ImageBase + text_section.VirtualAddress
    disassembler = capstone.Cs(
        capstone.CS_ARCH_PPC,
        capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN,
    )
    disassembler.skipdata = True

    if sys.argv[1] == "--function":
        if len(sys.argv) != 3:
            print(__doc__.strip())
            return 2
        target = int(sys.argv[2], 0)
        pdata_section = next(
            section for section in pe.sections
            if section.Name.rstrip(b"\0") == b".pdata"
        )
        pdata = pdata_section.get_data()
        starts = sorted(
            int.from_bytes(pdata[offset:offset + 4], "big")
            for offset in range(0, len(pdata) - 7, 8)
            if base <= int.from_bytes(pdata[offset:offset + 4], "big") <
                base + len(code)
        )
        start_index = max(
            index for index, address in enumerate(starts)
            if address <= target
        )
        start_address = starts[start_index]
        end_address = starts[start_index + 1]
        start = start_address - base
        end = end_address - base
        print(f"function 0x{start_address:08X}..0x{end_address:08X}")
        for instruction in disassembler.disasm(code[start:end], start_address):
            print(
                f"{instruction.address:08X}  "
                f"{instruction.mnemonic:<9} {instruction.op_str}"
            )
        return 0

    if sys.argv[1] == "--range":
        if len(sys.argv) != 4:
            print(__doc__.strip())
            return 2
        start_address = int(sys.argv[2], 0)
        end_address = int(sys.argv[3], 0)
        if start_address < base or end_address <= start_address:
            return 2
        start = start_address - base
        end = min(len(code), end_address - base)
        for instruction in disassembler.disasm(
            code[start:end], start_address
        ):
            print(
                f"{instruction.address:08X}  "
                f"{instruction.mnemonic:<9} {instruction.op_str}"
            )
        return 0

    if sys.argv[1] == "--branches":
        if len(sys.argv) not in (3, 4):
            print(__doc__.strip())
            return 2
        target = int(sys.argv[2], 0)
        context = int(sys.argv[3], 0) if len(sys.argv) == 4 else 8
        instructions = list(disassembler.disasm(code, base))
        hits = [
            index for index, instruction in enumerate(instructions)
            if instruction.mnemonic in ("b", "bl") and
            instruction.op_str == f"0x{target:x}"
        ]
        for hit in hits:
            start = max(0, hit - context)
            end = min(len(instructions), hit + context + 1)
            print(f"\nbranch near 0x{instructions[hit].address:08X}")
            for index in range(start, end):
                instruction = instructions[index]
                marker = ">" if index == hit else " "
                print(
                    f"{marker} {instruction.address:08X}  "
                    f"{instruction.mnemonic:<9} {instruction.op_str}"
                )
        print(f"\n{len(hits)} branch(es) to 0x{target:08X}")
        return 0

    if sys.argv[1] == "--grep":
        if len(sys.argv) not in (3, 4):
            print(__doc__.strip())
            return 2
        pattern = re.compile(sys.argv[2], re.IGNORECASE)
        context = int(sys.argv[3], 0) if len(sys.argv) == 4 else 6
        instructions = list(disassembler.disasm(code, base))
        hits = [
            index for index, instruction in enumerate(instructions)
            if pattern.search(
                f"{instruction.mnemonic} {instruction.op_str}")
        ]
        for hit in hits:
            start = max(0, hit - context)
            end = min(len(instructions), hit + context + 1)
            print(f"\nmatch near 0x{instructions[hit].address:08X}")
            for index in range(start, end):
                instruction = instructions[index]
                marker = ">" if index == hit else " "
                print(
                    f"{marker} {instruction.address:08X}  "
                    f"{instruction.mnemonic:<9} {instruction.op_str}"
                )
        print(f"\n{len(hits)} match(es) for {pattern.pattern!r}")
        return 0

    target = int(sys.argv[1], 0)
    context = int(sys.argv[2], 0) if len(sys.argv) > 2 else 8
    upper_addi = ((target + 0x8000) >> 16) & 0xFFFF
    upper_ori = (target >> 16) & 0xFFFF
    low = target & 0xFFFF
    hits: list[int] = []

    words = [int.from_bytes(code[i:i + 4], "big") for i in range(0, len(code), 4)]
    for index, word in enumerate(words):
        if word >> 26 != 15:  # addis / lis
            continue
        destination = (word >> 21) & 31
        source = (word >> 16) & 31
        immediate = word & 0xFFFF
        if source != 0 or immediate not in (upper_addi, upper_ori):
            continue
        for lookahead in range(index + 1, min(index + 9, len(words))):
            later = words[lookahead]
            opcode = later >> 26
            later_destination = (later >> 21) & 31
            later_source = (later >> 16) & 31
            later_immediate = later & 0xFFFF
            addi_match = (
                opcode == 14 and later_source == destination and
                later_immediate == low and immediate == upper_addi
            )
            ori_match = (
                opcode == 24 and later_destination == destination and
                later_source == destination and later_immediate == low and
                immediate == upper_ori
            )
            if addi_match or ori_match:
                hits.append(index)
                break

    for hit in hits:
        start = max(0, hit - context)
        end = min(len(words), hit + context + 9)
        address = base + start * 4
        print(f"\nreference near 0x{base + hit * 4:08X}")
        for instruction in disassembler.disasm(
            code[start * 4:end * 4], address
        ):
            marker = ">" if instruction.address == base + hit * 4 else " "
            print(
                f"{marker} {instruction.address:08X}  "
                f"{instruction.mnemonic:<9} {instruction.op_str}"
            )

    print(f"\n{len(hits)} materialization(s) for 0x{target:08X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
