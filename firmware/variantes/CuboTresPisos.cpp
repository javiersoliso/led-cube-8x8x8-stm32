//CuboTresPisos.cpp - Cubo de leds 8x8x8 con CINCO SECUENCIAS sobre los pisos 3, 4 y 5.
//Javier Solis y William Uriza - Sistemas Embebidos - Universidad ECCI - 2026-II
//Tarjeta: NUCLEO-F746ZG. Mismo mapa de pines y misma polaridad que CuboLed8x8x8.cpp (version 2).
//
//Los pisos 3, 4 y 5 son los que responden de forma confiable, asi que todas las secuencias estan
//disenadas para verse completas en esos tres. El barrido recorre SOLO esos tres pisos, con lo cual
//cada led recibe un tercio del tiempo en vez de un octavo: se ve casi tres veces mas brillante y
//refresca a 166 Hz en vez de 62 Hz.
//
//  B1 (azul) cambia de secuencia. El numero sale en los leds de la tarjeta y el nombre en la LCD.
//    1 PLANOS    los tres pisos encienden uno por uno, subiendo y bajando
//    2 ESFERA    una esfera que crece y se encoge desde el centro
//    3 LETRAS    texto que se desplaza; se lee MIRANDO EL CUBO DESDE ARRIBA
//    4 ESPIRAL   un brazo que gira, con los tres pisos desfasados (helice)
//    5 LLUVIA    gotas que caen y vuelven a nacer arriba
//
//  B1 PRESIONADO AL ARRANCAR (desde el reset): usa los OCHO pisos en vez de tres. Si mas adelante
//  quedan buenos los otros pisos, no hay que grabar nada distinto, solo arrancar con el B1 puesto.
//
//  LCD: se reinicializa cada 2 s durante los primeros 20 s despues del reset, para que no importe
//  en que orden se conecten la fuente de 5 V y el cable del computador.

#include "stm32f7xx.h"

#define COLUMNAS_ACTIVAS_EN_BAJO 1
#define PISOS_ACTIVOS_EN_ALTO    1
#define MS_POR_PISO              2
#define NUM_SECUENCIAS           5
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

//Pisos que se usan (indice 0..7 = piso 1..8 de William). Los pisos 3, 4 y 5 son z=2, 3, 4.
static const uint8_t PISOS_BUENOS[3] = { 2, 3, 4 };
static uint8_t PISOS[8] = { 2, 3, 4, 0, 0, 0, 0, 0 };
static volatile uint8_t nPisos = 3;

volatile uint8_t  cubo[8][8];
volatile uint32_t ms = 0;
volatile uint8_t  secuencia = 0;
volatile uint8_t  cambio = 0;
volatile uint32_t tUltimoB1 = 0;
static uint32_t   semilla = 22091;

static void Delay_us(uint32_t us){ volatile uint32_t n = us * 3u; while(n--){ __NOP(); } }
static void Delay_ms(uint32_t t){ uint32_t t0 = ms; while((ms - t0) < t){ if(cambio) return; } }
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

/* ---- lienzo ---- */
static void Limpiar(void){ uint8_t k, y; for(k=0; k<nPisos; k++) for(y=0; y<8; y++) cubo[PISOS[k]][y] = 0; }
static void Punto(int x, int y, int k){
	if(x < 0 || x > 7 || y < 0 || y > 7 || k < 0 || k >= (int)nPisos) return;
	cubo[PISOS[k]][y] = (uint8_t)(cubo[PISOS[k]][y] | (1u << x));
}
static void PisoLleno(uint8_t k){ uint8_t y; if(k < nPisos) for(y=0; y<8; y++) cubo[PISOS[k]][y] = 0xFF; }
static uint32_t Azar(void){ semilla = semilla * 1103515245u + 12345u; return (semilla >> 16); }

/* ---- LCD 16x2 HD44780, 4 bits: RS PA4, E PA15, D4 PB12, D5 PG14, D6 PG15, D7 PA6 ---- */
static void LCD_Pulso(void){ PinAlto(GPIOA, 15); Delay_us(5); PinBajo(GPIOA, 15); Delay_us(80); }
static void LCD_Nibble(uint8_t n){
	if(n & 1u) PinAlto(GPIOB, 12); else PinBajo(GPIOB, 12);
	if(n & 2u) PinAlto(GPIOG, 14); else PinBajo(GPIOG, 14);
	if(n & 4u) PinAlto(GPIOG, 15); else PinBajo(GPIOG, 15);
	if(n & 8u) PinAlto(GPIOA, 6);  else PinBajo(GPIOA, 6);
	Delay_us(2);
	LCD_Pulso();
}
static void LCD_Comando(uint8_t c){ PinBajo(GPIOA, 4); LCD_Nibble(c >> 4); LCD_Nibble(c & 0x0F); if(c <= 0x03){ volatile uint32_t w = 30000; while(w--){ __NOP(); } } }
static void LCD_Dato(uint8_t d){ PinAlto(GPIOA, 4); LCD_Nibble(d >> 4); LCD_Nibble(d & 0x0F); }
static void LCD_Iniciar(void){
	uint32_t t0 = ms; while((ms - t0) < 60u){ }
	PinBajo(GPIOA, 4); PinBajo(GPIOA, 15);
	LCD_Nibble(0x3); t0 = ms; while((ms - t0) < 6u){ }
	LCD_Nibble(0x3); t0 = ms; while((ms - t0) < 2u){ }
	LCD_Nibble(0x3); t0 = ms; while((ms - t0) < 2u){ }
	LCD_Nibble(0x2); t0 = ms; while((ms - t0) < 2u){ }
	LCD_Comando(0x28); LCD_Comando(0x08); LCD_Comando(0x01); LCD_Comando(0x06); LCD_Comando(0x0C);
}
static void LCD_Texto(uint8_t fila, const char* s){
	uint8_t i; LCD_Comando(fila ? 0xC0 : 0x80);
	for(i=0; i<16; i++){ if(*s){ LCD_Dato((uint8_t)*s); s++; } else LCD_Dato(' '); }
}

/* ---- interrupciones ---- */
extern "C" {
void SysTick_Handler(void){
	static uint8_t cuenta = 0, idx = 0;
	uint32_t poner[6] = {0,0,0,0,0,0}, quitar[6] = {0,0,0,0,0,0};
	uint8_t x, y, p, n, fila, z;
	ms++;
	if(++cuenta < MS_POR_PISO) return;
	cuenta = 0;
	Piso(PISOS[idx], 0);
	idx++; if(idx >= nPisos) idx = 0;
	z = PISOS[idx];
	for(y=0; y<8; y++){
		fila = cubo[z][y];
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
	Piso(z, 1);
}
void EXTI15_10_IRQHandler(void){
	EXTI->PR = (1u << 13);
	if((ms - tUltimoB1) >= MS_ANTIRREBOTE){
		tUltimoB1 = ms;
		secuencia = (uint8_t)((secuencia + 1) % NUM_SECUENCIAS);
		cambio = 1;
	}
}
}

/* ---- 1 PLANOS ---- */
static void Planos(uint32_t paso){
	uint8_t ciclo, k;
	Limpiar();
	if(nPisos < 2){ PisoLleno(0); return; }
	ciclo = (uint8_t)(paso % (2u*nPisos - 2u));
	k = (ciclo < nPisos) ? ciclo : (uint8_t)(2u*nPisos - 2u - ciclo);
	PisoLleno(k);
}

/* ---- 2 ESFERA ---- */
static void Esfera(uint32_t paso){
	int k, x, y, X, Y, Z, d2, R, ri, ro, zc;
	uint32_t p = paso % 20u;
	R = 2 + (int)((p <= 10u) ? p : (20u - p));       //2 .. 12 .. 2
	ri = (R - 2) * (R - 2);
	ro = (R + 2) * (R + 2);
	zc = PISOS[nPisos / 2];
	Limpiar();
	for(k = 0; k < (int)nPisos; k++){
		Z = 2 * ((int)PISOS[k] - zc);
		for(y = 0; y < 8; y++){
			Y = 2*y - 7;
			for(x = 0; x < 8; x++){
				X = 2*x - 7;
				d2 = X*X + Y*Y + Z*Z;
				if(d2 >= ri && d2 <= ro) Punto(x, y, k);
			}
		}
	}
}

/* ---- 3 LETRAS (se leen desde arriba) ---- */
static const char MENSAJE[] = "FELICES 30   CUBO 8X8X8 ECCI   JAVIER Y WILLIAM   ";
static const uint8_t FUENTE[21][5] = {
	{0x00,0x00,0x00,0x00,0x00},  //  espacio
	{0x3E,0x51,0x49,0x45,0x3E},  // 0
	{0x22,0x41,0x49,0x49,0x36},  // 3
	{0x36,0x49,0x49,0x49,0x36},  // 8
	{0x7E,0x11,0x11,0x11,0x7E},  // A
	{0x7F,0x49,0x49,0x49,0x36},  // B
	{0x3E,0x41,0x41,0x41,0x22},  // C
	{0x7F,0x49,0x49,0x49,0x41},  // E
	{0x7F,0x09,0x09,0x09,0x01},  // F
	{0x00,0x41,0x7F,0x41,0x00},  // I
	{0x20,0x40,0x41,0x3F,0x01},  // J
	{0x7F,0x40,0x40,0x40,0x40},  // L
	{0x7F,0x02,0x0C,0x02,0x7F},  // M
	{0x3E,0x41,0x41,0x41,0x3E},  // O
	{0x7F,0x09,0x19,0x29,0x46},  // R
	{0x46,0x49,0x49,0x49,0x31},  // S
	{0x3F,0x40,0x40,0x40,0x3F},  // U
	{0x1F,0x20,0x40,0x20,0x1F},  // V
	{0x3F,0x40,0x38,0x40,0x3F},  // W
	{0x63,0x14,0x08,0x14,0x63},  // X
	{0x03,0x04,0x78,0x04,0x03}   // Y
};
static const char INDICE[] = " 038ABCEFIJLMORSUVWXY";
static uint8_t Glifo(char c){
	uint8_t i;
	for(i = 0; INDICE[i]; i++) if(INDICE[i] == c) return i;
	return 0;
}
static uint8_t ColumnaTexto(uint32_t col){
	uint32_t largo = (uint32_t)(sizeof(MENSAJE) - 1u);
	uint32_t total = largo * 6u;
	uint32_t c = col % total;
	uint32_t iChar = c / 6u, iCol = c % 6u;
	if(iCol == 5u) return 0;
	return FUENTE[Glifo(MENSAJE[iChar])][iCol];
}
static void Letras(uint32_t paso){
	int k, x, y;
	uint8_t patron;
	Limpiar();
	for(x = 0; x < 8; x++){
		patron = ColumnaTexto(paso + (uint32_t)x);
		for(y = 0; y < 8; y++){
			if((patron >> y) & 1u) for(k = 0; k < (int)nPisos; k++) Punto(x, 7 - y, k);
		}
	}
}

/* ---- 4 ESPIRAL ---- */
static const uint8_t ANILLO[16][2] = {
	{7,3},{7,5},{6,6},{5,7},{4,7},{2,7},{1,6},{0,5},
	{0,3},{0,2},{1,1},{2,0},{3,0},{5,0},{6,1},{7,2}
};
static void Espiral(uint32_t paso){
	int k, t, a, px, py, ex, ey;
	Limpiar();
	for(k = 0; k < (int)nPisos; k++){
		a = (int)((paso + (uint32_t)(2*k)) & 15u);
		ex = (int)ANILLO[a][0]; ey = (int)ANILLO[a][1];
		for(t = 0; t <= 3; t++){
			px = 3 + ((ex - 3) * t) / 3;
			py = 3 + ((ey - 3) * t) / 3;
			Punto(px, py, k);
			Punto(px + 1, py + 1, k);
		}
	}
}

/* ---- 5 LLUVIA ---- */
#define NUM_GOTAS 12
static uint8_t gx[NUM_GOTAS], gy[NUM_GOTAS];
static int8_t  gk[NUM_GOTAS];
static void LluviaIniciar(void){
	uint8_t i;
	for(i = 0; i < NUM_GOTAS; i++){
		gx[i] = (uint8_t)(Azar() & 7u); gy[i] = (uint8_t)(Azar() & 7u);
		gk[i] = (int8_t)(Azar() % (uint32_t)(nPisos + 2u));
	}
}
static void Lluvia(void){
	uint8_t i;
	Limpiar();
	for(i = 0; i < NUM_GOTAS; i++){
		gk[i]--;
		if(gk[i] < 0){ gk[i] = (int8_t)(nPisos - 1); gx[i] = (uint8_t)(Azar() & 7u); gy[i] = (uint8_t)(Azar() & 7u); }
		Punto((int)gx[i], (int)gy[i], (int)gk[i]);
	}
}

/* ---- programa ---- */
static const char* const NOMBRE[NUM_SECUENCIAS] = { "1 PLANOS", "2 ESFERA", "3 LETRAS", "4 ESPIRAL", "5 LLUVIA" };
static const uint16_t CUADRO[NUM_SECUENCIAS]    = { 260, 110, 130, 90, 150 };

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

int main(){
	uint32_t paso = 0, tLCD = 0;
	uint8_t i, y, z, actual;
	ConfigurarPines();

	for(i = 0; i < 200; i++) __NOP();                    //dejar asentar el pull-down del B1
	if(GPIOC->IDR & (1u << 13)){                         //B1 presionado al arrancar: los 8 pisos
		for(i = 0; i < 8; i++) PISOS[i] = i;
		nPisos = 8;
	}else{
		for(i = 0; i < 3; i++) PISOS[i] = PISOS_BUENOS[i];
		nPisos = 3;
	}
	for(z = 0; z < 8; z++) for(y = 0; y < 8; y++) cubo[z][y] = 0;

	SysTick_Config(16000u);
	NVIC_SetPriority(SysTick_IRQn, 1);
	RCC->APB2ENR |= (1u << 14);
	SYSCFG->EXTICR[3] &= ~(0xFu << 4); SYSCFG->EXTICR[3] |= (0x2u << 4);
	EXTI->IMR |= (1u << 13); EXTI->RTSR |= (1u << 13); EXTI->FTSR &= ~(1u << 13); EXTI->PR = (1u << 13);
	NVIC_SetPriority(EXTI15_10_IRQn, 5);
	NVIC_EnableIRQ(EXTI15_10_IRQn);

	LCD_Iniciar();
	LCD_Texto(0, "CUBO 8x8x8 ECCI");
	LCD_Texto(1, "JAVIER Y WILLIAM");

	for(i = 0; i < nPisos; i++) PisoLleno(i);            //prueba de lamparas
	Delay_ms(1200);
	cambio = 1;
	LluviaIniciar();

	while(1){
		if(cambio){
			cambio = 0; paso = 0;
			actual = secuencia;
			LedsTarjeta((uint8_t)(actual + 1u));
			LCD_Texto(0, "CUBO 8x8x8 ECCI");
			LCD_Texto(1, NOMBRE[actual]);
			if(actual == 4) LluviaIniciar();
		}
		actual = secuencia;
		switch(actual){
			case 0: Planos(paso);  break;
			case 1: Esfera(paso);  break;
			case 2: Letras(paso);  break;
			case 3: Espiral(paso); break;
			default: Lluvia();     break;
		}
		paso++;
		Delay_ms(CUADRO[actual]);
		if(ms < 20000u && (ms - tLCD) >= 2500u){        //reintento de la pantalla al principio
			tLCD = ms;
			LCD_Iniciar();
			LCD_Texto(0, "CUBO 8x8x8 ECCI");
			LCD_Texto(1, NOMBRE[secuencia]);
		}
	}
}
