#!/usr/bin/env bash
# Compila un .cpp del cubo con arm-none-eabi-gcc y produce .elf/.bin/.hex.
# Uso: ./build.sh ../CuboSerial.cpp   (o ../CuboLed8x8x8.cpp, ../pruebas/PruebaEstatica.cpp ...)
# Requisitos: gcc-arm-none-eabi y los archivos CMSIS de STMicroelectronics (licencia Apache 2.0),
# que no se incluyen en este repositorio:
#   git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_f7.git cmsis/dev
#   git clone --depth 1 https://github.com/STMicroelectronics/cmsis_core.git      cmsis/core
# De cmsis_device_f7 salen los encabezados del STM32F746, el arranque (tabla de vectores, Reset_Handler)
# y SystemInit. El enlazador STM32F746ZGTx_FLASH.ld si es propio.
set -e
cd "$(dirname "$0")"
SRC="${1:?falta el .cpp}"
NAME="$(basename "${SRC%.*}")"
DEV="${CMSIS_DEV:-cmsis/dev}"
CORE="${CMSIS_CORE:-cmsis/core}"
STARTUP="$DEV/Source/Templates/gcc/startup_stm32f746xx.s"
SYSTEM="$DEV/Source/Templates/system_stm32f7xx.c"
F="-mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard -DSTM32F746xx -I$DEV/Include -I$CORE/CMSIS/Core/Include -O1 -ffunction-sections -fdata-sections"
mkdir -p build
arm-none-eabi-gcc $F -c "$STARTUP" -o build/startup.o
arm-none-eabi-gcc $F -c "$SYSTEM" -o build/system.o
arm-none-eabi-g++ $F -fno-exceptions -fno-rtti -Wall -Wextra -c "$SRC" -o "build/$NAME.o"
arm-none-eabi-g++ $F -fno-exceptions -fno-rtti -specs=nano.specs -specs=nosys.specs -Wl,--gc-sections \
  -T STM32F746ZGTx_FLASH.ld build/startup.o build/system.o "build/$NAME.o" -o "build/$NAME.elf"
arm-none-eabi-objcopy -O binary "build/$NAME.elf" "build/$NAME.bin"
arm-none-eabi-objcopy -O ihex   "build/$NAME.elf" "build/$NAME.hex"
arm-none-eabi-size "build/$NAME.elf"
echo "listo: build/$NAME.bin (arrastrar a NODE_F746ZG)"
