import pcbnew

# --- Configuracion ---
TEXT_WIDTH_MM  = 0.8
TEXT_HEIGHT_MM = 0.8
THICKNESS_MM   = 0.15                 # no bajar de 0.15: minimo de linea de silk (JLCPCB)
LAYERS         = [pcbnew.F_SilkS]     # agrega pcbnew.B_SilkS para incluir tambien el dorso
# ---------------------

# Solo estas clases se tocan: se excluye PCB_BARCODE (el QR) y las formas graficas,
# que tienen SetTextSize con otra firma o no aplican.
TEXT_CLASSES = ("PCB_TEXT", "PCB_TEXTBOX", "PCB_FIELD", "FP_TEXT", "FP_TEXTBOX")

board     = pcbnew.GetBoard()
SIZE      = pcbnew.VECTOR2I(pcbnew.FromMM(TEXT_WIDTH_MM), pcbnew.FromMM(TEXT_HEIGHT_MM))
THICKNESS = pcbnew.FromMM(THICKNESS_MM)


def _set(item):
    item.SetTextSize(SIZE)
    item.SetTextThickness(THICKNESS)


def _fix(item):
    if item.GetClass() in TEXT_CLASSES and item.GetLayer() in LAYERS:
        _set(item)
        return 1
    return 0


count = 0
for item in board.GetDrawings():                 # textos y textboxes sueltos del tablero
    count += _fix(item)
for fp in board.GetFootprints():                  # textos dentro de footprints
    for field in (fp.Reference(), fp.Value()):    # referencia y valor (siempre texto)
        if field.GetLayer() in LAYERS:
            _set(field)
            count += 1
    for it in fp.GraphicalItems():                # textos de usuario (excluye QR/barcode)
        count += _fix(it)

pcbnew.Refresh()
print(f"Done - {count} textos de F.Silk ajustados a {TEXT_WIDTH_MM}x{TEXT_HEIGHT_MM} mm. Guarda con Ctrl+S.")
