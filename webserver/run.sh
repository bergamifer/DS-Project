#!/bin/bash
# Script para ejecutar el servidor DS Beamforming Control

cd "$(dirname "$0")"

# Verificar si existe el entorno virtual
if [ ! -d "venv" ]; then
    echo "Creando entorno virtual..."
    python3 -m venv venv
    source venv/bin/activate
    echo "Instalando dependencias..."
    pip install -r requirements.txt
else
    source venv/bin/activate
fi

echo ""
echo "========================================"
echo "  DS Beamforming Control Server"
echo "========================================"
echo ""

python3 app.py
