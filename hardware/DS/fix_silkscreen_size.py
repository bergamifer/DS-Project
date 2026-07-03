import pcbnew

# --- Configuración ---
TEXT_WIDTH_MM  = 0.8
TEXT_HEIGHT_MM = 0.8
THICKNESS_MM   = 0.15
# --------------------

board = pcbnew.GetBoard()

SIZE      = pcbnew.VECTOR2I(pcbnew.FromMM(TEXT_WIDTH_MM), pcbnew.FromMM(TEXT_HEIGHT_MM))
THICKNESS = pcbnew.FromMM(THICKNESS_MM)
LAYERS    = [pcbnew.F_SilkS, pcbnew.B_SilkS]

count = 0

for item in board.GetDrawings():
    if item.GetClass() == 'PCB_TEXT' and item.GetLayer() in LAYERS:
        item.SetTextSize(SIZE)
        item.SetTextThickness(THICKNESS)
        count += 1

for fp in board.GetFootprints():
    for text in [fp.Reference(), fp.Value()]:
        if text.GetLayer() in LAYERS:
            text.SetTextSize(SIZE)
            text.SetTextThickness(THICKNESS)
            count += 1

pcbnew.Refresh()
print(f"Done — {count} text items updated.")
