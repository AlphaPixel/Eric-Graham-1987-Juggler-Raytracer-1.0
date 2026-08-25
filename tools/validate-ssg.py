#!/usr/bin/env python3
"""Validate reconstructed ssg output against captured original samples."""

from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Sample:
    name: str
    dat: str
    original_rgb: str
    recreated_rgb: str
    original_ss: str | None
    recreated_ss: str | None
    expected_len: int
    max_diff: int
    max_mae: float
    max_abs: int
    expected_ss_width: int | None = None
    expected_ss_height: int | None = None
    expected_ss_len: int | None = None
    extra_args: tuple[str, ...] = ()
    render_args: tuple[str, ...] = ("S=2",)


SAMPLES = (
    Sample(
        name="robot",
        dat="robot.dat",
        original_rgb="robot-s2-fresh.rgb",
        recreated_rgb="robot-s2-recreated.rgb",
        original_ss="robot-s2.ssimg",
        recreated_ss="robot-s2-recreated.ssimg",
        expected_len=12000,
        max_diff=10,
        max_mae=0.00442,
        max_abs=44,
        expected_ss_width=80,
        expected_ss_height=50,
        expected_ss_len=3052,
    ),
    Sample(
        name="robot-full",
        dat="robot.dat",
        original_rgb="robot.rgb",
        recreated_rgb="robot-recreated.rgb",
        original_ss=None,
        recreated_ss=None,
        expected_len=192000,
        max_diff=233,
        max_mae=0.04348,
        max_abs=149,
        render_args=(),
    ),
    Sample(
        name="ele",
        dat="ele.dat",
        original_rgb="ele-s2.rgb",
        recreated_rgb="ele-s2-recreated.rgb",
        original_ss=None,
        recreated_ss="ele-s2-recreated.ssimg",
        expected_len=12000,
        max_diff=4,
        max_mae=0.00034,
        max_abs=1,
        expected_ss_width=80,
        expected_ss_height=50,
        expected_ss_len=3052,
    ),
    Sample(
        name="ele-full",
        dat="ele.dat",
        original_rgb="ele.rgb",
        recreated_rgb="ele-recreated.rgb",
        original_ss=None,
        recreated_ss=None,
        expected_len=192000,
        max_diff=69,
        max_mae=0.00098,
        max_abs=59,
        render_args=(),
    ),
    Sample(
        name="dragon",
        dat="dragon.dat",
        original_rgb="dragon-s2.rgb",
        recreated_rgb="dragon-s2-recreated.rgb",
        original_ss=None,
        recreated_ss="dragon-s2-recreated.ssimg",
        expected_len=12000,
        max_diff=1,
        max_mae=0.00009,
        max_abs=1,
        expected_ss_width=80,
        expected_ss_height=50,
        expected_ss_len=3052,
    ),
    Sample(
        name="robot-v",
        dat="robot.dat",
        original_rgb="robot-s2-v.rgb",
        recreated_rgb="robot-s2-v-recreated.rgb",
        original_ss="robot-s2-v.ssimg",
        recreated_ss="robot-s2-v-recreated.ssimg",
        expected_len=12000,
        max_diff=4,
        max_mae=0.00800,
        max_abs=50,
        expected_ss_width=80,
        expected_ss_height=50,
        expected_ss_len=3052,
        extra_args=("V",),
    ),
)


def compare_bytes(a: bytes, b: bytes) -> tuple[int, float, int]:
    n = min(len(a), len(b))
    total = 0
    diff = 0
    max_abs = 0
    for i in range(n):
        d = abs(a[i] - b[i])
        if d:
            diff += 1
            total += d
            if d > max_abs:
                max_abs = d
    if len(a) != len(b):
        diff += abs(len(a) - len(b))
        max_abs = 255
    denom = max(n, 1)
    return diff, total / denom, max_abs


def palette_block(path: Path) -> bytes:
    data = path.read_bytes()
    if len(data) < 52:
        raise ValueError(f"{path} is too short to contain an ss header")
    return data[4:52]


def ss_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 4:
        raise ValueError(f"{path} is too short to contain ss dimensions")
    return int.from_bytes(data[0:2], "big"), int.from_bytes(data[2:4], "big")


def run_sample(exe: Path, scene_dir: Path, sample: Sample) -> None:
    args = [
        str(exe),
        "E",
        *sample.extra_args,
        *sample.render_args,
        f"I={sample.dat}",
        f"D={sample.recreated_rgb}",
    ]
    if sample.recreated_ss:
        args.append(f"O={sample.recreated_ss}")
    subprocess.run(args, cwd=scene_dir, check=True)


def validate(scene_dir: Path, sample: Sample) -> bool:
    ok = True
    original_rgb = scene_dir / sample.original_rgb
    recreated_rgb = scene_dir / sample.recreated_rgb
    if not original_rgb.exists() or not recreated_rgb.exists():
        print(f"{sample.name}: missing RGB comparison file", file=sys.stderr)
        return False

    original_data = original_rgb.read_bytes()
    recreated_data = recreated_rgb.read_bytes()
    diff, mae, max_abs = compare_bytes(original_data, recreated_data)
    print(
        f"{sample.name}: RGB diff={diff}/{sample.expected_len} mae={mae:.5f} max={max_abs}"
    )
    if len(original_data) != sample.expected_len or len(recreated_data) != sample.expected_len:
        print(
            f"{sample.name}: unexpected RGB length "
            f"{len(original_data)}/{len(recreated_data)}",
            file=sys.stderr,
        )
        ok = False
    if diff > sample.max_diff:
        print(
            f"{sample.name}: RGB diff exceeds limit {diff} > {sample.max_diff}",
            file=sys.stderr,
        )
        ok = False
    if mae > sample.max_mae:
        print(
            f"{sample.name}: RGB mae exceeds limit {mae:.5f} > {sample.max_mae:.5f}",
            file=sys.stderr,
        )
        ok = False
    if max_abs > sample.max_abs:
        print(
            f"{sample.name}: RGB max error exceeds limit {max_abs} > {sample.max_abs}",
            file=sys.stderr,
        )
        ok = False

    if sample.recreated_ss:
        recreated_ss = scene_dir / sample.recreated_ss
        if not recreated_ss.exists():
            print(f"{sample.name}: missing recreated ss file", file=sys.stderr)
            ok = False
        else:
            ss_len = recreated_ss.stat().st_size
            width, height = ss_dimensions(recreated_ss)
            if (
                sample.expected_ss_width is not None
                and sample.expected_ss_height is not None
                and (width != sample.expected_ss_width or height != sample.expected_ss_height)
            ):
                print(
                    f"{sample.name}: unexpected ss dimensions "
                    f"{width}x{height}",
                    file=sys.stderr,
                )
                ok = False
            if sample.expected_ss_len is not None and ss_len != sample.expected_ss_len:
                print(
                    f"{sample.name}: unexpected ss length "
                    f"{ss_len} != {sample.expected_ss_len}",
                    file=sys.stderr,
                )
                ok = False
            if sample.expected_ss_width is not None and sample.expected_ss_height is not None:
                print(f"{sample.name}: ss dimensions {width}x{height}, length {ss_len}")

            if sample.original_ss:
                original_ss = scene_dir / sample.original_ss
                if not original_ss.exists():
                    print(f"{sample.name}: missing ss palette comparison file", file=sys.stderr)
                    ok = False
                elif palette_block(original_ss) == palette_block(recreated_ss):
                    print(f"{sample.name}: ss palette/register block matches")
                else:
                    print(f"{sample.name}: ss palette/register block differs", file=sys.stderr)
                    ok = False

    return ok


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Validate reconstructed ssg samples against captured original outputs."
    )
    parser.add_argument(
        "--scene-dir",
        default="Raytracer_1987_Graham_Source_Code",
        type=Path,
        help="directory containing dat files and captured sample outputs",
    )
    parser.add_argument(
        "--exe",
        type=Path,
        help="optional reconstructed ssg executable to run before comparing",
    )
    args = parser.parse_args(argv)

    scene_dir = args.scene_dir.resolve()
    if not scene_dir.exists():
        print(f"scene directory not found: {scene_dir}", file=sys.stderr)
        return 2

    if args.exe:
        exe = args.exe.resolve()
        if not exe.exists():
            print(f"executable not found: {exe}", file=sys.stderr)
            return 2
        for sample in SAMPLES:
            run_sample(exe, scene_dir, sample)

    ok = True
    for sample in SAMPLES:
        ok = validate(scene_dir, sample) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
