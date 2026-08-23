#!/bin/bash
# Genera el PDF de la guía de operación a partir de docs/guia_operacion.md.
#
# Existe como script y no como un PDF suelto en el repo porque la guía se
# actualiza: un PDF a mano se desincroniza del texto en dos semanas y nadie se
# entera. Esto se vuelve a correr y listo.
#
#   ./docs/pdf/hacer_pdf.sh
#
# Salida: docs/pdf/Guia_DS11.pdf
#
# Cadena: pandoc (markdown → HTML) + Chrome headless (HTML → PDF).
# Chrome y no LaTeX a propósito: la guía usa ⚠️ ✅ 💡 para marcar lo que puede
# romper el equipo, y LaTeX no los dibuja. En una guía de operación el símbolo
# de peligro importa más que el numerito de página que LaTeX daría gratis.
set -euo pipefail

AQUI="$(cd "$(dirname "$0")" && pwd)"
DOCS="$(dirname "$AQUI")"
FUENTE="$DOCS/guia_operacion.md"
SALIDA="$AQUI/Guia_DS11.pdf"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
[ -x "$CHROME" ] || { echo "No encuentro Chrome en $CHROME" >&2; exit 1; }
command -v pandoc >/dev/null || { echo "Falta pandoc: brew install pandoc" >&2; exit 1; }
[ -f "$FUENTE" ] || { echo "No encuentro $FUENTE" >&2; exit 1; }

# La primera línea del .md es el título y la portada ya lo pone: sin el tail se
# repetiría dos veces, una en la portada y otra arriba del cuerpo.
tail -n +2 "$FUENTE" > "$TMP/cuerpo.md"

# El CSS va INCRUSTADO, no enlazado: Chrome headless imprime la página apenas
# la carga, y una hoja externa puede no haber llegado a aplicarse. Además el
# HTML intermedio queda autocontenido, útil para revisarlo en el navegador.
{ echo '<style>'; cat "$AQUI/estilo.css"; echo '</style>'; } > "$TMP/estilo_head.html"

pandoc "$TMP/cuerpo.md" \
  --from=gfm \
  --to=html5 \
  --standalone \
  --toc --toc-depth=2 \
  --template="$AQUI/plantilla.html" \
  --include-in-header="$TMP/estilo_head.html" \
  --metadata title="Guía de uso del DS11" \
  --metadata subtitulo="Conexionado, encendido, operación y carga de programa. Para alguien que va a manipular el equipo por primera vez: no hace falta saber de audio ni de electrónica." \
  --metadata fecha="$(date +%d/%m/%Y)" \
  -o "$TMP/guia.html"

# --no-pdf-header-footer: sin el encabezado con la URL y la fecha que Chrome
# mete por omisión, que en una guía impresa sólo ensucia.
"$CHROME" \
  --headless \
  --disable-gpu \
  --no-pdf-header-footer \
  --print-to-pdf="$SALIDA" \
  --virtual-time-budget=10000 \
  "file://$TMP/guia.html" 2>/dev/null

[ -s "$SALIDA" ] || { echo "Chrome no generó el PDF" >&2; exit 1; }
echo "✅ $SALIDA  ($(du -h "$SALIDA" | cut -f1))"
