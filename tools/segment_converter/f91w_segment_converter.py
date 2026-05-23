#!/usr/bin/env python3
"""
Rasterise Casio F-91W LCD segments from the Casio-F-91W demo SVG into f91w_segments.h.

Copyright (c) 2025 MikeDX — MIT — https://github.com/mikedx/casiof91w-rlcd

Requires Inkscape (or cairosvg) plus: pip install -r requirements.txt
See README.md in this folder.
"""

import argparse
import copy
import json
import math
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

try:
    from lxml import etree
except ImportError:
    sys.exit("Missing: pip install lxml")

try:
    from PIL import Image
except ImportError:
    sys.exit("Missing: pip install Pillow")

DIGIT_POSITIONS = ["hour_1", "hour_2", "minute_1", "minute_2",
                   "second_1", "second_2", "day_1", "day_2",
                   "mode_1", "mode_2"]
DIGIT_SEGMENTS  = list("ABCDEFG")
MODE_EXTRAS     = {"mode_1": ["H"], "mode_2": ["H", "I"]}
# LCD-area indicators only (alarm/24h/on/off groups are case silkscreen, off the glass)
INDICATORS      = ["lap", "dot-top", "dot-bottom", "timeSignalOnMark",
                   "timeMode24", "timeMode12"]
COMPOSITE_INDICATORS = {
    "alarmOnMark": [
        "alarmOnMark", "alarmOnMark_1", "alarmOnMark_2", "alarmOnMark_3",
        "alarmOnMark_4", "alarmOnMark_5",
    ],
}

FIRMWARE_INDICATOR_MAP = {
    "WATCH_INDICATOR_SIGNAL": "timeSignalOnMark",
    "WATCH_INDICATOR_BELL":   "alarmOnMark",
    "WATCH_INDICATOR_PM":     "timeMode12",
    "WATCH_INDICATOR_24H":    "timeMode24",
    "WATCH_INDICATOR_LAP":    "lap",
}
FIRMWARE_POSITION_MAP = {
    0:"mode_1", 1:"mode_2", 2:"day_1",    3:"day_2",
    4:"hour_1", 5:"hour_2", 6:"minute_1", 7:"minute_2",
    8:"second_1", 9:"second_2",
}

SVG_W=1480.0; SVG_H=1311.0
SCR_X=397.5;  SCR_Y=520.0; SCR_W=683.0; SCR_H=334.0
MARGIN=6

def get_svg(html_path):
    txt = html_path.read_text(encoding="utf-8")
    m = re.search(r'(<svg\b[^>]*id="CasioF91WSVG".*?</svg>)', txt, re.DOTALL)
    if not m:
        m = re.search(r'(<svg\b.*?</svg>)', txt, re.DOTALL)
    if not m:
        sys.exit("SVG not found")
    return m.group(1).encode("utf-8")

def parse(svg_bytes):
    return etree.fromstring(svg_bytes, etree.XMLParser(remove_comments=True, recover=True))

def all_seg_ids():
    ids = set()
    for pos in DIGIT_POSITIONS:
        ids.add(pos)
        for s in DIGIT_SEGMENTS + MODE_EXTRAS.get(pos,[]):
            ids.add(f"{pos}_{s}")
    for ind in INDICATORS:
        ids.add(ind)
    for e in ["alarm-on-off-24h","alarmOnMark","alarmOnMark_1","alarmOnMark_2",
              "alarmOnMark_3","alarmOnMark_4","alarmOnMark_5","timeSignalOnMark",
              "timeMode12","timeMode24","SoundOnOff","SoundOn","SoundOff",
              "lap","24h","dot-top","dot-bottom","dots","on","off","light","screen"]:
        ids.add(e)
    return ids

def _parent_id_map(root):
    pmap = {}
    for parent in root.iter():
        pid = parent.get("id")
        for child in parent:
            cid = child.get("id")
            if cid and pid:
                pmap[cid] = pid
    return pmap

def _ancestor_ids(show, pmap):
    """Keep ancestor <g> nodes visible (mode_1 lives inside mode_2 in the SVG)."""
    anc = set()
    for sid in show:
        cur = sid
        while cur in pmap:
            cur = pmap[cur]
            if cur:
                anc.add(cur)
    return anc

def show_only(root, show):
    root = copy.deepcopy(root)
    all_ids = all_seg_ids()
    pmap = _parent_id_map(root)
    ancestors = _ancestor_ids(show, pmap)
    always_hide = {
        "case-outer","case-middle","case-inner",
        "button-a","button-c","button-l",
        "buttonA","buttonB","buttonC","buttonL",
        "outline-outer","outline-inner","screen-outline",
        "screen","light","layers",
    }
    if show & {"alarm","24h"}:
        show = show | {"alarm-on-off-24h"}
    if show & {"dot-top","dot-bottom"}:
        show = show | {"dots"}
    if show & {"alarmOnMark","alarmOnMark_1","alarmOnMark_2","alarmOnMark_3","alarmOnMark_4","alarmOnMark_5"}:
        show = show | {"alarmOnMark"}
    for el in root.iter():
        eid = el.get("id")
        if not eid: continue
        if eid in always_hide:
            el.set("display","none")
        elif eid in show or eid in ancestors:
            if eid in show:
                el.set("display","inline")
                sty = re.sub(r'display\s*:\s*none','display:inline',el.get("style",""))
                el.set("style", sty)
                for child in el.iter():
                    if child.get("fill","").lower() in ("white","#ffffff","#fff"):
                        child.set("fill", "#304246")
            else:
                el.set("display","inline")
        elif eid in all_ids:
            el.set("display","none")
    return etree.tostring(root, encoding="unicode").encode("utf-8")

def render_raw(svg_bytes, inkscape, tmp):
    svg_f = os.path.join(tmp,"seg.svg")
    png_f = os.path.join(tmp,"seg.png")
    with open(svg_f,"wb") as f:
        f.write(svg_bytes)
    rw,rh = int(SVG_W*4), int(SVG_H*4)
    if inkscape and os.path.exists(inkscape):
        r = subprocess.run([inkscape, svg_f,
            f"--export-width={rw}", f"--export-height={rh}",
            f"--export-filename={png_f}", "--export-area-page"],
            capture_output=True, timeout=60)
        if r.returncode != 0:
            raise RuntimeError(r.stderr.decode()[:200])
    else:
        import cairosvg, io
        Image.open(io.BytesIO(cairosvg.svg2png(
            bytestring=svg_bytes,output_width=rw,output_height=rh))).save(png_f)
    return Image.open(png_f).convert("RGBA")

def crop_to_screen(img):
    sc = img.width / SVG_W
    x0,y0 = int(SCR_X*sc), int(SCR_Y*sc)
    x1,y1 = int((SCR_X+SCR_W)*sc), int((SCR_Y+SCR_H)*sc)
    return img.crop((x0,y0,x1,y1))

def to_grey_white(img_rgba, out_w, out_h):
    bg = Image.new("RGBA", img_rgba.size, (255,255,255,255))
    bg.paste(img_rgba, mask=img_rgba.split()[3])
    return bg.convert("L").resize((out_w, out_h), Image.LANCZOS)

def to_1bit(img_l, t):
    return img_l.point(lambda p: 0 if p < t else 255).convert("1")

def find_bbox(img_1, w, h):
    px = img_1.load()
    x0,y0,x1,y1 = w,h,0,0
    found = False
    for y in range(MARGIN, h-MARGIN):
        for x in range(MARGIN, w-MARGIN):
            v = px[x,y]
            if (v==0 if isinstance(v,int) else v[0]==0):
                if x<x0: x0=x
                if y<y0: y0=y
                if x>x1: x1=x
                if y>y1: y1=y
                found = True
    if not found: return None
    return (max(0,x0-1), max(0,y0-1),
            min(w-1,x1+1)-max(0,x0-1)+1,
            min(h-1,y1+1)-max(0,y0-1)+1)

def pack(img_1, box):
    bx,by,bw,bh = box
    stride = math.ceil(bw/8)
    buf = bytearray(stride*bh)
    px = img_1.load()
    for row in range(bh):
        for col in range(bw):
            v = px[bx+col, by+row]
            if (v==0 if isinstance(v,int) else v[0]==0):
                buf[row*stride+col//8] |= (1<<(7-col%8))
    return bytes(buf)

def csym(name):
    return "F91W_"+re.sub(r'[^A-Za-z0-9]','_',name).upper()

def make_header(segs, dw, dh):
    out = [
        "/*",
        " * Casio F-91W on Waveshare ESP32-S3 RLCD 4.2\" — LCD segment bitmaps",
        " *",
        " * Copyright (c) 2025 MikeDX",
        " * SPDX-License-Identifier: MIT",
        " *",
        " * https://github.com/mikedx/casiof91w-rlcd",
        " *",
        " * AUTO-GENERATED — see tools/segment_converter/. Regenerate; do not hand-edit.",
        " */",
        "#pragma once","#include <stdint.h>","#include <string.h>","",
        f"#define F91W_W {dw}",f"#define F91W_H {dh}",
        f"#define F91W_FB {dw*dh//8}","",
        "typedef struct{uint16_t x,y,w,h,stride,len;const uint8_t*data;}F91WSeg;","",
    ]
    for name in sorted(segs):
        s=segs[name]; sym=csym(name); x,y,w,h=s["bbox"]; d=s["data"]
        out.append(f"// {name} ({x},{y}) {w}x{h} {len(d)}B")
        out.append(f"static const uint8_t {sym}_D[{len(d)}]={{")
        row=[]
        for i,b in enumerate(d):
            row.append(f"0x{b:02X}")
            if len(row)==16 or i==len(d)-1:
                out.append("    "+",".join(row)+","); row=[]
        out.append("};"); out.append("")
    out+=["// descriptors",""]
    for name in sorted(segs):
        s=segs[name]; sym=csym(name); x,y,w,h=s["bbox"]
        out.append(f"static const F91WSeg {sym}={{{x},{y},{w},{h},{math.ceil(w/8)},{len(s['data'])},{sym}_D}};")
    out+=["","// F91W_DIGIT[pos 0-9][seg A=0..G=6]",
          "static const F91WSeg*const F91W_DIGIT[10][7]={"]
    for fp,sp in FIRMWARE_POSITION_MAP.items():
        ptrs=[f"&{csym(sp+'_'+s)}" if sp+'_'+s in segs else "NULL" for s in DIGIT_SEGMENTS]
        out.append(f"    {{{','.join(ptrs)}}},")
    out+=["};","","// F91W_IND[0=SIGNAL,1=BELL,2=PM,3=24H,4=LAP]",
          "static const F91WSeg*const F91W_IND[5]={"]
    for fw,sid in FIRMWARE_INDICATOR_MAP.items():
        out.append(f"    {'&'+csym(sid) if sid in segs else 'NULL'},  // {fw}")
    out+=["};","",
        "static inline void f91w_on(uint8_t*fb,const F91WSeg*s){",
        "    for(int r=0;r<s->h;r++)for(int c=0;c<s->w;c++){",
        "        if(!(s->data[r*s->stride+c/8]&(1<<(7-c%8))))continue;",
        "        int x=s->x+c,y=s->y+r;",
        f"        int iy={dh-1}-y;",
        f"        fb[(x/2)*{dh//4}+iy/4]&=~(1<<(7-(iy%4)*2-x%2));",
        "    }",
        "}",
        f"static inline void f91w_clear(uint8_t*fb){{memset(fb,0xFF,{dw*dh//8});}}",""]
    return "\n".join(out)

def main():
    ap = argparse.ArgumentParser(
        description="Build lib/f91w_segments/f91w_segments.h from Casio-F-91W demo HTML/SVG.")
    ap.add_argument("--input",     required=True,
                    help="Path to Casio-F-91W demo/index.html (clone separately)")
    ap.add_argument("--output",    required=True,
                    help="Output directory (writes f91w_segments.h and segments.json)")
    ap.add_argument("--width",     type=int, default=400)
    ap.add_argument("--height",    type=int, default=300)
    ap.add_argument("--threshold", type=int, default=100)
    ap.add_argument("--inkscape",  default="/usr/bin/inkscape")
    ap.add_argument("--preview",   action="store_true",
                    help="Write preview/ and preview_raw/ PNGs under --output")
    a = ap.parse_args()

    outdir = Path(a.output)
    prev   = outdir/"preview"
    raw    = outdir/"preview_raw"
    outdir.mkdir(parents=True, exist_ok=True)
    if a.preview:
        prev.mkdir(exist_ok=True)
        raw.mkdir(exist_ok=True)

    root = parse(get_svg(Path(a.input)))

    jobs = []
    for pos in DIGIT_POSITIONS:
        for seg in DIGIT_SEGMENTS + MODE_EXTRAS.get(pos,[]):
            jobs.append((f"{pos}_{seg}", {f"{pos}_{seg}", pos}))
    for name, members in COMPOSITE_INDICATORS.items():
        jobs.append((name, set(members)))
    for ind in INDICATORS:
        jobs.append((ind, {ind}))

    segs = {}
    with tempfile.TemporaryDirectory() as tmp:
        for i,(name,show) in enumerate(jobs):
            print(f"[{i+1:3d}/{len(jobs)}] {name:<30}", end="", flush=True)
            try:
                svg      = show_only(root, show)
                full     = render_raw(svg, a.inkscape, tmp)
                cropped  = crop_to_screen(full)
                grey     = to_grey_white(cropped, a.width, a.height)
                img_1    = to_1bit(grey, a.threshold)
                box      = find_bbox(img_1, a.width, a.height)

                if a.preview:
                    grey.save(str(raw/f"{name.replace('/','_')}.png"))
                    img_1.convert("L").save(str(prev/f"{name.replace('/','_')}_1bit.png"))

                if box is None:
                    print("no pixels")
                    continue

                x,y,w,h = box
                data = pack(img_1, box)
                print(f"({x:3},{y:3}) {w:3}x{h:3}  {len(data):4}B")
                segs[name] = {"bbox": box, "data": data}

                if a.preview:
                    crop = img_1.crop((x,y,x+w,y+h))
                    z = max(1, min(8, 200//max(w,h,1)))
                    crop.resize((w*z,h*z),Image.NEAREST).convert("L").save(
                        str(prev/f"{name.replace('/','_')}_crop.png"))

            except Exception as e:
                print(f"ERROR: {e}")
                import traceback; traceback.print_exc()

    total_b = sum(len(s["data"]) for s in segs.values())
    print(f"\n{len(segs)} segments  {total_b}B ({total_b/1024:.1f}KB)")
    hp = outdir/"f91w_segments.h"
    hp.write_text(make_header(segs, a.width, a.height))
    print(f"Header: {hp} ({hp.stat().st_size//1024}KB)")
    (outdir/"segments.json").write_text(
        json.dumps({n:{"bbox":list(v["bbox"]),"bytes":len(v["data"])}
                    for n,v in segs.items()},indent=2))
    print("Done. Copy f91w_segments.h to lib/f91w_segments/ — see README.md.")

if __name__=="__main__":
    main()
