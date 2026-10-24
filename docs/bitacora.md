# Bitácora del cubo

## Jueves 10/09/2026

- 17:30 Plan de integración escrito. Contrato de pines versión 1 (baquela 1 = y4–y7).
- 20:50 William trae el cubo terminado y las dos baquelas soldadas (diseño de referencia del curso completo: 9 zócalos,
  64 × 2N2907 con resistencias, 8 IRLZ44N, jack de 5 V). Fotos en `fotos/`.
- 21:30 Firmware `CuboLed8x8x8.cpp` versión 1 compilado. Se decide que el frente del cubo es un plano z-x.
- 22:00 Se conecta la tira "y0" al banco 1 de la baquela 1. Contrato **versión 2**: baquela 1 = y0–y3, baquela 2 =
  y4–y7; columna x = número de William − 1; tierra z → MOSFET z+1. Firmware versión 2 (+ modo prueba de cableado)
  compilado y grabado por arrastre a NODE_F746ZG. Verificado: LD1 verde y **los leds de la tarjeta cambian con el
  B1** (firmware, SysTick y EXTI corriendo).
- 22:30 Intento de alimentar la Nucleo desde el jack: JP3 a E5V sin fuente → FAIL.TXT. JP3 devuelto a U5V.
- 23:00 Con banco 1 + tira y0 + chip de pisos + 8 tierras + GND común + fuente: **no encendió nada**. No se midió
  nada con el multímetro. Se desarmó y cada uno se fue a casa; Javier se llevó el cubo y las baquelas.

## Viernes 11/09/2026

- Mañana: Javier en casa con el cubo. Objetivo: subir la escalera de pruebas (de lo más pequeño a lo más grande) desde el peldaño 1,
  midiendo en cada uno. Entrega a las 8 pm.
- **Prueba con batería (9 V + 180 Ω, sin Nucleo ni baquelas): el cubo funciona.** + en un hilo de la tira
  (columna) y − en un cable de tierra (piso) enciende un led; moviendo el − por las 8 tierras el led sube y baja
  piso por piso. Conclusión: polaridad del cubo = la del diseño (ánodos en columnas, cátodos en pisos), tierras en
  orden. **El cubo queda descartado como causa del fallo de anoche.** Sospechosos: alimentación de los chips
  (pines 1, 19, 20), header de entrada vs salida en los bancos, compuerta/drenador de los MOSFET, GND común.
  Siguiente: peldaño 2 (baquela con puentes a GND / 5 V, sin Nucleo). Nunca 9 V a la baquela.
- **Peldaño 2, baquela 1 con puentes (sin Nucleo). CAUSA ENCONTRADA.** Medidas: patas 20 y 1 = 5.1 V, patas 10 y 19
  = 0 V en los chips (alimentación bien). Pin izquierdo de out1 a GND → pata 11 del chip 1 a 0 V (out1 está cableado
  izquierda = A8, derecha = A1). Pin izquierdo de out5 azul a 5 V → pata 18 del chip 5 a 5 V y compuerta del MOSFET 1
  a 5 V (out5 azul: izquierda = A1). Los 8 pines de in1: uno a 5 V, siete a 0 V (cadena de columna completa).
  **MOSFET 1: compuerta 5 V, pata del centro 2.9 V, pata derecha 2.9 V → la pista de las fuentes (patas derechas)
  de los MOSFET no está unida al negativo del jack: flota.** Con la tierra del piso de abajo puesta directo al
  negativo del jack, el led enciende. Arreglo: unir la pista de las fuentes de los 8 MOSFET al negativo del jack
  (antes: continuidad de esa pista contra el positivo, no debe pitar).
- Nota de cableado: las "in azul" de los MOSFET son las patas del centro directamente (no hay conector aparte).
- **Confirmado con puente: fuentes de los MOSFET → negativo del jack, y el led (x0, y0, z0) enciende a través del
  MOSFET 1 y se queda fijo.** Al poner el puente la fuente se cayó a 0 V un momento (protección); desconectar y
  volver a conectar la restauró a 5 V y no se volvió a caer. Pendiente: dejar el puente soldado, verificar que las
  patas derechas de los 8 MOSFET pitan entre sí (una sola pista → un solo cable), y que esa pista NO pita con el
  positivo del jack. Peldaño 2 cerrado. Siguiente: peldaño 3 (Nucleo + PruebaEstatica, estado 1 = ese mismo led).
- Orientación de las regletas (medida): out1 pin izquierdo = A8 (x7), pin derecho = A1 (x0). out5 azul pin izquierdo
  = A1 (MOSFET 1, z0). Al cablear la Nucleo a out1 hay que contar desde la derecha: PD0 → pin derecho.

## Medidas pendientes (llenar)

| Punto | Esperado | Medido |
|---|---|---|
| Organizadora, estado 1 de PruebaEstatica: PD0 | 0 V | |
| Organizadora, estado 1: PD10 | 3.3 V | |
| Chip banco 1 pin 20 / pin 1 | 5 V / 5 V | |
| Chip banco 1 pin 19 / pin 10 | 0 V / 0 V | |
| Chip pisos pin 20 / pin 1 / pin 19 | 5 V / 5 V / 0 V | |
| Chip banco 1 pin 2 (A1) con PD0 conectado, estado 1 | 0 V | |
| Chip banco 1 pin 18 (B1) | 0 V | |
| PNP columna 1: base / colector | ≈4.3 V / ≈4.8 V | |
| Chip pisos pin 2 (A1) / pin 18 (B1) | 3.3 V / 5 V | |
| MOSFET 1: compuerta / pata central | 5 V / ≈0.05 V | |
| Led (x0, y0, z0) | encendido fijo | |

## Sabado 12/09/2026 (entrega y cierre del taller)

- Se solda parcialmente / se mejoran los contactos de la organizadora. Aparecen leds aleatorios y ningun estado
  en negro: **causa = entradas de los 74HCT245 al aire** porque los pines morpho estaban apoyados, no soldados.
  Una entrada CMOS sin conectar flota, coge ruido y su salida cambia sola.
- Se escribe `firmware/PruebaPisos.cpp`: un solo piso encendido a la vez, los 64 leds de ese piso, fijo y a
  brillo pleno (con varios pisos a la vez los 8 leds de una columna comparten una resistencia y se ven
  desparejos; eso no es falla, es fisica). Estados 1..8 = piso 1..8, 9 = recorrido automatico, 10 = cubo barrido.
- Tras mejorar contactos y cablear la baquela 2 (falta un 74HCT245 para el ultimo banco, fila y7): **el estado 0
  por fin apaga todo el cubo.** Responden de forma confiable solo dos o tres pisos, y cuales cambian entre pruebas.
- LCD montada en protoboard con potenciometro de contraste. Llega a mostrar los cuadros negros del renglon de
  arriba (alimentada y con contraste bien) pero **nunca se inicializa**: se queda sin texto. Sospechosos, en orden:
  la inicializacion ocurre una sola vez al arrancar y la pantalla se energiza despues; RW; y el umbral de 3.3 V
  contra los 3.5 V que pide un HD44780 a 5 V. Se escribe `firmware/PruebaLCD.cpp` (reintenta la inicializacion
  cada 2 s) pero no alcanza a probarse.
- Se escriben `firmware/CuboTresPisos.cpp` y `firmware/CuboVivo.cpp`: cinco secuencias como figuras de 8x8 en el
  plano horizontal (PLANOS, ANILLOS, LETRAS con texto corriendo, ESPIRAL, ONDA), repetidas en los pisos activos,
  con el barrido recorriendo solo esos pisos (mas brillo y mas refresco). CuboVivo agrega **selector de pisos en
  vivo**: pulsacion larga del B1 entra al modo pisos y cada pulsacion corta mueve el par activo.
- **Causa raiz confirmada: el contacto entre la placa organizadora de pines y los conectores morpho de la Nucleo.**
  Los pines estaban apoyados, no soldados; la placa organizadora tiene que ir soldada a la tarjeta.
- Nuevo objetivo para el viernes 18/09: el cubo fisico funcionando con las secuencias del simulador, es decir, el
  enlace serial de dos vias que estaba fuera de alcance antes de la entrega.

## Plan para el viernes 18/09

1. Soldar los 80 pines del morpho (64 columnas + 8 pisos + 6 de la LCD + 2 tierras). Calentar pin y anillo juntos
   2 s y despues acercar el estano al punto de union, nunca a la punta del cautin. Es la causa raiz y sin eso
   nada de lo demas es estable.
2. Verificar con `PruebaPisos`: estado 0 negro, estados 1 a 8 un piso completo cada uno, sin cambiar al mover la
   mesa. Anotar que piso falla y por que (compuerta del MOSFET, pata del chip de pisos, cable).
3. Conseguir el 74HCT245 que falta para el banco 4 de la baquela 2 (fila y7).
4. LCD con `PruebaLCD`. Si sigue en cuadros: RW a tierra, tierra comun, y VDD a 3.3 V.
5. `CuboLed8x8x8_v2.bin` con las cinco secuencias y la LCD.
6. Enlace serial simulador <-> cubo (Web Serial en Chrome, USART3 en PD8/PD9, tramas de 66 bytes 0xA5 0x5A +
   64 + XOR, 115200 baudios, reenganche por 5 ms de silencio; B1 fisico de vuelta al simulador).

## Viernes 18/09/2026

- Con el morpho soldado, **el cubo corre las cinco secuencias** de `CuboLed8x8x8_v2.bin`: planos por x, y y z, lluvia,
  cubo, espiral y onda.
- Punto 6 del plan: **enlace serial escrito**. `firmware/CuboSerial.cpp` (la versión 2 + USART3 en PD8/PD9 por el
  ST-LINK) y botón **Conectar cubo** en el simulador (Web Serial). Tramas `A5 5A TIPO DATOS XOR` (imagen 68 bytes,
  texto de la LCD 37 bytes; el diseño decía 66 porque no contaba el byte de tipo ni el XOR), 115200 baudios,
  reenganche por 5 ms de silencio, B1 físico de vuelta al simulador, estado cada segundo. Compilado con Keil desde
  la línea de comandos: 0 errores, 0 advertencias. Prueba cruzada del protocolo en software: bien. **Pendiente:
  probarlo en la tarjeta.**
- Si al conectar el simulador dice "el cubo no contesta": ¿está grabado `CuboSerial.bin`?, ¿se eligió el puerto del
  ST-LINK?, ¿hay otro programa con el COM abierto?
- **Tarde: falla nueva.** Con `CuboLed8x8x8_v2.bin` y con `CuboSerial.bin` (mismo binario v2 que antes andaba), un
  plano x-z, el tercero de atrás hacia adelante (Javier: y=2), se queda encendido. En el barrido de PLANOS por x se
  apaga, salvo cuando el plano que se mueve llega al extremo derecho (x7), donde se enciende un momento.
  `PruebaPisos` "funciona perfecto", pero **nunca deja un piso encendido con columnas apagadas**, así que no puede ver
  un banco que no apaga. Lectura: el plano prende cuando las 8 entradas de su 74HCT245 están en 3.3 V (todas las
  columnas apagadas) y se arregla cuando una de x0..x6 baja a 0 V → falla de ese banco (tierra o 5 V del chip, pata
  sin contacto en el zócalo, chip dañado o un cable de entrada), no del firmware: el firmware escribe cada bit por
  separado y no puede hacer que una columna dependa de otra. Se escribe `firmware/PruebaApagado.cpp`
  (compilado a `PruebaApagado.bin`): piso 1 fijo con columnas apagadas o una sola por fila, para medir con el multímetro.
- `PruebaApagado`, resultado: estado 0 todo apagado; **estado 1 prende la fila entera** (tercera de atrás hacia
  adelante, y=2) en el piso de prueba; estados 2 a 8 la línea avanza y la fila mala se apaga; **estado 9 (x7, PB15)
  la fila vuelve a prender entera**; estado 10 prende todo. Confirma: el chip del banco 3 de la baquela 1 funciona
  si alguna de PC6..PC12 está en 0 V, y falla si no; PB15 no lo rescata → la pata 9 (A8) no recibe el 0 V. Las patas
  9 (A8) y 10 (tierra) son vecinas, en la esquina de abajo a la izquierda del chip. Pendiente: continuidad y medidas.
- Nota: Javier ve el piso de prueba (z0, MOSFET 1) como el **de arriba**, y en `PruebaPisos` el recorrido z0→z7 le va
  del techo a la base. El contrato dice z0 = abajo. Por confirmar: si la lluvia sube, los pisos están invertidos y se
  corrige en `PISO_PUERTO`/`PISO_PIN` (en todos los archivos), no en los cables.
- **Pisos invertidos, confirmado**: con `CuboSerial` la lluvia sube. El MOSFET 1 es el piso de arriba. Se invierte
  `PISO_PUERTO`/`PISO_PIN` en los 7 firmwares (z0 = abajo = PG12 = MOSFET 8 … z7 = arriba = PD10 = MOSFET 1), en la
  pestaña Cables del simulador y en el contrato. Recompilados con Keil, 0 errores y 0 advertencias: `CuboSerial`,
  `PruebaApagado`, `PruebaPisos`, `PruebaEstatica`, `CuboTresPisos`, `CuboVivo`. `CuboLed8x8x8_v2.bin` se deja como
  el de la entrega (anterior a la corrección); lo reemplaza `CuboSerial.bin`.
- Con el simulador conectado, las letras de ECCI viajan de x0 a x7 en el cubo en vez de y7 a y0 como en pantalla:
  el cubo queda girado o en espejo en el plano horizontal respecto a la vista Frente. Las descripciones de x/y no
  cuadran entre sí, así que se agregan al simulador casillas de orientación (x↔y, invertir x, y, z) que solo
  transforman la imagen que se manda al cubo. Pendiente: saber qué casillas quedan marcadas para pasarlo a las tablas.
- **SNAKE jugable con el teclado** (simulador). Problemas del esquema anterior: ejes fijos (↑ = y+, que con la cámara
  por defecto acercaba la culebra), la dirección cambiaba al instante (↑ y ← rápido = reversa encima del cuerpo) y al
  morir reiniciaba sola. Ahora: teclas relativas a la cámara redondeada al lado más cercano (↑/W aleja, ↓/S acerca,
  ←/A, →/D, E/Espacio sube, Q/Shift baja; se lee `e.code`, así WASD es igual en cualquier teclado), cola de 2 giros
  que ignora reversas, arranque enrollada que espera la primera tecla, velocidad 378 ms que baja 3 % por comida hasta
  168 ms, P pausa (y pausa sola si la ventana pierde el foco), Enter otra partida con 1 s de gracia, récord en el
  navegador, LCD con largo y récord (también en la LCD física), cabeza celeste, comida verde y sombras en el piso de
  la vista 3D, vista de juego de frente y desde arriba. Probado en el navegador integrado: mapeo por lado de cámara,
  cola, reversas, muerte, reinicio y 385 ms por paso a 60 cuadros/s.
- **Observación de Javier: sin la fuente de 5 V el cubo funciona bien; con la fuente aparecen planos y columnas
  pegados.** Explicación: sin fuente, los pines del STM que están en 3.3 V (la mayoría: 3.3 V = columna apagada)
  alimentan el riel de "5 V" de las baquelas por los diodos de protección de las entradas de los 74HCT245 (entrada →
  diodo → pata 20). El riel queda en unos 2.5–3 V; con eso funcionan los chips, los IRLZ44N (nivel lógico) y los leds
  blancos (tenues). Con la fuente, los emisores de los 2N2907 están firmes en 5 V: una salida que no sostiene ~4.5 V
  (chip con la tierra o los 5 V mal conectados) deja conducir al transistor. Sin fuente, emisor y salida cuelgan del
  mismo riel débil: no hay margen y la falla no se ve. **La fuente no es la causa: es la que deja ver la falla.**
  No dejarlo andando sin fuente (corriente por los diodos de protección, 20 mA máximo por entrada en la hoja de datos).
  Medidas pendientes: pata 20 sin fuente (2.5–3 V esperado; si da ~5 V hay un cable llevando el USB al riel) y pata 10
  del chip que falla contra el negativo del jack, con fuente, en el estado 1 de `PruebaApagado` (0 V esperado).
- **Después de correrlo un buen rato sin fuente, con la fuente aparecen MÁS planos y columnas pegados.** Sospechas, en
  orden: (1) enclavamiento (latch-up) o daño de entradas en uno o más 74HCT245: trabajaron con las entradas por encima
  de su alimentación, que es justo lo que lo dispara; el enclavamiento no se suelta hasta quitar toda la energía y con
  la fuente de 2 A puede calentar y quemar el chip; (2) contactos movidos al conectar y desconectar; (3) pines del STM
  dañados (menos probable: con el riel en ~2.7 V la corriente por los pines fue baja). Pasos: tocar los chips con la
  fuente puesta (uno caliente = desconectar ya); apagar todo 10 s y encender fuente primero, USB después; `PruebaApagado`
  estado 1 para listar filas enteras (banco) y leds sueltos (canal); intercambiar chips; medir pin del STM vs entrada
  y salida del chip.

## Pantalla de cristal líquido

- Resolución: la pantalla funcionó tras corregir el cableado.
