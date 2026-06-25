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
    (6,    "Skel_Palette_first"),  # comment+LoadPalettes
    (89,   "Palette"),
    (145,  "Palette"),
    (165,  "Toolbar"),
    (219,  "Palette"),
    (273,  "Skel"),
    (387,  "Skel"),
    (445,  "Skel"),
    (482,  "Skel"),
    (507,  "Skel"),
    (514,  "Skel"),
    (612,  "BaseIO"),
    (621,  "BaseIO"),
    (649,  "BaseIO"),
    (715,  "BaseIO"),
    (737,  "BaseIO"),
    (759,  "BaseIO"),
    (952,  "BaseIO"),
    (960,  "BaseIO"),
    (969,  "BaseIO"),
    (980,  "BaseIO"),
    (988,  "BaseIO"),
    (998,  "BaseIO"),
    (1033, "BaseIO"),
    (1059, "BaseIO"),
    (1079, "BaseIO"),
    (1091, "BaseIO"),
    (1099, "Skel"),
    (1114, "Skel"),
    (1124, "Toolbar"),
    (1337, "Skel"),
    (1353, "Skel"),
    (1364, "Skel"),
    (1405, "Skel"),
    (1455, "Parts"),
    (1535, "Parts"),
    (1618, "Parts"),
    (1659, "Parts"),
    (1697, "Parts"),   # NMS_CategoryIcon (static)
    (1730, "Parts"),
    (1795, "Toolbar"),
    (1836, "Parts"),
    (1844, "Parts"),
    (1880, "Parts"),
    (1886, "Parts"),
    (1892, "Parts"),
    (1976, "Parts"),
    (2037, "Parts"),
    (2102, "Curve"),
    (2177, "Curve"),
    (2207, "Curve"),
    (2223, "Curve"),
    (2316, "Data"),
    (2332, "Data"),
    (2347, "Data"),
    (2388, "Data"),
    (2416, "Data"),
    (2439, "Data"),   # NMS_UserFile (static)
    (2444, "Data"),
    (2468, "Data"),
    (2487, "Data"),
    (2495, "Data"),
    (2505, "Data"),
    (2517, "Data"),
    (2526, "Parts"),  # MovePartToCategory
    (2542, "Data"),
    (2602, "SaveManager"),
    (2628, "SaveManager"),
    (2640, "SaveManager"),
    (2661, "SaveManager"),
    (2705, "SaveManager"),
    (2767, "SaveManager"),
    (2853, "SaveManager"),
    (2870, "SaveManager"),
    (2883, "SaveManager"),
    (2898, "SaveManager"),
    (2923, "SaveManager"),
    (2956, "RightPanel"),
    (2961, "RightPanel"),
    (2970, "RightPanel"),
    (2979, "RightPanel"),
    (3026, "RightPanel"),
    (3051, "RightPanel"),
    (3060, "RightPanel"),
    (3072, "RightPanel"),
    (3094, "RightPanel"),
    (3100, "RightPanel"),
    (3146, "RightPanel"),
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

# Skeleton keeps its own canonical header. SNMSBuilderUI.h идёт ПЕРВЫМ —
# требование IWYU UBT (matching header первым в .cpp).
SKEL_HEADER = ("#include \"SNMSBuilderUI.h\"" + EOL +
               "#include \"SNMSBuilderUI_Internal.h\"" + EOL + EOL +
               "#define LOCTEXT_NAMESPACE \"NMSBuilder\"" + EOL + EOL)

for key, fname in FILES.items():
    path = os.path.join(DST_DIR, fname)
    head = SKEL_HEADER if key == "Skel" else HEADER
    content = head + "".join(buckets[key]).rstrip("\r\n") + FOOTER
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content)
    print(f"{fname}: {len(buckets[key])} lines body")

print("done")
