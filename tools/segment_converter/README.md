# Segment converter

Optional build tool for this firmware. Rasterises each LCD segment from the
[Casio-F-91W](https://github.com/alexisphilip/Casio-F-91W) demo SVG into
`lib/f91w_segments/f91w_segments.h`.

That Casio project is **not** part of this repo — clone it separately when you
need to regenerate artwork. We do **not** use any JavaScript from it; only
`demo/index.html` (embedded SVG) as the segment source.

Copyright © 2025 MikeDX. MIT License — see [../../LICENSE](../../LICENSE).

## Requirements

- Python 3.9+
- [Inkscape](https://inkscape.org/) on your `PATH`, or `pip install cairosvg` as a fallback
- `pip install -r requirements.txt`

## Usage

From this directory:

```bash
pip install -r requirements.txt

python3 f91w_segment_converter.py \
  --input  /path/to/Casio-F-91W/demo/index.html \
  --output ./out \
  --threshold 100 \
  --preview

cp out/f91w_segments.h ../../lib/f91w_segments/
```

`--preview` writes PNG checks under `out/preview/` and `out/preview_raw/`.
Add `out/` to your local ignore list if you like (generated output).

## Notes

- Default LCD size is 400×300 (Waveshare ST7305).
- Threshold 100 works well for the demo SVG; tweak if segments look thin or fat.
- On macOS, Inkscape is often `/Applications/Inkscape.app/Contents/MacOS/inkscape`.
