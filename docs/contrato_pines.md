# Contrato de pines — versión 2 (jueves 10/09/2026, 22:00)

Regla: `cubo[z][y]` bit `x`. Cada tira de 8 hilos del cubo es una fila y (un plano z-x). Dentro de la tira, la
columna x es el número de William en la madera menos uno (columna 1 = x0 … columna 8 = x7). Cada tierra es un
piso z y va a la pata central (drenador) del MOSFET 8−z: **el MOSFET 1 es el piso de arriba** (z7) y el MOSFET 8 el
de abajo (z0). Corregido el 18/09: con el orden anterior la lluvia subía.

Fila y → 74HCT245: entrada A(x+1) = pin 2+x, salida B(x+1) = pin 18−x. Piso z → chip de pisos, A(8−z) = pin 9−z.

## Columnas

| Fila | Baquela · banco | x0 | x1 | x2 | x3 | x4 | x5 | x6 | x7 |
|---|---|---|---|---|---|---|---|---|---|
| y=0 | 1 · banco 1 | PD0 | PD1 | PD2 | PD3 | PD4 | PD5 | PD6 | PD7 |
| y=1 | 1 · banco 2 | PG0 | PG1 | PG2 | PG3 | PG4 | PG5 | PG8 | PG9 |
| y=2 | 1 · banco 3 | PC6 | PC7 | PC8 | PC9 | PC10 | PC11 | PC12 | PB15 |
| y=3 | 1 · banco 4 | PB1 | PB2 | PB3 | PB4 | PB5 | PB6 | PB10 | PB11 |
| y=4 | 2 · banco 1 | PE0 | PE1 | PE2 | PE3 | PE4 | PE5 | PE6 | PE7 |
| y=5 | 2 · banco 2 | PE8 | PE9 | PE10 | PE11 | PE12 | PE13 | PE14 | PE15 |
| y=6 | 2 · banco 3 | PF0 | PF1 | PF2 | PF3 | PF4 | PF5 | PF6 | PF7 |
| y=7 | 2 · banco 4 | PF8 | PF9 | PF10 | PF11 | PF12 | PF13 | PF14 | PF15 |

## Pisos (chip de pisos en la baquela 1)

| Piso | z0 | z1 | z2 | z3 | z4 | z5 | z6 | z7 |
|---|---|---|---|---|---|---|---|---|
| MOSFET (número de William) | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
| Entrada del chip | A8 pin 9 | A7 pin 8 | A6 pin 7 | A5 pin 6 | A4 pin 5 | A3 pin 4 | A2 pin 3 | A1 pin 2 |
| Nucleo | PG12 | PG10 | PD15 | PD14 | PD13 | PD12 | PD11 | PD10 |
| Posición | abajo | | | | | | | arriba |

## Cada 74HCT245 (DIP-20, muesca arriba, pin 1 a la izquierda)

| Pin | Nombre | Va a |
|---|---|---|
| 1 | DIR | +5 V (dirección A → B) |
| 2–9 | A1 … A8 | Los 8 pines de la Nucleo de esa fila, en orden x0 … x7 |
| 10 | GND | GND común |
| 11–18 | B8 … B1 | Al revés: B1 es el pin 18. Salida x0 = pin 18, x7 = pin 11 |
| 19 | /OE | GND (salidas habilitadas) |
| 20 | VCC | +5 V, con 100 nF a GND pegado al chip |

## LCD 16x2 (HD44780, 4 bits, solo escritura)

VSS GND · VDD 5 V · VO cursor del potenciómetro 10 kΩ (extremos a 5 V y GND) · RS PA4 · RW GND · E PA15 ·
D0–D3 al aire · D4 PB12 · D5 PG14 · D6 PG15 · D7 PA6 · A 220 Ω → 5 V · K GND.

## Otros

Pulsador B1 = PC13 (3.3 V al presionar; pull-down; flanco de subida; EXTI13). Leds LD1 PB0, LD2 PB7, LD3 PB14.
Polaridad: columna enciende con 0 en el pin; piso enciende con 1 en el pin.
Serial con el simulador (`CuboSerial.cpp`): USART3, **PD8 = TX, PD9 = RX**, función alternativa 7, ya puenteados al
ST-LINK en la tarjeta: viaja por el cable USB de CN1 como puerto COM virtual. PD8 y PD9 no se cablean a nada.

Alimentación: Nucleo por USB (JP3 en U5V). Fuente 5 V / 2 A al jack de la baquela 1 → VCC y DIR de los chips,
emisores de los PNP, baquela 2 (5 V y GND por cable). GND común obligatorio. 470–1000 µF en el riel de 5 V y
100 nF por chip. Opción para la entrega, solo cuando ya funcione: JP3 en E5V y 5 V del jack al pin E5V del morpho
(CN11 pin 6), fuente antes que USB.

## Componentes y sus patas

2N2907 (TO-92, cara plana hacia ti, patas abajo): E – B – C. Emisor a 5 V, base por 3.9 kΩ desde B del 74HCT245,
colector → 220 Ω → columna (ánodos).
IRLZ44N (TO-220, letras hacia ti): G – D – S. Compuerta desde el 74HCT245 de pisos, drenador (pata central y aleta)
a la tierra del piso (cátodos), fuente a GND. Las aletas no deben tocarse entre sí.
