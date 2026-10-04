"""Encode the UE-rendered preview frames as a GIF; does not generate effect imagery."""
import argparse
import json
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    files = sorted(args.directory.glob("DA_Attack01_01_character_*ms.png"))
    if len(files) < 2:
        raise RuntimeError("First export at least two UE-rendered frames to the specified directory as DA_Attack01_01_character_<milliseconds>ms.png")
    images = [Image.open(file).convert("RGB") for file in files]
    times = [int(file.stem.rsplit("_", 1)[1][:-2]) for file in files]
    durations = [max(10, end - start) for start, end in zip(times, times[1:])] + [600]
    output = args.directory / "Attack01_Reference.gif"
    images[0].save(output, save_all=True, append_images=images[1:], duration=durations,
                   loop=0, disposal=2, optimize=False)
    print(json.dumps({"output": str(output), "frames": len(files), "bytes": output.stat().st_size}))


if __name__ == "__main__":
    main()
