-Al iniciar el proyecto, si pregunta, pone Debug

-Para compilar el proyecto:
cd /Users/bergamifer/STM32CubeIDE/DS-Firmware/firmware && rm -rf build && cmake --preset Debug && cmake --build build/Debug

El .elf queda en build/Debug/DS-Firmware.elf

-Iniciar servidor web
cd webserver
./run.sh

-Para detener el servidor
Presioná Ctrl+C en la terminal donde está corriendo