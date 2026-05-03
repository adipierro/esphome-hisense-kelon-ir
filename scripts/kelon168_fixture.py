#!/usr/bin/env python3
"""Validate Kelon168 capture fixtures.

The fixture format is intentionally tiny:

bit_order: lsb_first
bytes: 83 06 00 71 00 00 80 00 00 00 00 00 00 71 00 06 00 00 38 00 3E

Set bit_order to msb_first when bytes were exported in MSB-first display order.
Those bytes are bit-reversed per byte before checksum validation.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def reverse_bits(value: int) -> int:
    value = ((value & 0xF0) >> 4) | ((value & 0x0F) << 4)
    value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2)
    return ((value & 0xAA) >> 1) | ((value & 0x55) << 1)


def parse_fixture(path: Path) -> tuple[str, list[int]]:
    bit_order = None
    raw_bytes = None
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        key, _, value = line.partition(":")
        key = key.strip()
        value = value.strip()
        if key == "bit_order":
            bit_order = value
        elif key == "bytes":
            raw_bytes = [int(item, 16) for item in value.replace(",", " ").split()]

    if bit_order not in ("lsb_first", "msb_first"):
        raise ValueError(f"{path}: bit_order must be declared as lsb_first or msb_first")
    if raw_bytes is None:
        raise ValueError(f"{path}: missing bytes")
    if len(raw_bytes) != 21:
        raise ValueError(f"{path}: expected 21 bytes, got {len(raw_bytes)}")
    return bit_order, raw_bytes


def normalize(bit_order: str, values: list[int]) -> list[int]:
    if bit_order == "lsb_first":
        return values
    return [reverse_bits(value) for value in values]


def xor(values: list[int]) -> int:
    out = 0
    for value in values:
        out ^= value
    return out


def validate(path: Path) -> None:
    bit_order, raw = parse_fixture(path)
    values = normalize(bit_order, raw)
    if values[0:2] != [0x83, 0x06]:
        raise ValueError(f"{path}: bad preamble after {bit_order} normalization")
    if values[13] != xor(values[2:13]):
        raise ValueError(f"{path}: checksum byte 13 mismatch")
    if values[20] != xor(values[14:20]):
        raise ValueError(f"{path}: checksum byte 20 mismatch")
    print(f"{path}: ok command=0x{values[15]:02X}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixtures", nargs="+", type=Path)
    args = parser.parse_args()
    for fixture in args.fixtures:
        validate(fixture)


if __name__ == "__main__":
    main()
