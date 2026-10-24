//PruebaPisos.cpp - Prueba PISO POR PISO del cubo 8x8x8
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG. Mismo mapa de pines y misma polaridad que CuboLed8x8x8.cpp (version 2).
//
//POR QUE ESTE FIRMWARE: con varios pisos encendidos a la vez, los 8 leds de una misma columna
//cuelgan en paralelo de una sola resistencia de 220 ohm y se reparten la corriente; el led de menor
//tension directa se la roba y los otros quedan tenues. Con UN SOLO PISO encendido, cada resistencia
//alimenta UN SOLO led: brillo pleno y parejo. Asi se ve de verdad si un piso o una columna falla.
//
//Cada pulsacion del B1 (azul) pasa al siguiente estado. Numero del estado en los leds de la tarjeta
//(LD3 LD2 LD1 = binario, se repite cada 8) y en la LCD.
//
//  0        TODO APAGADO
//  1 .. 8   PISO 1 .. PISO 8, los 64 leds de ese piso, FIJOS, a brillo pleno. Piso 1 = el de abajo.
//  9        RECORRIDO automatico: sube piso por piso solo, 800 ms cada uno, sin multiplexar.
//  10       CUBO COMPLETO multiplexado (2 ms por piso), como en el firmware de entrega.
//  y vuelve a 0.
//
//Que mirar en los estados 1 a 8: los 64 leds del piso encendidos, parejos, sin parpadeo.
//  - Falta una linea recta de 8 leds  -> esa fila y (esa tira / ese banco) no llega.
//  - Faltan los mismos leds en todos los pisos -> es la columna, no el piso.
//  - Un piso entero no enciende -> su MOSFET, su cable de tierra o su entrada del chip de pisos.
//  - Un piso enciende tenue -> mala union en su cable de tierra o en la fuente de su MOSFET.

#include "stm32f7xx.h"

#define COLUMNAS_ACTIVAS_EN_BAJO 1
#define PISOS_ACTIVOS_EN_ALTO    1
#define MS_POR_PISO              2
#define MS_RECORRIDO             800
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

volatile uint8_t  cubo[8][8];
volatile uint32_t ms = 0;
volatile uint8_t  estado = 0;
volatile uint8_t  cambio = 0;
volatile uint8_t  barriendo = 0;
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
static void TodoApagado(void){ TodasLasColumnas(0); TodosLosPisos(0); }
static void LedsTarjeta(uint8_t n){
	GPIOB->BSRR = ((n & 1u) ? (1u<<0) : (1u<<16)) | ((n & 2u) ? (1u<<7) : (1u<<23)) | ((n & 4u) ? (1u<<14) : (1u<<30));
}

/* LCD 16x2 HD44780, 4 bits: RS PA4, E PA15, D4 PB12, D5 PG14, D6 PG15, D7 PA6 */
static void LCD_Pulso(void){ PinAlto(GPIOA, 15); Delay_us(2); PinBajo(GPIOA, 15); Delay_us(60); }
static void LCD_Nibble(uint8_t n){
	if(n & 1u) PinAlto(GPIOB, 12); else PinBajo(GPIOB, 12);
	if(n & 2u) PinAlto(GPIOG, 14); else PinBajo(GPIOG, 14);
	if(n & 4u) PinAlto(GPIOG, 15); else PinBajo(GPIOG, 15);
	if(n & 8u) PinAlto(GPIOA, 6);  else PinBajo(GPIOA, 6);
	LCD_Pulso();
}
static void LCD_Comando(uint8_t c){ PinBajo(GPIOA, 4); LCD_Nibble(c >> 4); LCD_Nibble(c & 0x0F); if(c <= 0x03) Delay_ms(2); }
static void LCD_Dato(uint8_t d){ PinAlto(GPIOA, 4); LCD_Nibble(d >> 4); LCD_Nibble(d & 0x0F); }
static void LCD_Iniciar(void){
	Delay_ms(50); PinBajo(GPIOA, 4);
	LCD_Nibble(0x3); Delay_ms(5); LCD_Nibble(0x3); Delay_us(150); LCD_Nibble(0x3); Delay_us(150); LCD_Nibble(0x2); Delay_us(150);
	LCD_Comando(0x28); LCD_Comando(0x08); LCD_Comando(0x01); LCD_Comando(0x06); LCD_Comando(0x0C);
}
static void LCD_Texto(uint8_t fila, const char* s){
	uint8_t i; LCD_Comando(fila ? 0xC0 : 0x80);
	for(i=0; i<16; i++){ if(*s){ LCD_Dato((uint8_t)*s); s++; } else LCD_Dato(' '); }
}

extern "C" {
void SysTick_Handler(void){
	static uint8_t cuenta = 0, piso = 7;
	uint32_t poner[6] = {0,0,0,0,0,0}, quitar[6] = {0,0,0,0,0,0};
	uint8_t x, y, p, n, fila;
	ms++;
	if(!barriendo) return;                //en los estados fijos el SysTick no toca los pines
	if(++cuenta < MS_POR_PISO) return;
	cuenta = 0;
	Piso(piso, 0);
	piso = (uint8_t)((piso + 1) & 7);
	for(y=0; y<8; y++){
		fila = cubo[piso][y];
		for(x=0; x<8; x++){
			p = COL_PUERTO[y][x]; n = COL_PIN[y][x];
#if COLUMNAS_ACTIVAS_EN_BAJO
			if((fila >> x) & 1u) quitar[p] |= (1u << n); else poner[p] |= (1u << n);
#else
			if((fila >> x) & 1u) poner[p]  |= (1u << n); else quitar[p] |= (1u << n);
#endif
		}
	}
	for(p=0; p<6; p++) PUERTO[p]->BSRR = poner[p] | (quitar[p] << 16);
	Piso(piso, 1);
}
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
	TodoApagado();
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

static void LCD_Piso(uint8_t z){
	char t[17] = "PISO n  64 LEDS";
	t[5] = (char)('1' + z);
	LCD_Texto(1, t);
}

static void AplicarEstado(uint8_t e){
	uint8_t y, z;
	barriendo = 0;                        //parar el barrido y apagar todo antes de cambiar
	Delay_ms(5);
	TodoApagado();
	for(z=0; z<8; z++) for(y=0; y<8; y++) cubo[z][y] = 0;
	LedsTarjeta((uint8_t)(e & 7u));
	LCD_Texto(0, "PRUEBA DE PISOS");
	if(e == 0){
		LCD_Texto(1, "0 TODO APAGADO");
	}else if(e <= 8){                     //un solo piso, los 64 leds, fijo, brillo pleno
		LCD_Piso((uint8_t)(e - 1));
		TodasLasColumnas(1);
		Piso((uint8_t)(e - 1), 1);
	}else if(e == 9){
		LCD_Texto(1, "9 RECORRIDO");
		TodasLasColumnas(1);              //el recorrido lo mueve el bucle principal
	}else{
		LCD_Texto(1, "10 CUBO BARRIDO");
		for(z=0; z<8; z++) for(y=0; y<8; y++) cubo[z][y] = 0xFF;
		barriendo = 1;
	}
}

int main(){
	uint8_t z = 0;
	uint32_t t0;
	ConfigurarPines();
	ConfigurarInterrupciones();
	LCD_Iniciar();
	AplicarEstado(0);
	while(1){
		if(cambio){ cambio = 0; z = 0; AplicarEstado(estado); }
		if(estado == 9){                  //recorrido automatico: un piso a la vez, sin multiplexar
			TodosLosPisos(0);
			Piso(z, 1);
			LCD_Piso(z);
			t0 = ms;
			while(((ms - t0) < MS_RECORRIDO) && !cambio){ }
			z = (uint8_t)((z + 1) & 7);
		}
	}
}
