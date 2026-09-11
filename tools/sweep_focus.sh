#!/bin/bash
# Barre el foco TX en X, escuchando por oido si el nivel sube/baja al pasar
# por tu posicion. Mic/oido FIJO, el foco se mueve.
#
# Uso:
#   ./sweep_focus.sh                    # barrido default -600..600..-600
#   ./sweep_focus.sh 800                # barrido -800..800..-800
#   FZ=500 ./sweep_focus.sh             # foco a 0.5m en vez de 1m
#   STEP=100 HOLD=1.0 ./sweep_focus.sh  # pasos mas finos, hold mas corto

ESP="${ESP:-http://192.168.100.143}"
RANGE="${1:-600}"
STEP="${STEP:-200}"
HOLD="${HOLD:-1.3}"
FY="${FY:--240}"
FZ="${FZ:-1000}"

echo "Foco: fy=$FY fz=$FZ | barrido fx: 0 -> +$RANGE -> -$RANGE -> 0 (paso $STEP mm, hold ${HOLD}s)"
echo "Quedate PARADO en tu posicion. Empezando en 3s..."
sleep 3

seq_fx() {
  seq 0 "$STEP" "$RANGE"
  seq $((RANGE - STEP)) -"$STEP" -"$RANGE" 2>/dev/null
  seq $((-RANGE + STEP)) "$STEP" 0 2>/dev/null
}

for fx in $(seq_fx); do
  echo ">>> fx=$fx mm"
  curl -s -X POST "$ESP/beam" -d "{\"type\":\"tx\",\"fx\":$fx,\"fy\":$FY,\"fz\":$FZ}" > /dev/null
  sleep "$HOLD"
done

# Volver al centro al terminar
curl -s -X POST "$ESP/beam" -d "{\"type\":\"tx\",\"fx\":0,\"fy\":$FY,\"fz\":$FZ}" > /dev/null
echo "Barrido terminado. Foco vuelto a fx=0 (centro)."
