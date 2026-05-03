import importlib.util
from pathlib import Path

import pytest

FIXTURE_SCRIPT = Path("scripts/kelon168_fixture.py")
spec = importlib.util.spec_from_file_location("kelon168_fixture", FIXTURE_SCRIPT)
kelon168_fixture = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(kelon168_fixture)


def test_reverse_bits():
    assert kelon168_fixture.reverse_bits(0x83) == 0xC1
    assert kelon168_fixture.reverse_bits(0x06) == 0x60
    assert kelon168_fixture.reverse_bits(0xA0) == 0x05


def test_lsb_fixture_validates():
    kelon168_fixture.validate(Path("tests/fixtures/display_off_lsb_first.fixture"))


def test_msb_fixture_validates_after_bit_reversal():
    kelon168_fixture.validate(Path("tests/fixtures/display_off_msb_first.fixture"))


def test_missing_bit_order_rejected(tmp_path):
    fixture = tmp_path / "missing_bit_order.fixture"
    fixture.write_text(
        "bytes: 83 06 00 71 00 00 A0 00 00 00 00 00 00 D1 00 00 00 00 38 00 38\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="bit_order must be declared"):
        kelon168_fixture.validate(fixture)


def test_bad_checksum_rejected(tmp_path):
    fixture = tmp_path / "bad_checksum.fixture"
    fixture.write_text(
        "bit_order: lsb_first\n"
        "bytes: 83 06 00 71 00 00 A0 00 00 00 00 00 00 00 00 00 00 00 38 00 38\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="checksum byte 13 mismatch"):
        kelon168_fixture.validate(fixture)
