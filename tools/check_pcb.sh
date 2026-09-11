#!/usr/bin/env bash
# check_pcb.sh — impide que salgan gerbers con el cobre desactualizado.
#
# POR QUÉ EXISTE
# --------------
# KiCad NO recalcula los rellenos de zona al plotear: usa el cobre que está
# GUARDADO dentro del .kicad_pcb. Si ese cobre es anterior a las últimas pistas
# y vías, los gerbers salen con planos que nunca abrieron el clearance — y el
# defecto es invisible en pantalla, porque pcbnew te muestra lo que recalculó.
#
# Esto ya arruinó DOS tandas de la DS11 (ver docs/ds11_errata.md) y el
# 2026-09-11 se encontró una TERCERA instancia latente: las vías de I2C1_SCL y
# I2C1_SDA con clearance 0,0000 mm contra el plano de masa. La regla de
# "acordate de apretar B antes de plotear" lleva dos fallas: por eso ahora la
# verifica una máquina.
#
# CÓMO FUNCIONA
# -------------
# Corre el DRC dos veces sobre el mismo archivo:
#   1. tal como está guardado  → lo que se plotearía HOY
#   2. con --refill-zones      → lo que el diseño realmente dice
# Si (1) tiene más violaciones que (2), el cobre guardado está viejo.
#
# USO
#   tools/check_pcb.sh              # verifica y explica
#   tools/check_pcb.sh --fix        # además refillea y guarda
#   tools/check_pcb.sh --install    # lo instala como hook de pre-commit
#
# Sale 0 si está al día, 1 si no.
#
# AL PLOTEAR LOS GERBERS, ADEMÁS: `kicad-cli pcb export gerbers` tiene la
# opción --check-zones ("Check and refill zones if required"), que viene
# APAGADA por default. Usala siempre. La GUI sí avisa antes de plotear; la
# línea de comandos no dice nada, y por eso existe este script.

set -uo pipefail

PCB="${PCB:-hardware/DS/DS.kicad_pcb}"
KICAD_CLI="${KICAD_CLI:-/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli}"
[ -x "$KICAD_CLI" ] || KICAD_CLI="$(command -v kicad-cli 2>/dev/null || true)"

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT" || exit 0

# --install: dejar el hook puesto y salir
if [ "${1:-}" = "--install" ]; then
  mkdir -p .git/hooks
  cat > .git/hooks/pre-commit <<'HOOK'
#!/usr/bin/env bash
# Instalado por tools/check_pcb.sh --install
if git diff --cached --name-only | grep -q '\.kicad_pcb$'; then
  exec "$(git rev-parse --show-toplevel)/tools/check_pcb.sh"
fi
HOOK
  chmod +x .git/hooks/pre-commit
  echo "✅ hook instalado en .git/hooks/pre-commit"
  echo "   Se dispara sólo cuando hay un .kicad_pcb en el commit."
  exit 0
fi

if [ -z "$KICAD_CLI" ] || [ ! -x "$KICAD_CLI" ]; then
  echo "⚠️  check_pcb: no encuentro kicad-cli — salteo la verificación."
  echo "   Definí KICAD_CLI=/ruta/a/kicad-cli si está en otro lado."
  exit 0   # no bloquear a quien no tenga KiCad instalado
fi
[ -f "$PCB" ] || { echo "⚠️  check_pcb: no existe $PCB"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

count() {  # cuenta violaciones en un reporte JSON de DRC
  python3 -c "import json,sys;print(len(json.load(open(sys.argv[1])).get('violations',[])))" "$1" 2>/dev/null || echo -1
}

"$KICAD_CLI" pcb drc --format json -o "$TMP/stored.json" "$PCB" >/dev/null 2>&1
"$KICAD_CLI" pcb drc --format json --refill-zones -o "$TMP/refill.json" "$PCB" >/dev/null 2>&1

S=$(count "$TMP/stored.json"); R=$(count "$TMP/refill.json")
if [ "$S" -lt 0 ] || [ "$R" -lt 0 ]; then
  echo "⚠️  check_pcb: el DRC no produjo un reporte legible — salteo."
  exit 0
fi

if [ "$S" -le "$R" ]; then
  echo "✅ check_pcb: rellenos al día ($S violaciones guardadas / $R recalculadas)."
  exit 0
fi

echo ""
echo "❌ EL COBRE GUARDADO ESTÁ VIEJO — no commitees así."
echo ""
echo "   $PCB"
echo "   con los rellenos guardados : $S violaciones   ← esto es lo que se plotearía"
echo "   con --refill-zones         : $R violaciones   ← esto es lo que el diseño dice"
echo ""
echo "   Las $((S-R)) de diferencia sólo existen porque el relleno quedó desactualizado."
echo "   Las que desaparecen al refillear:"
python3 - "$TMP/stored.json" "$TMP/refill.json" <<'PY'
import json,sys,collections
# Diferencia de MULTICONJUNTO, no de conjunto: el DRC repite violaciones con la
# misma clave (una por capa de la zona), así que un `in set()` cuenta de más.
def key(v):
    it=v.get('items',[]); p=it[0].get('pos',{}) if it else {}
    return (v.get('type'), v.get('description',''), round(p.get('x',0),2), round(p.get('y',0),2))
def load(p): return json.load(open(p)).get('violations',[])
s,r = load(sys.argv[1]), load(sys.argv[2])
rc = collections.Counter(key(v) for v in r)
nets, sobra = collections.Counter(), []
for v in s:
    k=key(v)
    if rc[k] > 0: rc[k] -= 1; continue     # emparejada con una del refill
    sobra.append(v)
    vistas=set()
    for i in v.get('items',[]):
        d=i.get('description','')
        if '[' in d and ']' in d:
            n=d[d.index('[')+1:d.index(']')]
            if n not in vistas and n != 'GND':   # GND es la zona, está en todas
                vistas.add(n); nets[n]+=1
print(f"      ({len(sobra)} violaciones sobran al comparar)")
for n,c in nets.most_common(6):
    print(f"      {c:3}x  en la red {n}")
PY
echo ""
echo "   Arreglo:  tools/check_pcb.sh --fix        (refillea y guarda)"
echo "             o abrir en pcbnew, apretar B, guardar."
echo ""
echo "   Contexto: docs/ds11_errata.md — este mecanismo ya arruinó dos tandas."
echo ""

if [ "${1:-}" = "--fix" ]; then
  echo "→ refilleando y guardando…"
  "$KICAD_CLI" pcb drc --refill-zones --save-board --format json -o "$TMP/after.json" "$PCB" >/dev/null 2>&1
  A=$(count "$TMP/after.json")
  echo "✅ guardado. Ahora quedan $A violaciones (las preexistentes, que no dependen del relleno)."
  echo "   Revisá el diff: tiene que tocar SÓLO líneas '(xy ...)'."
  exit 0
fi
exit 1
