"""Extract esp_hosted component zip using Win32 extended paths (no LongPathsEnabled required)."""
from __future__ import annotations

import os
import sys
import zipfile


def win_long_path(path: str) -> str:
    path = os.path.abspath(path)
    if path.startswith("\\\\?\\"):
        return path
    if path.startswith("\\\\"):
        return "\\\\?\\UNC\\" + path[2:]
    return "\\\\?\\" + path


def unpack_zip_long(zip_path: str, dest_dir: str) -> None:
    dest_long = win_long_path(dest_dir)
    os.makedirs(dest_long, exist_ok=True)
    with zipfile.ZipFile(zip_path) as archive:
        for item in archive.infolist():
            rel = item.filename.replace("/", os.sep).replace("\\", os.sep)
            target = win_long_path(os.path.join(dest_dir, rel))
            parent = win_long_path(os.path.dirname(target))
            if parent:
                os.makedirs(parent, exist_ok=True)
            if item.is_dir():
                os.makedirs(target, exist_ok=True)
                continue
            data = archive.read(item.filename)
            with open(target, "wb") as out:
                out.write(data)


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: extract_esp_hosted_zip.py <zip> <destination_dir>", file=sys.stderr)
        return 2
    unpack_zip_long(sys.argv[1], sys.argv[2])
    print("OK:", sys.argv[2])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
