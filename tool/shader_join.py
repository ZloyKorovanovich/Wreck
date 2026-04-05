#!/usr/bin/env python3
import argparse
import re
from pathlib import Path


def make_macro_name(stem: str) -> str:
    name = stem.upper()
    name = re.sub(r'[^A-Z0-9]', '_', name)
    name = re.sub(r'_+', '_', name).strip('_')
    if not name:
        name = "SHADER"
    if name[0].isdigit():
        name = "_" + name
    return name


def collect_spv_files(directory: Path):
    return sorted(
        [p for p in directory.iterdir() if p.is_file() and p.suffix.lower() == ".spv"],
        key=lambda p: p.name.lower()
    )


def ensure_unique_names(files):
    used = {}
    result = []

    for path in files:
        base = make_macro_name(path.stem)

        if base not in used:
            used[base] = 0
            unique = base
        else:
            used[base] += 1
            unique = f"{base}_{used[base]}"

        result.append((path, unique))

    return result


def write_outputs(entries, header_path: Path, bin_path: Path):
    offset = 0
    records = []

    with bin_path.open("wb") as bout:
        for spv_path, macro in entries:
            data = spv_path.read_bytes()
            bout.write(data)

            records.append({
                "macro": macro,
                "file": spv_path.name,
                "begin": offset,
                "size": len(data),
            })

            offset += len(data)

    with header_path.open("w", encoding="utf-8", newline="\n") as hout:

        hout.write("#ifndef _SHADERS_AUTO_INCLUDED\n")
        hout.write("#define _SHADERS_AUTO_INCLUDED\n\n")
        hout.write("// Auto-generated file. Do not edit manually.\n\n")

        for r in records:
            hout.write(f"// {r['file']}\n")
            hout.write(f'#define SHADER_{r["macro"]}_NAME "{r["file"]}"\n')
            hout.write(f"#define SHADER_{r['macro']}_BEGIN {r['begin']}\n")
            hout.write(f"#define SHADER_{r['macro']}_SIZE  {r['size']}\n\n")

        hout.write(f"#define SHADER_BLOB_TOTAL_SIZE {offset}\n\n")
        hout.write("#endif\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir")
    parser.add_argument("output_bin")
    parser.add_argument("output_header")

    args = parser.parse_args()

    input_dir = Path(args.input_dir)
    output_bin = Path(args.output_bin)
    output_header = Path(args.output_header)

    spv_files = collect_spv_files(input_dir)
    entries = ensure_unique_names(spv_files)

    output_bin.parent.mkdir(parents=True, exist_ok=True)
    output_header.parent.mkdir(parents=True, exist_ok=True)

    write_outputs(entries, output_header, output_bin)

    print(f"Packed {len(entries)} shaders")


if __name__ == "__main__":
    main()
