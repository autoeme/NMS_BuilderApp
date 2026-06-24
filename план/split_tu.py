#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Разнесение методов SNMSBuilderUI по нескольким TU (Часть C плана).
Читает текущий SNMSBuilderUI.cpp, режет по границам методов, раскладывает
по целевым .cpp. Каждый TU получает свой include-блок + LOCTEXT_NAMESPACE.
"""
import io, os

SRC = r"C:\Users\User\Documents\Unreal Projects\NMS_BuilderApp\Source\NMS_BuilderApp\SNMSBuilderUI.cpp"
DST_DIR = os.path.dirname(SRC)
EOL = "\r\n"

# (start_line_1based, target)
CHUNKS = [
    (5,    "Skel_Palette_first"),  # comment+LoadPalettes
    (88,   "Palette"),
    (144,  "Palette"),
    (164,  "Toolbar"),
    (218,  "Palette"),
    (272,  "Skel"),
    (367,  "Skel"),
    (425,  "Skel"),
    (462,  "Skel"),
    (487,  "Skel"),
    (494,  "Skel"),
    (592,  "BaseIO"),
    (601,  "BaseIO"),
    (629,  "BaseIO"),
    (695,  "BaseIO"),
    (717,  "BaseIO"),
    (739,  "BaseIO"),
    (932,  "BaseIO"),
    (940,  "BaseIO"),
    (949,  "BaseIO"),
    (960,  "BaseIO"),
    (968,  "BaseIO"),
    (978,  "BaseIO"),
    (1013, "BaseIO"),
    (1039, "BaseIO"),
    (1059, "BaseIO"),
    (1071, "BaseIO"),
    (1079, "Skel"),
    (1094, "Skel"),
    (1104, "Toolbar"),
    (1317, "Skel"),
    (1333, "Skel"),
    (1344, "Skel"),
    (1377, "Skel"),
    (1427, "Parts"),
    (1507, "Parts"),
    (1590, "Parts"),
    (1631, "Parts"),
    (1669, "Parts"),   # NMS_CategoryIcon (static)
    (1702, "Parts"),
    (1767, "Toolbar"),
    (1808, "Parts"),
    (1816, "Parts"),
    (1852, "Parts"),
    (1858, "Parts"),
    (1864, "Parts"),
    (1948, "Parts"),
    (2009, "Parts"),
    (2085, "Curve"),
    (2160, "Curve"),
    (2190, "Curve"),
    (2206, "Curve"),
    (2299, "Data"),
    (2315, "Data"),
    (2330, "Data"),
    (2371, "Data"),
    (2399, "Data"),
    (2422, "Data"),   # NMS_UserFile (static)
    (2427, "Data"),
    (2451, "Data"),
    (2470, "Data"),
    (2478, "Data"),
    (2488, "Data"),
    (2500, "Data"),
    (2509, "Parts"),  # MovePartToCategory
    (2525, "Data"),
    (2585, "SaveManager"),
    (2611, "SaveManager"),
    (2623, "SaveManager"),
    (2644, "SaveManager"),
    (2688, "SaveManager"),
    (2750, "SaveManager"),
    (2836, "SaveManager"),
    (2853, "SaveManager"),
    (2866, "SaveManager"),
    (2881, "SaveManager"),
    (2906, "SaveManager"),
    (2939, "RightPanel"),
    (2944, "RightPanel"),
    (2953, "RightPanel"),
    (2962, "RightPanel"),
    (3009, "RightPanel"),
    (3034, "RightPanel"),
    (3043, "RightPanel"),
    (3055, "RightPanel"),
    (3077, "RightPanel"),
    (3083, "RightPanel"),
    (3129, "RightPanel"),
]

FILES = {
    "Skel":       "SNMSBuilderUI.cpp",
    "Parts":      "SNMSBuilderUI.Parts.cpp",
    "Data":       "SNMSBuilderUI.Data.cpp",
    "Curve":      "SNMSBuilderUI.Curve.cpp",
    "BaseIO":     "SNMSBuilderUI.BaseIO.cpp",
    "SaveManager":"SNMSBuilderUI.SaveManager.cpp",
    "Toolbar":    "SNMSBuilderUI.Toolbar.cpp",
    "Palette":    "SNMSBuilderUI.Palette.cpp",
    "RightPanel": "SNMSBuilderUI.RightPanel.cpp",
}

def is_loctext_line(line):
    s = line.strip()
    return s.startswith("#define LOCTEXT_NAMESPACE") or s.startswith("#undef LOCTEXT_NAMESPACE")

with io.open(SRC, "r", encoding="utf-8", newline="") as f:
    lines = f.readlines()  # keep CRLF
N = len(lines)

# Build chunk ranges [start, end] (1-based inclusive)
starts = [c[0] for c in CHUNKS]
ranges = []
for i, (start, tgt) in enumerate(CHUNKS):
    end = (CHUNKS[i+1][0] - 1) if i+1 < len(CHUNKS) else N
    ranges.append((start, end, tgt))

# The "Skel_Palette_first" chunk = comment(5) + LoadPalettes body -> Palette
# but its leading content (line5..) goes to Palette.
buckets = {k: [] for k in FILES}
for (start, end, tgt) in ranges:
    body = lines[start-1:end]  # 0-based slice
    body = [ln for ln in body if not is_loctext_line(ln)]
    key = "Palette" if tgt == "Skel_Palette_first" else tgt
    buckets[key].extend(body)

HEADER = ("// Часть класса SNMSBuilderUI (разнесение по TU, Часть C плана)." + EOL +
          "// Включения общие для всех TU класса — см. SNMSBuilderUI_Internal.h." + EOL +
          "#include \"SNMSBuilderUI_Internal.h\"" + EOL + EOL +
          "#define LOCTEXT_NAMESPACE \"NMSBuilder\"" + EOL + EOL)
FOOTER = (EOL + "#undef LOCTEXT_NAMESPACE" + EOL)

# Skeleton keeps its own canonical header (already had include+define).
SKEL_HEADER = ("#include \"SNMSBuilderUI_Internal.h\"" + EOL + EOL +
               "#define LOCTEXT_NAMESPACE \"NMSBuilder\"" + EOL + EOL)

for key, fname in FILES.items():
    path = os.path.join(DST_DIR, fname)
    head = SKEL_HEADER if key == "Skel" else HEADER
    content = head + "".join(buckets[key]).rstrip("\r\n") + FOOTER
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content)
    print(f"{fname}: {len(buckets[key])} lines body")

print("done")
