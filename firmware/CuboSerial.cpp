//CuboSerial.cpp - Cubo de leds 8x8x8 + LCD 16x2 + B1 + ENLACE SERIAL EN VIVO con el simulador web
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG (STM32F746ZGT6). En Keil: dispositivo STM32F746ZGTx, CMSIS->CORE y Device->Startup.
//Estilo del curso: registros directos, sin HAL. Reloj interno HSI de 16 MHz (sin PLL).
//
//Es CuboLed8x8x8.cpp (version 2, la de la entrega) mas el enlace serial de dos vias con el simulador:
//  SIN simulador  corre sus cinco secuencias y el B1 las cambia, igual que la version 2.
//  CON simulador  (Chrome o Edge, boton "Conectar cubo") el cubo muestra en vivo lo mismo que la vista 3D:
//                 las 13 secuencias, TOCAR (la esfera se mueve con el mouse), SNAKE y DIBUJAR. La LCD y los
//                 leds de la tarjeta copian los del simulador, y el B1 fisico cambia la secuencia alla.
//  Si deja de llegar imagen durante 1 s (pestana minimizada o cerrada, cable suelto), el cubo vuelve solo a
//  sus secuencias y sigue con la ultima que mostraba el simulador, si era una de las cinco del taller.
//
//ENLACE: USART3, PD8 = TX y PD9 = RX. En la Nucleo esos dos pines van al ST-LINK: es el puerto COM virtual
//  del mismo cable USB de CN1. No se cablea nada; PD8 y PD9 deben quedar libres en la organizadora.
//  115200 baudios, 8 bits, sin paridad, 1 bit de parada. Reloj del USART3 = PCLK1 = 16 MHz.
//  PC -> cubo (binario):  A5 5A TIPO DATOS... XOR          XOR = TIPO ^ DATOS[0] ^ ... ^ DATOS[n-1]
//    TIPO 01 IMAGEN  64 bytes: cubo[0][0..7], cubo[1][0..7] ... cubo[7][0..7]        (68 bytes por trama)
//    TIPO 02 TEXTO   1 byte = secuencia del simulador (0..12) + 16 + 16 caracteres de la LCD  (37 bytes)
//  cubo -> PC (texto, una linea por mensaje, terminada en \r\n):
//    CUBO LISTO          al arrancar
//    B1                  en cada pulsacion valida del B1
//    ESTADO VIVO 25 0    cada segundo: modo (VIVO o LOCAL), tramas buenas y tramas malas de ese segundo
//  Reenganche: si a mitad de una trama pasan mas de 5 ms sin bytes, se descarta y se espera otro A5 5A.
//  Recepcion por interrupcion (RXNE) con prioridad 0, por encima del barrido: el USART3 del F746 guarda un
//  solo byte y a 115200 baudios llega uno cada 87 us, asi que su rutina no puede esperar a que acabe el SysTick.
//
//ARQUITECTURA
//  cubo[z][y]  memoria de imagen: 8 pisos (z) x 8 filas (y), un byte por fila, bit x = columna.
//  SysTick     cada 1 ms; cada MS_POR_PISO ms apaga el piso actual, escribe las 64 columnas y enciende el siguiente.
//              8 pisos x 2 ms = 16 ms por vuelta = 62 Hz: el ojo ve el cubo completo.
//  Secuencias  solo escriben en la memoria de imagen (Limpiar/Led/Volcar). Son las mismas del simulador web.
//              En vivo es la rutina del USART3 la que escribe cubo[][] cuando llega una imagen completa y buena.
//  B1 (PC13)   EXTI13, flanco de subida. Antirrebote por marca de tiempo del SysTick, NO con retardos dentro de la rutina.
//  LCD 16x2    HD44780 en modo de 4 bits, solo escritura (RW a GND). Se reinicializa cada 2.5 s durante los
//              primeros 20 s, para que no importe si la fuente de 5 V se conecta despues que el USB.
//
//CONTRATO DE PINES (el chip 74HCT245 es la fila y, el bit es la columna x, U9 son los pisos z)
//  Baquela 1 (la de tiras, con jack y MOSFET):
//    y=0 banco 1: PD0..PD7          y=1 banco 2: PG0-PG5,PG8,PG9
//    y=2 banco 3: PC6-PC12,PB15     y=3 banco 4: PB1-PB6,PB10,PB11
//  Baquela 2 (perforada):
//    y=4 banco 1: PE0..PE7          y=5 banco 2: PE8..PE15
//    y=6 banco 3: PF0..PF7          y=7 banco 4: PF8..PF15
//  Pisos (U9 de la baquela 1). MOSFET 1 = piso de ARRIBA (medido 18/09), asi que MOSFET = 8 - z:
//    z0 (abajo) PG12 = MOSFET 8, z1 PG10, z2 PD15, z3 PD14, z4 PD13, z5 PD12, z6 PD11, z7 (arriba) PD10 = MOSFET 1
//  Columna x = numero de William en la madera menos 1 (columna 1 = x0 ... columna 8 = x7).
//  LCD: RS PA4, E PA15, D4 PB12, D5 PG14, D6 PG15, D7 PA6 (RW a GND)
//  Leds de la tarjeta: LD1 PB0, LD2 PB7, LD3 PB14 muestran el numero de secuencia en binario.
//  Serial: PD8 (TX) y PD9 (RX) al ST-LINK, ya puenteados en la tarjeta.
//
//MODO PRUEBA DE CABLEADO: mantener presionado B1 mientras se pulsa RESET (o al conectar el USB).
//  Recorre columnas x=0..7, filas y=0..7 y pisos z=0..7, un plano a la vez, 700 ms cada uno.
//  Los leds de la tarjeta muestran el indice en binario (LD3 LD2 LD1) y la LCD lo escribe.
//  En este modo el enlace serial no se enciende.
//
//POLARIDAD (cambiar segun el montaje)
//  74HCT245 + 2N2907 (PNP):  la columna ENCIENDE con 0 en el pin  -> COLUMNAS_ACTIVAS_EN_BAJO 1
//  74HCT245 directo al led:  la columna ENCIENDE con 1 en el pin  -> COLUMNAS_ACTIVAS_EN_BAJO 0
//  74HCT245 + IRLZ44N:       el piso ENCIENDE con 1 en el pin     -> PISOS_ACTIVOS_EN_ALTO 1

#include "stm32f7xx.h"

#define COLUMNAS_ACTIVAS_EN_BAJO 1
#define PISOS_ACTIVOS_EN_ALTO    1
#define MS_POR_PISO              2      //2 ms por piso -> 62 Hz de refresco
#define NUM_SECUENCIAS           5
#define MS_POR_CUADRO            140    //velocidad de las animaciones (igual al simulador)
#define MS_ANTIRREBOTE           200

#define USART3_BRR               139    //16 MHz / 115200 = 138.9 -> 139 (error de 0.08 %)
#define MS_REENGANCHE            5      //silencio a mitad de trama que la descarta
#define MS_SIN_ENLACE            1000   //sin imagenes durante este tiempo -> vuelve a las secuencias propias
#define TRAMA_IMAGEN             0x01
#define TRAMA_TEXTO              0x02
#define LARGO_IMAGEN             64
#define LARGO_TEXTO              33
#define TX_TAM                   128    //cola de salida del serial (potencia de 2)

/* ------------------------------------------------------------------ */
/* Mapa de pines. Indice de puerto: 0=B 1=C 2=D 3=E 4=F 5=G              */
/* ------------------------------------------------------------------ */
enum { pB = 0, pC = 1, pD = 2, pE = 3, pF = 4, pG = 5 };
static GPIO_TypeDef* const PUERTO[6] = { GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };

//COL_PUERTO[y][x] y COL_PIN[y][x]: a que pin del micro va la columna (x, y).
//Si una tira quedo cableada en otro orden, se corrige AQUI reordenando la fila, no en el hardware.
static const uint8_t COL_PUERTO[8][8] = {
	{ pD, pD, pD, pD, pD, pD, pD, pD },   //y=0  baquela 1, banco 1
	{ pG, pG, pG, pG, pG, pG, pG, pG },   //y=1  baquela 1, banco 2
	{ pC, pC, pC, pC, pC, pC, pC, pB },   //y=2  baquela 1, banco 3
	{ pB, pB, pB, pB, pB, pB, pB, pB },   //y=3  baquela 1, banco 4
	{ pE, pE, pE, pE, pE, pE, pE, pE },   //y=4  baquela 2, banco 1
	{ pE, pE, pE, pE, pE, pE, pE, pE },   //y=5  baquela 2, banco 2
	{ pF, pF, pF, pF, pF, pF, pF, pF },   //y=6  baquela 2, banco 3
	{ pF, pF, pF, pF, pF, pF, pF, pF }    //y=7  baquela 2, banco 4
};
static const uint8_t COL_PIN[8][8] = {
	{  0,  1,  2,  3,  4,  5,  6,  7 },   //PD0..PD7
	{  0,  1,  2,  3,  4,  5,  8,  9 },   //PG0..PG5, PG8, PG9
	{  6,  7,  8,  9, 10, 11, 12, 15 },   //PC6..PC12, PB15
	{  1,  2,  3,  4,  5,  6, 10, 11 },   //PB1..PB6, PB10, PB11
	{  0,  1,  2,  3,  4,  5,  6,  7 },   //PE0..PE7
	{  8,  9, 10, 11, 12, 13, 14, 15 },   //PE8..PE15
	{  0,  1,  2,  3,  4,  5,  6,  7 },   //PF0..PF7
	{  8,  9, 10, 11, 12, 13, 14, 15 }    //PF8..PF15
};
//Pisos: MOSFET 1 es el piso de ARRIBA (medido el 18/09: la lluvia subia), asi que z0 = abajo = MOSFET 8.
static const uint8_t PISO_PUERTO[8] = { pG, pG, pD, pD, pD, pD, pD, pD };   //z0 (abajo) ... z7 (arriba)
static const uint8_t PISO_PIN[8]    = { 12, 10, 15, 14, 13, 12, 11, 10 };   //PG12 = MOSFET 8 ... PD10 = MOSFET 1

/* ------------------------------------------------------------------ */
/* Variables compartidas con las interrupciones                         */
/* ------------------------------------------------------------------ */
volatile uint8_t  cubo[8][8];           //memoria de imagen que barre el SysTick
static   uint8_t  lienzo[8][8];         //donde dibujan las secuencias; Volcar() lo copia a cubo
volatile uint32_t ms = 0;               //milisegundos desde el arranque (SysTick)
volatile uint8_t  secuencia = 0;        //0..4, la cambia el pulsador (o el simulador)
volatile uint8_t  cambio = 0;           //bandera: hubo cambio de secuencia
volatile uint32_t tUltimoB1 = 0;        //marca de tiempo de la ultima pulsacion valida

//Enlace serial
volatile uint8_t  enVivo = 0;           //1 = la imagen la pone el simulador
volatile uint32_t tUltimaImagen = 0;    //marca de tiempo de la ultima imagen buena
volatile uint8_t  texto[LARGO_TEXTO];   //ultimo texto bueno: secuencia + 32 caracteres de la LCD
volatile uint8_t  hayTexto = 0;         //bandera: llego texto nuevo
volatile uint16_t tramasBuenas = 0, tramasMalas = 0;   //contadores del segundo en curso
volatile uint8_t  txCola[TX_TAM];       //cola de salida: la llena Enviar(), la vacia la rutina del USART3
volatile uint8_t  txIni = 0, txFin = 0;

/* ------------------------------------------------------------------ */
/* Utilidades                                                           */
/* ------------------------------------------------------------------ */
static void Delay_us(uint32_t us){       //aprox. con HSI de 16 MHz; sirve para la LCD (no necesita precision)
	volatile uint32_t n = us * 3u;
	while(n--){ __NOP(); }
}
static void Delay_ms(uint32_t t){        //usa el SysTick, no bloquea las interrupciones
	uint32_t t0 = ms;
	while((ms - t0) < t){ }
}
static uint32_t semilla = 20260911u;
static uint32_t Rand(void){              //generador propio: no depende de la biblioteca estandar
	semilla = semilla * 1103515245u + 12345u;
	return (semilla >> 16) & 0x7FFFu;
}
static void Salida(GPIO_TypeDef* p, uint8_t n){   //pin como salida push-pull, velocidad media, sin pull
	p->MODER   &= ~(3u << (2u*n));
	p->MODER   |=  (1u << (2u*n));
	p->OTYPER  &= ~(1u << n);
	p->OSPEEDR &= ~(3u << (2u*n));
	p->OSPEEDR |=  (1u << (2u*n));
	p->PUPDR   &= ~(3u << (2u*n));
}
static void PinAlto(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << n); }
static void PinBajo(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << (n + 16u)); }

/* ------------------------------------------------------------------ */
/* Memoria de imagen                                                    */
/* ------------------------------------------------------------------ */
static void Limpiar(void){
	uint8_t z, y;
	for(z=0; z<8; z++) for(y=0; y<8; y++) lienzo[z][y] = 0x00;
}
static void Led(uint8_t x, uint8_t y, uint8_t z){
	if(x < 8 && y < 8 && z < 8) lienzo[z][y] |= (uint8_t)(1u << x);
}
static void Volcar(void){                //copia el lienzo al cubo (64 bytes, ~1 us: el barrido no lo nota)
	uint8_t z, y;
	if(enVivo) return;                   //en vivo manda el simulador: no se pisa su imagen
	for(z=0; z<8; z++) for(y=0; y<8; y++) cubo[z][y] = lienzo[z][y];
}
static void Piso(uint8_t z, uint8_t encendido){
	GPIO_TypeDef* p = PUERTO[PISO_PUERTO[z]];
	uint8_t n = PISO_PIN[z];
#if PISOS_ACTIVOS_EN_ALTO
	if(encendido) PinAlto(p, n); else PinBajo(p, n);
#else
	if(encendido) PinBajo(p, n); else PinAlto(p, n);
#endif
}

/* ------------------------------------------------------------------ */
/* Leds de la tarjeta: numero de secuencia (1..5) en binario            */
/* ------------------------------------------------------------------ */
static void LedsTarjeta(uint8_t n){
	GPIOB->BSRR = ((n & 1u) ? (1u<<0)  : (1u<<16))
	            | ((n & 2u) ? (1u<<7)  : (1u<<23))
	            | ((n & 4u) ? (1u<<14) : (1u<<30));
}

/* ------------------------------------------------------------------ */
/* LCD 16x2 HD44780, 4 bits, solo escritura                             */
/*   RS PA4, E PA15, D4 PB12, D5 PG14, D6 PG15, D7 PA6                  */
/*   lcd[][] guarda lo que se le escribio, para reescribirla completa    */
/*   despues de reinicializarla.                                        */
/* ------------------------------------------------------------------ */
static char lcd[2][17] = { "                ", "                " };

static void LCD_Pulso(void){
	PinAlto(GPIOA, 15); Delay_us(5);
	PinBajo(GPIOA, 15); Delay_us(80);    //>= 37 us que tarda cualquier instruccion normal
}
static void LCD_Nibble(uint8_t n){
	if(n & 1u) PinAlto(GPIOB, 12); else PinBajo(GPIOB, 12);   //D4
	if(n & 2u) PinAlto(GPIOG, 14); else PinBajo(GPIOG, 14);   //D5
	if(n & 4u) PinAlto(GPIOG, 15); else PinBajo(GPIOG, 15);   //D6
	if(n & 8u) PinAlto(GPIOA, 6);  else PinBajo(GPIOA, 6);    //D7
	Delay_us(2);
	LCD_Pulso();
}
static void LCD_Comando(uint8_t c){
	PinBajo(GPIOA, 4);                    //RS = 0: instruccion
	LCD_Nibble(c >> 4); LCD_Nibble(c & 0x0F);
	if(c <= 0x03) Delay_ms(3);            //Clear y Home tardan 1.5 ms
}
static void LCD_Dato(uint8_t d){
	PinAlto(GPIOA, 4);                    //RS = 1: dato
	LCD_Nibble(d >> 4); LCD_Nibble(d & 0x0F);
}
static void LCD_Iniciar(void){
	Delay_ms(60);                         //la LCD necesita >40 ms despues de alimentarla
	PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);
	LCD_Nibble(0x3); Delay_ms(6);         //secuencia de arranque en 8 bits (hoja de datos)
	LCD_Nibble(0x3); Delay_ms(2);
	LCD_Nibble(0x3); Delay_ms(2);
	LCD_Nibble(0x2); Delay_ms(2);         //pasar a 4 bits
	LCD_Comando(0x28);                    //4 bits, 2 lineas, 5x8
	LCD_Comando(0x08);                    //display apagado
	LCD_Comando(0x01);                    //borrar
	LCD_Comando(0x06);                    //cursor avanza, sin desplazar
	LCD_Comando(0x0C);                    //display encendido, sin cursor
}
static void LCD_Escribir(uint8_t fila){   //manda a la pantalla la fila guardada en lcd[fila]
	uint8_t i;
	LCD_Comando(fila ? 0xC0 : 0x80);
	for(i=0; i<16; i++) LCD_Dato((uint8_t)lcd[fila][i]);
}
static void LCD_Texto(uint8_t fila, const char* s){   //escribe 16 caracteres, rellena con espacios
	uint8_t i;
	for(i=0; i<16; i++) lcd[fila][i] = *s ? *s++ : ' ';
	LCD_Escribir(fila);
}
static void LCD_Copiar(uint8_t fila, const uint8_t* s){   //16 bytes que llegaron del PC; lo no imprimible va como espacio
	uint8_t i;
	for(i=0; i<16; i++) lcd[fila][i] = (s[i] >= 0x20u && s[i] <= 0x7Eu) ? (char)s[i] : ' ';
	LCD_Escribir(fila);
}

/* ------------------------------------------------------------------ */
/* Enlace serial: salida al PC                                          */
/* ------------------------------------------------------------------ */
//Pone el texto en la cola y enciende la interrupcion TXE; la rutina del USART3 lo saca byte por byte.
//Nunca espera: si la cola se llena, lo que no cabe se pierde. Se puede llamar desde main o desde una rutina.
static void Enviar(const char* s){
	uint32_t primask = __get_PRIMASK();
	__disable_irq();                      //dos productores (main y EXTI): la cola se toca sin interrupciones
	while(*s){
		uint8_t sig = (uint8_t)((txFin + 1u) & (TX_TAM - 1u));
		if(sig == txIni) break;           //cola llena
		txCola[txFin] = (uint8_t)*s++;
		txFin = sig;
	}
	USART3->CR1 |= USART_CR1_TXEIE;
	__set_PRIMASK(primask);
}
static uint8_t Copiar(char* d, const char* s){       //copia s en d y devuelve cuantos caracteres copio
	uint8_t k = 0;
	while(s[k]){ d[k] = s[k]; k++; }
	return k;
}
static uint8_t Numero(char* d, uint16_t n){          //n en decimal, sin printf; devuelve cuantas cifras
	char t[5];
	uint8_t k = 0, i;
	do{ t[k++] = (char)('0' + n % 10u); n = (uint16_t)(n / 10u); }while(n);
	for(i=0; i<k; i++) d[i] = t[k - 1u - i];
	return k;
}

/* ------------------------------------------------------------------ */
/* Enlace serial: entrada del PC (se llama desde la rutina del USART3)  */
/* ------------------------------------------------------------------ */
enum { ESPERA_A5, ESPERA_5A, ESPERA_TIPO, DATOS, SUMA };
static uint8_t  rxEstado = ESPERA_A5, rxTipo, rxLargo, rxN, rxXor;
static uint8_t  rx[LARGO_IMAGEN];
static uint32_t rxT = 0;

static void TramaBuena(void){
	uint8_t i;
	tramasBuenas++;
	if(rxTipo == TRAMA_IMAGEN){
		for(i=0; i<LARGO_IMAGEN; i++) cubo[i >> 3][i & 7u] = rx[i];   //directo a la memoria que barre el SysTick
		tUltimaImagen = ms;
		enVivo = 1;
	}else{
		for(i=0; i<LARGO_TEXTO; i++) texto[i] = rx[i];                 //la LCD es lenta: la escribe main
		hayTexto = 1;
	}
}
static void Recibir(uint8_t b){
	uint32_t ahora = ms;
	if(rxEstado != ESPERA_A5 && (ahora - rxT) > MS_REENGANCHE){   //silencio a mitad de trama: se descarta
		rxEstado = ESPERA_A5;
		tramasMalas++;
	}
	rxT = ahora;
	switch(rxEstado){
		case ESPERA_A5:
			if(b == 0xA5) rxEstado = ESPERA_5A;
			break;
		case ESPERA_5A:
			if(b == 0x5A)      rxEstado = ESPERA_TIPO;
			else if(b != 0xA5) rxEstado = ESPERA_A5;
			break;
		case ESPERA_TIPO:
			rxTipo = b; rxXor = b; rxN = 0;
			if(b == TRAMA_IMAGEN)     { rxLargo = LARGO_IMAGEN; rxEstado = DATOS; }
			else if(b == TRAMA_TEXTO) { rxLargo = LARGO_TEXTO;  rxEstado = DATOS; }
			else                      { rxEstado = ESPERA_A5; tramasMalas++; }
			break;
		case DATOS:
			rx[rxN++] = b;
			rxXor ^= b;
			if(rxN >= rxLargo) rxEstado = SUMA;
			break;
		default:                          //SUMA: el ultimo byte es el XOR de todo lo anterior
			if(b == rxXor) TramaBuena(); else tramasMalas++;
			rxEstado = ESPERA_A5;
			break;
	}
}

/* ------------------------------------------------------------------ */
/* Las cinco secuencias (mismo codigo que la pestana "Codigo" del sim)  */
/* ------------------------------------------------------------------ */
static const char* const NOMBRES[NUM_SECUENCIAS] = { "PLANOS", "LLUVIA", "CUBO", "ESPIRAL", "ONDA" };

//1. PLANOS: un plano recorre el cubo de ida y vuelta, por los tres ejes
static void Planos(uint32_t f){
	uint8_t a, b, eje, s, p;
	Limpiar();
	f   = f % 48;
	eje = (uint8_t)(f / 16);              //0=niveles 1=filas 2=columnas
	s   = (uint8_t)(f % 16);
	p   = (s < 8) ? s : (uint8_t)(15 - s);
	for(a=0; a<8; a++){
		for(b=0; b<8; b++){
			if(eje == 0)      Led(a, b, p);
			else if(eje == 1) Led(a, p, b);
			else              Led(p, a, b);
		}
	}
}

//2. LLUVIA: gotas que caen
static uint8_t gx[16], gy[16], gz[16], ngotas = 0;
static void Lluvia(void){
	uint8_t i, j = 0;
	Limpiar();
	for(i=0; i<ngotas; i++){              //bajar un nivel; las que tocaron piso se borran
		if(gz[i] > 0){
			gz[i]--;
			gx[j] = gx[i]; gy[j] = gy[i]; gz[j] = gz[i];
			j++;
		}
	}
	ngotas = j;
	if(ngotas < 14){                      //nace una gota nueva arriba
		gx[ngotas] = (uint8_t)(Rand() % 8);
		gy[ngotas] = (uint8_t)(Rand() % 8);
		gz[ngotas] = 7;
		ngotas++;
	}
	for(i=0; i<ngotas; i++) Led(gx[i], gy[i], gz[i]);
}

//3. CUBO: un cubo hueco que crece y se encoge desde el centro
static void Cubo(uint32_t f){
	static const uint8_t tam[6] = { 0, 1, 2, 3, 2, 1 };
	uint8_t s, lo, hi, x, y, z;
	Limpiar();
	s  = tam[(f/2) % 6];
	lo = (uint8_t)(3 - s);
	hi = (uint8_t)(4 + s);
	for(x=lo; x<=hi; x++)
		for(y=lo; y<=hi; y++)
			for(z=lo; z<=hi; z++)
				if(x==lo || x==hi || y==lo || y==hi || z==lo || z==hi)   //solo la cascara
					Led(x, y, z);
}

//4. ESPIRAL: un punto por nivel dando la vuelta al borde, cada nivel corrido
static const uint8_t PX[28] = { 0,1,2,3,4,5,6,7, 7,7,7,7,7,7,7, 6,5,4,3,2,1,0, 0,0,0,0,0,0 };
static const uint8_t PY[28] = { 0,0,0,0,0,0,0,0, 1,2,3,4,5,6,7, 7,7,7,7,7,7,7, 6,5,4,3,2,1 };
static void Espiral(uint32_t f){
	uint8_t z, i;
	Limpiar();
	for(z=0; z<8; z++){
		i = (uint8_t)((f + z*3) % 28);
		Led(PX[i], PY[i], z);
	}
}

//5. ONDA: una superficie senoidal que viaja en diagonal
static const uint8_t SEN[16] = { 4,5,6,7,7,7,6,5,4,2,1,0,0,0,1,2 };
static void Onda(uint32_t f){
	uint8_t x, y;
	Limpiar();
	for(x=0; x<8; x++)
		for(y=0; y<8; y++)
			Led(x, y, SEN[(x + y + f) & 15]);
}

/* ------------------------------------------------------------------ */
/* Interrupciones                                                       */
/* ------------------------------------------------------------------ */
extern "C" {

//Barrido del cubo. Entra cada 1 ms; cada MS_POR_PISO ms cambia de piso.
void SysTick_Handler(void){
	static uint8_t cuenta = 0, piso = 7;
	uint32_t poner[6] = {0,0,0,0,0,0}, quitar[6] = {0,0,0,0,0,0};
	uint8_t x, y, p, n, fila;

	ms++;
	if(++cuenta < MS_POR_PISO) return;
	cuenta = 0;

	Piso(piso, 0);                        //1. apagar el piso que estaba encendido
	piso = (uint8_t)((piso + 1) & 7);     //2. siguiente piso

	for(y=0; y<8; y++){                   //3. preparar los 64 bits de columnas en mascaras por puerto
		fila = cubo[piso][y];
		for(x=0; x<8; x++){
			p = COL_PUERTO[y][x];
			n = COL_PIN[y][x];
#if COLUMNAS_ACTIVAS_EN_BAJO
			if((fila >> x) & 1u) quitar[p] |= (1u << n); else poner[p] |= (1u << n);
#else
			if((fila >> x) & 1u) poner[p]  |= (1u << n); else quitar[p] |= (1u << n);
#endif
		}
	}
	for(p=0; p<6; p++) PUERTO[p]->BSRR = poner[p] | (quitar[p] << 16);   //4. escribir (atomico, no toca otros pines)

	Piso(piso, 1);                        //5. encender el piso nuevo
}

//Pulsador B1 (PC13): flanco de subida. Antirrebote por tiempo: se ignora todo lo que llegue
//en los MS_ANTIRREBOTE ms siguientes a una pulsacion valida. Nada de retardos aqui adentro.
void EXTI15_10_IRQHandler(void){
	EXTI->PR = (1u << 13);                //bajar la bandera (con '=' para no borrar otras)
	if((ms - tUltimoB1) >= MS_ANTIRREBOTE){
		tUltimoB1 = ms;
		Enviar("B1\r\n");                 //el simulador tambien se entera (si esta conectado)
		if(!enVivo){                      //en vivo la secuencia la cambia el simulador y la devuelve en el texto
			secuencia = (uint8_t)((secuencia + 1) % NUM_SECUENCIAS);
			cambio = 1;
		}
	}
}

//USART3: recibe del PC (RXNE) y le envia la cola de salida (TXE).
void USART3_IRQHandler(void){
	uint32_t isr = USART3->ISR;
	if(isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)){   //byte perdido o con error: la trama en curso no sirve
		USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;   //si no se borra ORE, la rutina entra sin parar
		if(rxEstado != ESPERA_A5){ rxEstado = ESPERA_A5; tramasMalas++; }
	}
	if(isr & USART_ISR_RXNE) Recibir((uint8_t)USART3->RDR);   //leer RDR baja RXNE
	if((USART3->CR1 & USART_CR1_TXEIE) && (isr & USART_ISR_TXE)){
		if(txIni != txFin){
			USART3->TDR = txCola[txIni];
			txIni = (uint8_t)((txIni + 1u) & (TX_TAM - 1u));
		}else{
			USART3->CR1 &= ~USART_CR1_TXEIE;   //cola vacia: apagar TXE hasta el proximo Enviar()
		}
	}
}

} //extern "C"

/* ------------------------------------------------------------------ */
/* Configuracion                                                        */
/* ------------------------------------------------------------------ */
static void ConfigurarPines(void){
	uint8_t x, y, z;
	//Reloj de los puertos A, B, C, D, E, F, G (RCC_AHB1ENR bits 0..6)
	RCC->AHB1ENR |= (1u<<0) | (1u<<1) | (1u<<2) | (1u<<3) | (1u<<4) | (1u<<5) | (1u<<6);

	//Columnas: salida y en estado APAGADO
	for(y=0; y<8; y++) for(x=0; x<8; x++){
		GPIO_TypeDef* p = PUERTO[COL_PUERTO[y][x]];
		Salida(p, COL_PIN[y][x]);
#if COLUMNAS_ACTIVAS_EN_BAJO
		PinAlto(p, COL_PIN[y][x]);
#else
		PinBajo(p, COL_PIN[y][x]);
#endif
	}
	//Pisos: salida y apagados
	for(z=0; z<8; z++){ Salida(PUERTO[PISO_PUERTO[z]], PISO_PIN[z]); Piso(z, 0); }

	//LCD
	Salida(GPIOA, 4);  Salida(GPIOA, 15); Salida(GPIOA, 6);
	Salida(GPIOB, 12); Salida(GPIOG, 14); Salida(GPIOG, 15);
	PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);

	//Leds de la tarjeta
	Salida(GPIOB, 0); Salida(GPIOB, 7); Salida(GPIOB, 14);
	LedsTarjeta(0);

	//Pulsador PC13 como entrada con pull-down (B1 entrega 3.3 V al presionar)
	GPIOC->MODER &= ~(3u << 26);
	GPIOC->PUPDR &= ~(3u << 26);
	GPIOC->PUPDR |=  (2u << 26);
}

static void ConfigurarInterrupciones(void){
	//SysTick a 1 ms con el HSI de 16 MHz. Prioridad alta: el barrido no se puede atrasar.
	SysTick_Config(16000u);
	NVIC_SetPriority(SysTick_IRQn, 1);

	//EXTI13 en PC13, flanco de subida, prioridad mas baja que el barrido
	RCC->APB2ENR |= (1u << 14);           //reloj de SYSCFG
	SYSCFG->EXTICR[3] &= ~(0xFu << 4);    //campo EXTI13
	SYSCFG->EXTICR[3] |=  (0x2u << 4);    //0010 = puerto C
	EXTI->IMR  |=  (1u << 13);
	EXTI->RTSR |=  (1u << 13);
	EXTI->FTSR &= ~(1u << 13);
	EXTI->PR    =  (1u << 13);            //limpiar cualquier bandera vieja antes de habilitar
	NVIC_SetPriority(EXTI15_10_IRQn, 5);
	NVIC_EnableIRQ(EXTI15_10_IRQn);
}

//USART3 en PD8 (TX) y PD9 (RX), funcion alternativa 7, 115200 8N1, interrupcion de recepcion.
static void ConfigurarSerial(void){
	RCC->APB1ENR |= RCC_APB1ENR_USART3EN; //reloj del USART3 (bit 18); el del puerto D ya esta encendido
	GPIOD->MODER  &= ~((3u << 16) | (3u << 18));
	GPIOD->MODER  |=  ((2u << 16) | (2u << 18));        //10 = funcion alternativa
	GPIOD->AFR[1] &= ~((0xFu << 0) | (0xFu << 4));
	GPIOD->AFR[1] |=  ((7u << 0) | (7u << 4));          //AF7 = USART3 en PD8 y PD9
	GPIOD->PUPDR  &= ~(3u << 18);
	GPIOD->PUPDR  |=  (1u << 18);                       //pull-up en RX: en reposo la linea esta en 1

	USART3->CR1 = 0;                                    //apagado mientras se configura
	USART3->BRR = USART3_BRR;                           //sobremuestreo x16: BRR = fck / baudios
	USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
	USART3->CR1 |= USART_CR1_UE;

	NVIC_SetPriority(USART3_IRQn, 0);     //la mas alta: la rutina es corta y un byte no puede esperar al barrido
	NVIC_EnableIRQ(USART3_IRQn);
}

/* ------------------------------------------------------------------ */
/* Tareas de fondo del programa principal                               */
/* ------------------------------------------------------------------ */
//Se llama sin parar desde las esperas de main. Nunca desde una rutina de interrupcion (escribe la LCD).
static void Atender(void){
	static uint32_t tEstado = 0, tLCD = 0;
	char m[28];
	uint16_t buenas, malas;
	uint8_t k;
	uint32_t ahora = ms;

	//LCD: reinicializar cada 2.5 s en los primeros 20 s y reescribir lo que tenia
	if(ahora < 20000u && (ahora - tLCD) >= 2500u){
		tLCD = ahora;
		LCD_Iniciar();
		LCD_Escribir(0); LCD_Escribir(1);
	}
	//Cada segundo: estado del enlace para el PC ("ESTADO VIVO 25 0")
	if((ahora - tEstado) >= 1000u){
		tEstado = ahora;
		__disable_irq();
		buenas = tramasBuenas; malas = tramasMalas;
		tramasBuenas = 0; tramasMalas = 0;
		__enable_irq();
		k = Copiar(m, enVivo ? "ESTADO VIVO " : "ESTADO LOCAL ");
		k = (uint8_t)(k + Numero(&m[k], buenas));
		m[k++] = ' ';
		k = (uint8_t)(k + Numero(&m[k], malas));
		m[k++] = '\r'; m[k++] = '\n'; m[k] = 0;
		Enviar(m);
	}
}
static void Esperar(uint32_t t){         //espera t ms atendiendo el enlace; sale antes si cambio la secuencia o llego el simulador
	uint32_t t0 = ms;
	while((ms - t0) < t && !cambio && !enVivo) Atender();
}

/* ------------------------------------------------------------------ */
/* Programa principal                                                   */
/* ------------------------------------------------------------------ */
static void MostrarSecuencia(uint8_t s){
	char l1[17] = "SECUENCIA n/5";
	l1[10] = (char)('1' + s);
	LCD_Texto(0, l1);
	LCD_Texto(1, NOMBRES[s]);
	LedsTarjeta((uint8_t)(s + 1));
}

//Modo prueba de cableado: un plano a la vez. eje 0 = columnas x, 1 = filas y, 2 = pisos z.
static void PlanoPrueba(uint8_t eje, uint8_t p){
	uint8_t a, b;
	Limpiar();
	for(a=0; a<8; a++) for(b=0; b<8; b++){
		if(eje == 0)      Led(p, a, b);     //columna x = p, todas las filas y pisos
		else if(eje == 1) Led(a, p, b);     //fila y = p
		else              Led(a, b, p);     //piso z = p
	}
	Volcar();
}
static void ModoPrueba(void){
	static const char* const EJE[3] = { "COLUMNA X=", "FILA    Y=", "PISO    Z=" };
	char l2[17];
	uint8_t eje, p, i;
	LCD_Texto(0, "PRUEBA CABLEADO");
	while(1){
		for(eje=0; eje<3; eje++){
			for(p=0; p<8; p++){
				for(i=0; i<10; i++) l2[i] = EJE[eje][i];
				l2[10] = (char)('0' + p);
				l2[11] = ' '; l2[12] = '('; l2[13] = (char)('1' + ((eje == 2) ? (7 - p) : p)); l2[14] = ')'; l2[15] = 0;   //(n) = columna de William o MOSFET
				LCD_Texto(1, l2);
				LedsTarjeta(p);
				PlanoPrueba(eje, p);
				Delay_ms(700);
			}
		}
		LCD_Texto(1, "TODO ENCENDIDO");
		LedsTarjeta(7);
		for(p=0; p<8; p++) for(i=0; i<8; i++) lienzo[p][i] = 0xFF;
		Volcar();
		Delay_ms(1200);
	}
}

//Secuencias propias: corre la secuencia actual hasta que el B1 la cambie o empiece a llegar imagen del PC.
static void ModoLocal(void){
	uint32_t f = 0;
	uint8_t  s = secuencia;
	cambio = 0;
	ngotas = 0;
	MostrarSecuencia(s);
	while(!cambio && !enVivo){
		switch(s){
			case 0: Planos(f);  break;
			case 1: Lluvia();   break;
			case 2: Cubo(f);    break;
			case 3: Espiral(f); break;
			default: Onda(f);   break;
		}
		Volcar();
		f++;
		Esperar(MS_POR_CUADRO);
	}
}

//En vivo: la imagen la escribe la rutina del USART3; aqui solo se atiende el texto (LCD, leds, secuencia)
//y se vigila que sigan llegando imagenes.
static void ModoEnVivo(void){
	uint8_t  lin[LARGO_TEXTO], i, n;
	uint32_t t0;
	while(1){
		Atender();
		if(hayTexto){
			__disable_irq();                      //copiar el texto sin que llegue otro a mitad de camino
			for(i=0; i<LARGO_TEXTO; i++) lin[i] = texto[i];
			hayTexto = 0;
			__enable_irq();
			n = lin[0];
			if(n < NUM_SECUENCIAS) secuencia = n; //si se cae el enlace, el cubo sigue con esta
			LedsTarjeta((uint8_t)(n < NUM_SECUENCIAS ? n + 1u : 0u));
			LCD_Copiar(0, &lin[1]);
			LCD_Copiar(1, &lin[17]);
		}
		__disable_irq();                          //la marca se lee antes que ms y sin que la rutina la mueva en medio
		t0 = tUltimaImagen;
		if((ms - t0) > MS_SIN_ENLACE){ enVivo = 0; __enable_irq(); return; }
		__enable_irq();
	}
}

int main(){
	uint8_t z, y;

	ConfigurarPines();
	ConfigurarInterrupciones();
	LCD_Iniciar();

	//B1 presionado al arrancar -> modo prueba de cableado (no vuelve; soltar B1 y RESET para salir)
	if(GPIOC->IDR & (1u << 13)) ModoPrueba();

	//Prueba de lamparas: todo encendido un momento (sirve para ver columnas o pisos muertos)
	LCD_Texto(0, "CUBO 8x8x8");
	LCD_Texto(1, "PRUEBA DE LEDS");
	for(z=0; z<8; z++) for(y=0; y<8; y++) lienzo[z][y] = 0xFF;
	Volcar();
	Delay_ms(1000);

	ConfigurarSerial();                   //el enlace arranca despues de la prueba de lamparas
	Enviar("CUBO LISTO\r\n");

	while(1){
		if(enVivo) ModoEnVivo();          //manda el simulador
		else       ModoLocal();           //secuencias propias
	}
}
