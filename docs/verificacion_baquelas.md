# Verificación del cableado de las baquelas 1 y 2

Viernes 11/09/2026. Se hace **antes** de volver a conectar la Nucleo. Es el peldaño 2 de la escalera de
pruebas (de lo más pequeño a lo más grande), desarrollado punto por punto.

> Nota posterior (18/09): este procedimiento se escribió con el contrato de pines anterior, en el que el
> MOSFET 1 era el piso de abajo. Se midió que el MOSFET 1 es el piso de arriba; el orden vigente está en
> [`contrato_pines.md`](contrato_pines.md).

Regla del día: cada casilla de "Medido" se llena. El primer valor que no cuadre es el fallo, y ahí se para.

## Fase A — En frío (sin fuente, sin Nucleo, chips FUERA de los zócalos)

Multímetro en continuidad (pito) para los cables, en ohmios para lo que pasa por resistencia.
Sacar el 74HCT245 del banco 1 y el de pisos: los chips falsean las medidas y se pueden dañar.

Numeración del zócalo: muesca arriba, pin 1 arriba a la izquierda, se cuenta hacia abajo por la izquierda
(1 a 10) y se sigue de abajo hacia arriba por la derecha (11 a 20).

### A.1 Rieles de alimentación (las dos baquelas)

| Punto | Esperado | Baquela 1 | Baquela 2 |
|---|---|---|---|
| Positivo del jack → pin 20 de cada zócalo (los 5 en la baquela 1) | pito | | |
| Positivo del jack → pin 1 (DIR) de cada zócalo | pito | | |
| Negativo del jack → pin 10 (GND) de cada zócalo | pito | | |
| Negativo del jack → pin 19 (/OE) de cada zócalo | pito | | |
| Positivo del jack → emisor de cada 2N2907 (pata izquierda, cara plana hacia ti) | pito | | |
| Riel de 5 V ↔ riel de GND | **sin pito**, resistencia alta | | |

Si el pin 1, el 19 o el 20 de algún zócalo no está cableado, ese es el fallo más probable de anoche: un chip
sin VCC, o con /OE al aire, no saca nada y no da ningún síntoma visible. Se corrige con un puente ahí mismo.

Antes de enchufar la fuente al jack: medir el plug de la fuente en voltaje continuo, centro contra anillo.
Debe dar +5 V con el centro positivo. Nunca 9 V a la baquela.

### A.2 Header de entrada → pines A del chip (los 4 bancos de cada baquela)

Una punta en el pin del header de entrada, la otra recorriendo el zócalo hasta que pite. Se **anota lo que
salga**, no se asume.

| Header de entrada, pin | Pin del zócalo esperado | Señal | b1 b1 | b1 b2 | b1 b3 | b1 b4 | b2 b1 | b2 b2 | b2 b3 | b2 b4 |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 2 | A1 = x0 | | | | | | | | |
| 2 | 3 | A2 = x1 | | | | | | | | |
| 3 | 4 | A3 = x2 | | | | | | | | |
| 4 | 5 | A4 = x3 | | | | | | | | |
| 5 | 6 | A5 = x4 | | | | | | | | |
| 6 | 7 | A6 = x5 | | | | | | | | |
| 7 | 8 | A7 = x6 | | | | | | | | |
| 8 | 9 | A8 = x7 | | | | | | | | |

Si un banco sale invertido (header 1 → pin 9) o desordenado, **no se resuelda**: se corrige la fila
correspondiente de `COL_PIN` / `COL_PUERTO` en `CuboLed8x8x8.cpp` y en `PruebaEstatica.cpp`, y se anota en
`contrato_pines.md`. Si el header de abajo resulta ser la salida y el de arriba la entrada, se intercambian
los dos conectores; eso sí es cableado externo.

### A.3 Pines B del chip → base del 2N2907 (por la resistencia de 3.9 kΩ)

Multímetro en ohmios. El pin 18 (B1, columna x0) debe dar ≈3.9 kΩ contra la base (pata central) de un
2N2907, y el pin 11 (B8, columna x7) contra la base de otro.

| Pin del zócalo | Columna | Base esperada | Medido (≈3.9 kΩ) |
|---|---|---|---|
| 18 | x0 | PNP 1 | |
| 17 | x1 | PNP 2 | |
| 16 | x2 | PNP 3 | |
| 15 | x3 | PNP 4 | |
| 14 | x4 | PNP 5 | |
| 13 | x5 | PNP 6 | |
| 12 | x6 | PNP 7 | |
| 11 | x7 | PNP 8 | |

Basta hacerlo completo en un banco de cada baquela; en los otros tres, los extremos (pin 18 y pin 11).

### A.4 Colector del 2N2907 → header de salida (por la resistencia de 220 Ω)

Del colector (pata derecha) al pin del header de salida donde entra la tira: ≈220 Ω. El orden del header de
salida es el orden físico de los hilos de la tira, y eso se confirma de verdad en la fase C.

### A.5 Transistores vivos y en la posición correcta (prueba de diodo)

Multímetro en diodo, con los chips todavía fuera. Punta roja en el riel de 5 V (o en el emisor), punta negra
en la base de cada 2N2907: **0.6 a 0.8 V**. Al revés: abierto. Si un transistor da 0 V en los dos sentidos
está en corto; si da abierto en los dos, está montado al revés o mal soldado.

### A.6 Pisos, solo baquela 1

| Comprobación | Esperado | Medido |
|---|---|---|
| Header de pisos pin z+1 → zócalo de pisos pin 2+z (8 líneas) | pito | |
| Zócalo de pisos pin 18−z → compuerta del MOSFET z+1 (pata izquierda, letras hacia ti) | pito | |
| Fuente de cada IRLZ44N (pata derecha) → GND | pito | |
| Drenador de cada IRLZ44N (pata central y aleta) → su borne de tierra del cubo | pito | |
| Aleta de un MOSFET ↔ aleta del vecino | **sin pito** | |
| Compuerta ↔ fuente de cada MOSFET | sin pito (abierto) | |

Ojo con el orden: MOSFET 1 = z0 = piso de abajo. Si la salida B1 del chip de pisos va al MOSFET 8 en vez del
1, el cubo se ve invertido de arriba abajo y se corrige en `PISO_PIN`.

### A.7 Enlace entre baquelas

El cable de 5 V y el de GND que salen de la baquela 1 hacia la baquela 2 deben pitar contra los rieles
correspondientes de la baquela 2, y el GND de las dos baquelas debe pitar contra el GND de la Nucleo cuando
esté todo conectado. **GND común siempre**, aunque la Nucleo esté alimentada por USB.

## Fase B — Con fuente, sin Nucleo (chips puestos)

1. Fuente al jack, sin nada más conectado. Mirar el consumo si la fuente lo muestra, y tocar los chips: en
   reposo son decenas de miliamperios y nada calienta. Si algo calienta, apagar de inmediato: hay un corto.
2. Multímetro en voltaje continuo sobre cada zócalo con chip: **pin 20 = 5 V, pin 1 = 5 V, pin 19 = 0 V,
   pin 10 = 0 V**. Estas cuatro medidas por chip son las que descartan la sospecha número uno.
3. Prueba de una columna, a mano: **todos** los pines del header de entrada del banco 1 puenteados a 5 V
   (columnas apagadas) y el pin 1 de ese header pasado a GND. Todos los pines del header de pisos a GND,
   menos el 1, que va a 5 V. Debe encender **un led fijo**: columna 1, fila y0, piso de abajo.

   Las entradas del 74HCT245 no se dejan al aire: flotando, las salidas quedan indeterminadas y la prueba no
   significa nada.

   Si no enciende, medir en este orden y parar en el primero que falle: pin 18 del chip del banco 1 (0 V),
   base del PNP de la columna 1 (≈4.3 V), colector (≈4.8 V), pin 18 del chip de pisos (5 V), compuerta del
   MOSFET 1 (5 V), pata central del MOSFET 1 (≈0.05 V).

## Fase C — Con la Nucleo y `PruebaEstatica.bin` (confirma el orden real de los hilos)

Esta fase confirma la correspondencia completa, banco por banco. Grabar el `.bin` compilado de `PruebaEstatica` arrastrándolo
a `NODE_F746ZG`, conectar el header de entrada del banco que se esté probando, su tira y las 8 tierras, y
avanzar con el pulsador azul B1:

- **Estado 1** (leds de la tarjeta 001): un solo led, columna 1 de la fila, piso de abajo. Confirma toda la
  cadena de ese banco.
- **Estado 2** (010): los 8 leds de la fila en el piso de abajo. Si encienden los 8, el banco está bien; si
  falta uno, el fallo está en esa columna. Una tira invertida no se nota aquí, se nota en el estado 1: el led
  encendido es el de la columna 8 en vez del de la 1 → se gira el conector 180° o se corrige `COL_PIN`.
- **Estado 3** (011): columna vertical de 8 leds, tenues. Confirma el orden de las 8 tierras y de los MOSFET.
  Si el led sube salteado o invertido, el orden de las tierras no es el del contrato.
- **Estado 5** (101): plano y0 completo con multiplexado real, a brillo normal.
- **Estado 6** (110): barrido de todo, para cuando ya estén las dos baquelas.

Se repiten los estados 1 y 2 para los bancos 2, 3 y 4 de la baquela 1, y luego para los cuatro bancos de la
baquela 2. Un banco no se da por bueno hasta que sus 8 leds encienden en el estado 2.

## Qué confirma cada fase

| Fase | Qué queda confirmado |
|---|---|
| A | Que los chips reciben alimentación, que entradas y salidas están donde se cree, y el orden interno de cada banco |
| B | Que la cadena chip → PNP → columna → led → MOSFET → GND conduce, sin depender del firmware |
| C | Que el mapa de pines del firmware coincide con el cableado físico, columna por columna y piso por piso |
