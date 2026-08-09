#!/usr/bin/env python3
"""
Chuyen doi MP4/GIF -> GIF vuong, toi uu cho ESP32 GC9A01 240x240.

Yeu cau: ffmpeg  (sudo apt install ffmpeg)

Vi du:
  python3 tools/mp4_to_gif.py video.mp4
  python3 tools/mp4_to_gif.py video.mp4 -o data/anim1.gif -s 240 -f 12
  python3 tools/mp4_to_gif.py video.mp4 --start 5 --duration 8 -f 10
"""

import argparse
import os
import subprocess
import sys
import tempfile


def check_ffmpeg():
    try:
        subprocess.run(['ffmpeg', '-version'], capture_output=True, check=True)
    except FileNotFoundError:
        print('Loi: ffmpeg chua duoc cai dat.')
        print('  Ubuntu/Debian : sudo apt install ffmpeg')
        print('  macOS         : brew install ffmpeg')
        sys.exit(1)


def crop_scale_fps(size, fps):
    return (
        f'crop=min(iw\\,ih):min(iw\\,ih),'
        f'scale={size}:{size}:flags=lanczos,'
        f'fps={fps}'
    )


def convert(input_path, output_path, size, fps, start, duration, colors):
    out_dir = os.path.dirname(os.path.abspath(output_path))
    os.makedirs(out_dir, exist_ok=True)

    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as tmp:
        palette_path = tmp.name
    with tempfile.NamedTemporaryFile(suffix='.gif', delete=False, dir=out_dir) as tmp:
        render_path = tmp.name

    try:
        vf = crop_scale_fps(size, fps)

        seek = []
        if start is not None:
            seek += ['-ss', str(start)]
        if duration is not None:
            seek += ['-t', str(duration)]

        # Buoc 1: tao palette toi uu
        print(f'[1/2] Tao palette ({colors} mau)...')
        r = subprocess.run(
            ['ffmpeg', '-y'] + seek + [
                '-i', input_path,
                '-vf', f'{vf},palettegen=max_colors={colors}:stats_mode=diff',
                palette_path,
            ],
            capture_output=True,
        )
        if r.returncode != 0:
            print('Loi tao palette:\n', r.stderr.decode())
            sys.exit(1)

        # Buoc 2: render GIF voi palette (ghi ra file tam neu input = output)
        print(f'[2/2] Render GIF {size}x{size} @ {fps}fps -> {output_path} ...')
        r = subprocess.run(
            ['ffmpeg', '-y'] + seek + [
                '-i', input_path,
                '-i', palette_path,
                '-filter_complex',
                (
                    f'{vf} [x];'
                    ' [x][1:v] paletteuse=dither=bayer'
                    ':bayer_scale=5:diff_mode=rectangle'
                ),
                render_path,
            ],
            capture_output=True,
        )
        if r.returncode != 0:
            os.remove(render_path)
            print('Loi render GIF:\n', r.stderr.decode())
            sys.exit(1)

        os.replace(render_path, output_path)
        size_kb = os.path.getsize(output_path) / 1024
        print(f'Hoan thanh!  {output_path}  ({size_kb:.0f} KB)')

        if size_kb > 800:
            print()
            print('Canh bao: file > 800 KB, co the qua lon cho LittleFS.')
            print(f'  Thu:  -f {fps - 2}   (giam fps)')
            print('        --colors 64   (giam so mau)')
            print('        --duration 5  (cat ngan clip)')

    finally:
        if os.path.exists(palette_path):
            os.remove(palette_path)
        if os.path.exists(render_path):
            os.remove(render_path)


def main():
    parser = argparse.ArgumentParser(
        description='Chuyen MP4/GIF -> GIF vuong 240x240 cho ESP32 GC9A01'
    )
    parser.add_argument('input', help='File dau vao (mp4, gif, ...)')
    parser.add_argument(
        '-o', '--output',
        help='File GIF dau ra (mac dinh: ten_file_goc.gif)',
    )
    parser.add_argument(
        '-s', '--size', type=int, default=240,
        help='Kich thuoc pixel vuong — mac dinh: 240',
    )
    parser.add_argument(
        '-f', '--fps', type=int, default=12,
        help='Frame/giay — mac dinh: 12',
    )
    parser.add_argument(
        '--start', type=float, default=None,
        help='Bat dau tai giay N',
    )
    parser.add_argument(
        '--duration', type=float, default=None,
        help='Chi lay N giay',
    )
    parser.add_argument(
        '--colors', type=int, default=128,
        help='So mau palette 16-256 — mac dinh: 128',
    )

    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"Loi: khong tim thay file '{args.input}'")
        sys.exit(1)

    output = args.output or (os.path.splitext(args.input)[0] + '.gif')
    colors = max(16, min(256, args.colors))

    check_ffmpeg()
    convert(args.input, output, args.size, args.fps, args.start, args.duration, colors)


if __name__ == '__main__':
    main()
