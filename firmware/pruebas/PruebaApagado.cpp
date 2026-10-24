//PruebaApagado.cpp - Prueba de APAGADO de columnas: piso encendido y columnas apagadas, todo FIJO
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG. Mismo mapa de pines y misma polaridad que CuboLed8x8x8.cpp (version 2).
//
//POR QUE ESTE FIRMWARE: PruebaPisos solo prueba que las cosas PRENDAN: o todo apagado (pisos incluidos),
//o un piso con sus 64 columnas encendidas. Nunca deja un piso encendido con columnas APAGADAS, asi que no
//puede mostrar un banco que no se apaga. Aqui el piso 1 queda encendido fijo (sin multiplexar) y las
//columnas se apagan todas, o se prende una sola por fila. Como nada se mueve, se puede medir con el multimetro.
//
//Cada pulsacion del B1 pasa al siguiente estado (numero en la LCD; en los leds de la tarjeta, modulo 8):
//  0        TODO APAGADO, pisos y columnas. Referencia: aqui nada puede encender.
//  1        PISO 1 ENCENDIDO Y LAS 64 COLUMNAS APAGADAS. Debe verse NEGRO.
//           Si prende una fila entera de 8 leds, el banco de esa fila no apaga: MEDIR AQUI.
//  2 .. 9   PISO 1 Y SOLO LA COLUMNA x = 0 .. 7 DE CADA FILA: una linea de 8 leds (uno por fila) que avanza.
//           Cada estado pone en 0 V una sola entrada de cada 74HCT245 (la A1 en el estado 2 ... la A8 en el 9).
//  10       PISO 1 COMPLETO, los 64 leds (igual al estado 1 de PruebaPisos).
//
//QUE MEDIR en el 74HCT245 del banco que falla. Punta negra SIEMPRE en el negativo del jack (no en la pata 10).
//  Estado 1: pata 20 = 5 V, pata 1 = 5 V, pata 10 = 0 V, pata 19 = 0 V, patas 2..9 = 3.3 V, patas 11..18 = 5 V.
//  Estado 2: lo mismo, salvo pata 2 (A1) = 0 V y pata 18 (B1) = 0 V.
//  - Pata 10 o 19 lejos de 0 V en el estado 1: la tierra del chip no llega (pata doblada en el zocalo,
//    soldadura fria, tira de cobre cortada o puente suelto).
//  - Pata 20 o 1 lejos de 5 V: los 5 V del chip no llegan.
//  - Las cuatro bien, entradas en 3.3 V y salidas bajas: chip danado, cambiarlo.
//  - Una entrada que no baja a 0 V en su estado (pata 2+x en el estado 2+x): ese cable no llega al chip.

#include "stm32f7xx.h"

#define COLUMNAS_ACTIVAS_EN_BAJO 1
#define PISOS_ACTIVOS_EN_ALTO    1
#define PISO_DE_PRUEBA           0      //piso 1 = el de abajo
#define NUM_ESTADOS              11
#define MS_ANTIRREBOTE           250

enum { pB = 0, pC = 1, pD = 2, pE = 3, pF = 4, pG = 5 };
static GPIO_TypeDef* const PUERTO[6] = { GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG };

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

volatile uint32_t ms = 0;
volatile uint8_t  estado = 0;
volatile uint8_t  cambio = 1;           //1: aplicar el estado 0 al arrancar
volatile uint32_t tUltimoB1 = 0;

static void Delay_us(uint32_t us){ volatile uint32_t n = us * 3u; while(n--){ __NOP(); } }
static void Delay_ms(uint32_t t){ uint32_t t0 = ms; while((ms - t0) < t){ } }
static void Salida(GPIO_TypeDef* p, uint8_t n){
	p->MODER   &= ~(3u << (2u*n));  p->MODER   |= (1u << (2u*n));
	p->OTYPER  &= ~(1u << n);
	p->OSPEEDR &= ~(3u << (2u*n));  p->OSPEEDR |= (1u << (2u*n));
	p->PUPDR   &= ~(3u << (2u*n));
}
static void PinAlto(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << n); }
static void PinBajo(GPIO_TypeDef* p, uint8_t n){ p->BSRR = (1u << (n + 16u)); }

static void Columna(uint8_t x, uint8_t y, uint8_t encendida){
	GPIO_TypeDef* p = PUERTO[COL_PUERTO[y][x]];
	uint8_t n = COL_PIN[y][x];
#if COLUMNAS_ACTIVAS_EN_BAJO
	if(encendida) PinBajo(p, n); else PinAlto(p, n);
#else
	if(encendida) PinAlto(p, n); else PinBajo(p, n);
#endif
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
static void TodasLasColumnas(uint8_t encendidas){
	uint8_t x, y;
	for(y=0; y<8; y++) for(x=0; x<8; x++) Columna(x, y, encendidas);
}
static void TodosLosPisos(uint8_t encendidos){
	uint8_t z;
	for(z=0; z<8; z++) Piso(z, encendidos);
}
static void LedsTarjeta(uint8_t n){
	GPIOB->BSRR = ((n & 1u) ? (1u<<0) : (1u<<16)) | ((n & 2u) ? (1u<<7) : (1u<<23)) | ((n & 4u) ? (1u<<14) : (1u<<30));
}

/* LCD 16x2 HD44780, 4 bits: RS PA4, E PA15, D4 PB12, D5 PG14, D6 PG15, D7 PA6 (tiempos de CuboSerial) */
static void LCD_Pulso(void){ PinAlto(GPIOA, 15); Delay_us(5); PinBajo(GPIOA, 15); Delay_us(80); }
static void LCD_Nibble(uint8_t n){
	if(n & 1u) PinAlto(GPIOB, 12); else PinBajo(GPIOB, 12);
	if(n & 2u) PinAlto(GPIOG, 14); else PinBajo(GPIOG, 14);
	if(n & 4u) PinAlto(GPIOG, 15); else PinBajo(GPIOG, 15);
	if(n & 8u) PinAlto(GPIOA, 6);  else PinBajo(GPIOA, 6);
	Delay_us(2);
	LCD_Pulso();
}
static void LCD_Comando(uint8_t c){ PinBajo(GPIOA, 4); LCD_Nibble(c >> 4); LCD_Nibble(c & 0x0F); if(c <= 0x03) Delay_ms(3); }
static void LCD_Dato(uint8_t d){ PinAlto(GPIOA, 4); LCD_Nibble(d >> 4); LCD_Nibble(d & 0x0F); }
static void LCD_Iniciar(void){
	Delay_ms(60); PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);
	LCD_Nibble(0x3); Delay_ms(6); LCD_Nibble(0x3); Delay_ms(2); LCD_Nibble(0x3); Delay_ms(2); LCD_Nibble(0x2); Delay_ms(2);
	LCD_Comando(0x28); LCD_Comando(0x08); LCD_Comando(0x01); LCD_Comando(0x06); LCD_Comando(0x0C);
}
static void LCD_Texto(uint8_t fila, const char* s){
	uint8_t i; LCD_Comando(fila ? 0xC0 : 0x80);
	for(i=0; i<16; i++){ if(*s){ LCD_Dato((uint8_t)*s); s++; } else LCD_Dato(' '); }
}

extern "C" {
void SysTick_Handler(void){ ms++; }     //aqui no hay barrido: todo es fijo
void EXTI15_10_IRQHandler(void){
	EXTI->PR = (1u << 13);
	if((ms - tUltimoB1) >= MS_ANTIRREBOTE){
		tUltimoB1 = ms;
		estado = (uint8_t)((estado + 1) % NUM_ESTADOS);
		cambio = 1;
	}
}
}

static void ConfigurarPines(void){
	uint8_t x, y, z;
	RCC->AHB1ENR |= (1u<<0) | (1u<<1) | (1u<<2) | (1u<<3) | (1u<<4) | (1u<<5) | (1u<<6);
	for(y=0; y<8; y++) for(x=0; x<8; x++) Salida(PUERTO[COL_PUERTO[y][x]], COL_PIN[y][x]);
	for(z=0; z<8; z++) Salida(PUERTO[PISO_PUERTO[z]], PISO_PIN[z]);
	TodasLasColumnas(0); TodosLosPisos(0);
	Salida(GPIOA, 4); Salida(GPIOA, 15); Salida(GPIOA, 6); Salida(GPIOB, 12); Salida(GPIOG, 14); Salida(GPIOG, 15);
	PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);
	Salida(GPIOB, 0); Salida(GPIOB, 7); Salida(GPIOB, 14); LedsTarjeta(0);
	GPIOC->MODER &= ~(3u << 26); GPIOC->PUPDR &= ~(3u << 26); GPIOC->PUPDR |= (2u << 26);
}
static void ConfigurarInterrupciones(void){
	SysTick_Config(16000u);
	NVIC_SetPriority(SysTick_IRQn, 1);
	RCC->APB2ENR |= (1u << 14);
	SYSCFG->EXTICR[3] &= ~(0xFu << 4); SYSCFG->EXTICR[3] |= (0x2u << 4);
	EXTI->IMR |= (1u << 13); EXTI->RTSR |= (1u << 13); EXTI->FTSR &= ~(1u << 13); EXTI->PR = (1u << 13);
	NVIC_SetPriority(EXTI15_10_IRQn, 5);
	NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void AplicarEstado(uint8_t e){
	char l1[17] = "n PISO1 + X=k";
	uint8_t y;
	TodosLosPisos(0);                     //primero todo apagado, luego columnas, al final el piso
	TodasLasColumnas(0);
	LedsTarjeta((uint8_t)(e & 7u));
	if(e == 0){
		LCD_Texto(0, "0 TODO APAGADO"); LCD_Texto(1, "ESPERADO: NADA");
	}else if(e == 1){
		LCD_Texto(0, "1 PISO1 SIN COL"); LCD_Texto(1, "ESPERADO: NEGRO");
		Piso(PISO_DE_PRUEBA, 1);
	}else if(e <= 9){
		l1[0]  = (char)('0' + e);
		l1[12] = (char)('0' + (e - 2));
		LCD_Texto(0, l1); LCD_Texto(1, "8 LEDS EN LINEA");
		for(y=0; y<8; y++) Columna((uint8_t)(e - 2), y, 1);
		Piso(PISO_DE_PRUEBA, 1);
	}else{
		LCD_Texto(0, "10 PISO1 LLENO"); LCD_Texto(1, "ESPERADO: 64");
		TodasLasColumnas(1);
		Piso(PISO_DE_PRUEBA, 1);
	}
}

int main(){
	ConfigurarPines();
	ConfigurarInterrupciones();
	while(1){
		if(cambio){
			cambio = 0;
			LCD_Iniciar();                //se reinicia en cada cambio: no importa en que orden llego la energia
			AplicarEstado(estado);
		}
	}
}
