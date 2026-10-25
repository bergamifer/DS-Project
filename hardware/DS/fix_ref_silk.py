"""Achica los designadores (REF**) de la serigrafia a 0.6 mm, sin tocar los grandes.

Los que ya miden SKIP_DESDE_MM o mas (en alto o en ancho) se dejan como estan: son
rotulos que tienen que verse (J1, TP5, "24 V" si es una referencia, etc.).
No toca valores, textos sueltos ni el QR.

USO desde KiCad (Editor de PCB > Herramientas > Consola de scripting):
    exec(open('/Users/bergamifer/STM32CubeIDE/DS-Firmware/hardware/DS/fix_ref_silk.py').read())
  Primero corre en simulacro (APLICAR = False): imprime que cambiaria y no modifica nada.
  Para aplicar, pone APLICAR = True abajo (o:  APLICAR = True  antes del exec), y Ctrl+S.
  Ctrl+Z deshace todo de una vez.

USO desde la terminal (sobre una COPIA, nunca sobre DS.kicad_pcb):
    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
        fix_ref_silk.py copia.kicad_pcb [--aplicar]
"""
import sys
import pcbnew

# --- Configuracion ---
TEXTO_MM        = 0.6      # alto y ancho nuevos
GROSOR_MM       = 0.15     # no bajar de 0.15: minimo de linea de silk (JLCPCB)
SKIP_DESDE_MM   = 2.0      # si alto o ancho >= esto, no se toca
CAPAS           = [pcbnew.F_SilkS, pcbnew.B_SilkS]
# ---------------------

def main():
    """Todo adentro de una funcion: un `return` no tumba nada, un sys.exit() en la consola de KiCad si."""
    aplicar = bool(globals().get("APLICAR", False))
    board = pcbnew.GetBoard()                 # con el editor abierto devuelve el PCB; sin GUI, None
    archivo = None
    if board is None:                         # modo terminal
        args = [a for a in sys.argv[1:] if not a.startswith("--")]
        if not args or not args[0].endswith(".kicad_pcb"):
            print(__doc__)
            return
        archivo = args[0]
        board = pcbnew.LoadBoard(archivo)
        aplicar = "--aplicar" in sys.argv

    nuevo = pcbnew.VECTOR2I(pcbnew.FromMM(TEXTO_MM), pcbnew.FromMM(TEXTO_MM))
    grosor = pcbnew.FromMM(GROSOR_MM)
    limite = pcbnew.FromMM(SKIP_DESDE_MM)

    cambiados, grandes, ya_ok, ocultos = [], [], 0, 0
    for fp in board.GetFootprints():
        ref = fp.Reference()
        if ref.GetLayer() not in CAPAS:
            continue
        if not ref.IsVisible():
            ocultos += 1
            continue
        s = ref.GetTextSize()
        if max(s.x, s.y) >= limite:
            grandes.append(f"{ref.GetText()} ({pcbnew.ToMM(s.x):.2f}x{pcbnew.ToMM(s.y):.2f})")
        elif s == nuevo and ref.GetTextThickness() == grosor:
            ya_ok += 1
        else:
            cambiados.append(f"{ref.GetText()} ({pcbnew.ToMM(s.x):.2f}x{pcbnew.ToMM(s.y):.2f})")
            if aplicar:
                ref.SetTextSize(nuevo)
                ref.SetTextThickness(grosor)

    if aplicar:
        if archivo:
            board.Save(archivo)
        else:
            pcbnew.Refresh()

    modo = "APLICADO" if aplicar else "SIMULACRO (no se modifico nada)"
    print(f"{modo}: {len(cambiados)} refs a {TEXTO_MM} mm, {ya_ok} ya estaban, "
          f"{len(grandes)} grandes intactos, {ocultos} ocultos ignorados.")
    print("Intactos por grandes:", ", ".join(grandes) or "ninguno")
    if not aplicar:
        print("Cambiaria:", ", ".join(cambiados[:40]) + (" ..." if len(cambiados) > 40 else ""))
    elif not archivo:
        print("Guarda con Ctrl+S.")


main()
