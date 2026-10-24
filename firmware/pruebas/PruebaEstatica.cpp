//PruebaEstatica.cpp - Prueba de cableado del cubo con NIVELES FIJOS (medibles con multimetro)
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG. Mismo mapa de pines y misma polaridad que CuboLed8x8x8.cpp (version 2).
//
//Cada pulsacion del B1 (azul) pasa al siguiente estado. El estado se ve en los leds de la tarjeta
//(LD3 LD2 LD1 = numero en binario) y en la LCD si esta conectada. El estado se queda FIJO hasta la
//siguiente pulsacion, asi que se puede medir con el multimetro con calma.
//
//  0  TODO APAGADO         columnas en 1, pisos en 0                     leds tarjeta: 000
//  1  UN LED               columna x0 (PD0=0) + piso z0 (PD10=1)         001
//  2  FILA y0, PISO 0      PD0..PD7 = 0, PD10 = 1  -> 8 leds             010
//  3  COLUMNA x0, 8 PISOS  PD0 = 0, los 8 pisos = 1 -> 8 leds en vertical (tenues: comparten 10 mA)   011
//  4  PLANO y0 FIJO        PD0..PD7 = 0, 8 pisos = 1 -> 64 leds tenues   100
//  5  BARRIDO PLANO y0     multiplexado real (2 ms por piso), plano y0 completo   101
//  6  BARRIDO TODO         multiplexado real, las 8 filas (para cuando esten las dos baquelas)   110
//  y vuelve a 0.
//
//Que medir en el estado 1 (todo con la fuente de 5 V puesta y GND comun):
//  chip banco 1: pin 20 = 5 V, pin 1 = 5 V, pin 19 = 0 V, pin 10 = 0 V
//  chip banco 1: pin 2 (A1) = 0 V,  pin 18 (B1) = 0 V
//  PNP columna 1: base ~4.3 V, emisor 5 V, colector ~4.8 V
//  chip pisos:   pin 2 (A1) = 3.3 V, pin 18 (B1) = 5 V
//  MOSFET 1: compuerta 5 V, pata central (drenador) ~0.05 V
//  Y un solo led encendido, fijo, a brillo completo: columna 1, fila y0, piso de abajo.

#include "stm32f7xx.h"

#define COLUMNAS_ACTIVAS_EN_BAJO 1
#define PISOS_ACTIVOS_EN_ALTO    1
#define MS_POR_PISO              2
#define NUM_ESTADOS              7
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
volatile uint8_t  barriendo = 0;        //1 = el SysTick maneja los pines; 0 = niveles fijos
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
static void TodoApagado(void){
	uint8_t x, y, z;
	for(y=0; y<8; y++) for(x=0; x<8; x++) Columna(x, y, 0);
	for(z=0; z<8; z++) Piso(z, 0);
}
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

static const char* const NOMBRE[NUM_ESTADOS] = {
	"0 TODO APAGADO", "1 UN LED x0y0z0", "2 FILA y0 PISO0", "3 COL x0 8PISOS",
	"4 PLANO y0 FIJO", "5 BARRIDO y0", "6 BARRIDO TODO"
};

static void AplicarEstado(uint8_t e){
	uint8_t x, y, z;
	barriendo = 0;                        //primero parar el barrido y apagar todo
	Delay_ms(5);
	TodoApagado();
	for(z=0; z<8; z++) for(y=0; y<8; y++) cubo[z][y] = 0;
	LedsTarjeta(e);
	LCD_Texto(0, "PRUEBA ESTATICA");
	LCD_Texto(1, NOMBRE[e]);
	switch(e){
		case 1: Columna(0, 0, 1); Piso(0, 1); break;
		case 2: for(x=0; x<8; x++) Columna(x, 0, 1); Piso(0, 1); break;
		case 3: Columna(0, 0, 1); for(z=0; z<8; z++) Piso(z, 1); break;
		case 4: for(x=0; x<8; x++) Columna(x, 0, 1); for(z=0; z<8; z++) Piso(z, 1); break;
		case 5: for(z=0; z<8; z++) cubo[z][0] = 0xFF; barriendo = 1; break;
		case 6: for(z=0; z<8; z++) for(y=0; y<8; y++) cubo[z][y] = 0xFF; barriendo = 1; break;
		default: break;
	}
}

int main(){
	ConfigurarPines();
	ConfigurarInterrupciones();
	LCD_Iniciar();
	AplicarEstado(0);
	while(1){
		if(cambio){ cambio = 0; AplicarEstado(estado); }
	}
}
