"""Generate a tiny RP2040-friendly on-device story model.

This is not the ESP32-S3 28.9M-parameter PLE model. The RP2040 has only 264KB of
SRAM and no ESP-IDF flash mmap/PSRAM, so this generator builds a compact byte
language model that lives in flash as C arrays and runs directly from Arduino.

The model is a deterministic byte-level backoff LM:
  - primary: top continuations for a 2-byte context
  - fallback: top continuations for a 1-byte context
  - final fallback: global byte distribution

It is small, dependency-free, and produces TinyStories-like text on a Pico/Pico W.
Pass a text file path to train it on your own corpus:

  python src/gen_rp2040_assets.py data/tinystories_slice.txt
"""

from __future__ import annotations

import collections
import os
import sys
from dataclasses import dataclass


HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "firmware", "rp2040_tinylm", "rp2040_model.h")

DEFAULT_CORPUS = """
Once upon a time there was a small robot named Pip. Pip lived in a bright room
with a blue chair, a red cup, and a window full of morning light. Every day Pip
listened to the rain, counted the stars on the wallpaper, and made up little
stories for the sleepy moon.

One day Pip found a tiny key under the rug. The key was warm and gold. "What do
you open?" asked Pip. The key did not talk, but it pointed with a little shine
toward the old box on the shelf.

Inside the box was a folded map. The map showed a garden, a hill, and a door in
the sky. Pip packed a button, a cookie, and a very brave smile. Then Pip rolled
out into the garden.

The flowers whispered, "Be kind and the path will stay bright." Pip said thank
you and gave the cookie to a hungry bird. The bird sang a song so sweet that the
grass began to glow.

At the top of the hill, Pip saw the sky door. It was not locked. The tiny key was
only a reminder: the best doors open when you are gentle. Pip stepped through and
found a room full of stars, each one waiting for a story.

Pip told the stars about the blue chair, the red cup, the kind flowers, and the
bird who liked cookies. The stars laughed softly. When Pip came home, the window
was full of moonlight, and the little robot felt big inside.

Once upon a time a girl named Nia had a yellow kite. The kite wanted to fly
higher than the clouds, but the wind was shy. Nia waited and waited. Then she
said, "Wind, you can start small." The wind made one tiny puff. The kite danced.
Soon the wind felt brave, and the kite climbed into the blue sky.

A little bear lost his hat in the forest. He asked the pine tree, the river, and
the round gray stone. Nobody had seen it. At last a squirrel waved from a branch.
"Your hat is my boat," said the squirrel. The bear laughed and let the squirrel
sail it across a puddle before taking it home.

Milo had a wooden train that could not go fast. His friends had shiny cars and
rockets, but Milo loved the slow train. It stopped for ants, flowers, and every
interesting cloud. By dinner time Milo had seen more wonders than anyone.

The moon dropped a silver crumb into Lila's pocket. She planted it in a pot and
watered it with songs. A small ladder grew. It did not reach the moon, but it did
reach the top shelf where the jam was hiding.
"""


ALLOWED = set(b"\n .,!?;:'\"-ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")


@dataclass(frozen=True)
class Entry:
    ctx: tuple[int, ...]
    choices: list[tuple[int, int]]


def clean_text(text: str) -> bytes:
    data = text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8", errors="ignore")
    cleaned = bytearray()
    last_space = False
    for b in data:
        if b not in ALLOWED:
            b = 32
        is_space = b in (9, 10, 32)
        if is_space:
            if not last_space:
                cleaned.append(32)
            last_space = True
        else:
            cleaned.append(b)
            last_space = False
    return bytes(cleaned).strip() + b" "


def top_counts(counter: collections.Counter[int], limit: int) -> list[tuple[int, int]]:
    items = counter.most_common(limit)
    if not items:
        return [(32, 1)]
    return [(byte, min(count, 65535)) for byte, count in items]


def build_entries(data: bytes, order: int, max_contexts: int, max_choices: int) -> list[Entry]:
    counts: dict[tuple[int, ...], collections.Counter[int]] = collections.defaultdict(collections.Counter)
    for i in range(order, len(data)):
        ctx = tuple(data[i - order : i])
        counts[ctx][data[i]] += 1

    ranked = sorted(counts.items(), key=lambda kv: sum(kv[1].values()), reverse=True)
    entries = [Entry(ctx, top_counts(counter, max_choices)) for ctx, counter in ranked[:max_contexts]]
    entries.sort(key=lambda e: e.ctx)
    return entries


def c_array(name: str, ctype: str, values: list[int], per_line: int = 16) -> str:
    lines = [f"static const {ctype} {name}[] = {{"]
    for i in range(0, len(values), per_line):
        lines.append("  " + ", ".join(str(v) for v in values[i : i + per_line]) + ",")
    lines.append("};")
    return "\n".join(lines)


def flatten_entries(entries: list[Entry], order: int) -> tuple[list[int], list[int], list[int], list[int]]:
    ctxs: list[int] = []
    offsets: list[int] = []
    bytes_out: list[int] = []
    weights: list[int] = []
    for entry in entries:
        ctxs.extend(entry.ctx)
        offsets.append(len(bytes_out))
        for b, w in entry.choices:
            bytes_out.append(b)
            weights.append(w)
    offsets.append(len(bytes_out))
    if order == 1:
        assert len(ctxs) == len(entries)
    else:
        assert len(ctxs) == len(entries) * order
    return ctxs, offsets, bytes_out, weights


def main() -> int:
    if len(sys.argv) > 1:
        with open(sys.argv[1], "r", encoding="utf-8", errors="ignore") as f:
            text = f.read()
    else:
        text = DEFAULT_CORPUS

    data = clean_text(text)
    if len(data) < 512:
        raise SystemExit("corpus is too small; provide at least a few paragraphs")

    global_counts = collections.Counter(data)
    uni = top_counts(global_counts, 48)
    one = build_entries(data, order=1, max_contexts=96, max_choices=8)
    two = build_entries(data, order=2, max_contexts=384, max_choices=6)

    one_ctx, one_off, one_bytes, one_weights = flatten_entries(one, 1)
    two_ctx, two_off, two_bytes, two_weights = flatten_entries(two, 2)
    uni_bytes = [b for b, _ in uni]
    uni_weights = [w for _, w in uni]

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Generated by src/gen_rp2040_assets.py. Do not edit by hand.\n")
        f.write("#ifndef RP2040_MODEL_H\n#define RP2040_MODEL_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define RP2040_MODEL_CORPUS_BYTES {len(data)}\n")
        f.write(f"#define RP2040_MODEL_ONE_COUNT {len(one)}\n")
        f.write(f"#define RP2040_MODEL_TWO_COUNT {len(two)}\n")
        f.write(f"#define RP2040_MODEL_UNI_COUNT {len(uni)}\n\n")
        f.write(c_array("LM_ONE_CTX", "uint8_t", one_ctx) + "\n\n")
        f.write(c_array("LM_ONE_OFF", "uint16_t", one_off) + "\n\n")
        f.write(c_array("LM_ONE_BYTES", "uint8_t", one_bytes) + "\n\n")
        f.write(c_array("LM_ONE_WEIGHTS", "uint16_t", one_weights) + "\n\n")
        f.write(c_array("LM_TWO_CTX", "uint8_t", two_ctx) + "\n\n")
        f.write(c_array("LM_TWO_OFF", "uint16_t", two_off) + "\n\n")
        f.write(c_array("LM_TWO_BYTES", "uint8_t", two_bytes) + "\n\n")
        f.write(c_array("LM_TWO_WEIGHTS", "uint16_t", two_weights) + "\n\n")
        f.write(c_array("LM_UNI_BYTES", "uint8_t", uni_bytes) + "\n\n")
        f.write(c_array("LM_UNI_WEIGHTS", "uint16_t", uni_weights) + "\n\n")
        f.write("#endif\n")

    total_bytes = (
        len(one_ctx)
        + len(one_off) * 2
        + len(one_bytes)
        + len(one_weights) * 2
        + len(two_ctx)
        + len(two_off) * 2
        + len(two_bytes)
        + len(two_weights) * 2
        + len(uni_bytes)
        + len(uni_weights) * 2
    )
    print(f"wrote {OUT}")
    print(f"corpus={len(data)} bytes, flash table ~= {total_bytes} bytes")
    print(f"contexts: order2={len(two)}, order1={len(one)}, unigram={len(uni)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
