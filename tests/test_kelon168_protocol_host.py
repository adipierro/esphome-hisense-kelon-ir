import subprocess
from pathlib import Path


def test_kelon168_protocol_host(tmp_path):
    source = Path("tests/kelon168_protocol_host_test.cpp")
    binary = tmp_path / "kelon168_protocol_host_test"

    subprocess.run(
        ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)],
        check=True,
    )
    result = subprocess.run([str(binary)], check=True, text=True, capture_output=True)

    assert "kelon168 protocol host tests passed" in result.stdout
