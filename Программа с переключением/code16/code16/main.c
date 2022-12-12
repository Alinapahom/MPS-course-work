#define F_CPU 8000000UL //частота работы МК
#define BAUDRATE 9600L	//скорость передачи данных по usart

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LCD_DDR		DDRA				//порт дисплея
#define LCD_PORT	PORTA				//порт дисплея
#define E1			PORTA |= 0b00001000	//установка линии E в 1
#define E0			PORTA &= 0b11110111	//установка линии E в 0
#define RS1			PORTA |= 0b00000100 //установка линии RS в 1 (данные)
#define RS0			PORTA &= 0b11111011	//установка линии RS в 0 (команда)

#define B_DDR	DDRC
#define B_PORT	PORTC
#define B_PIN	PINC
#define BUT		0

char	data[32];		//массив символов для приема по usart
char	buffer[32];		//массив для вывода символов на дисплей
uint8_t	count;			//полученное кол-во талонов

static int LCD_putchar(char c, FILE *stream);
static int USART_putchar(char c, FILE *stream);
static FILE lcd = FDEV_SETUP_STREAM(LCD_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE usart = FDEV_SETUP_STREAM(USART_putchar, NULL, _FDEV_SETUP_WRITE);

//отправка полбайта в дисплей
void LCD_sendhalfbyte(unsigned char c)
{
	c <<= 4;
	E1;						//включение линии Е
	_delay_us(50);
	LCD_PORT &= 0b00001111; //стираем информацию на входах DB4-DB7, остальное не трогаем
	LCD_PORT |= c;
	E0;						//выключение линии Е
	_delay_us(50);
}

//отправка байта в дисплей
void LCD_sendbyte(unsigned char c, unsigned char mode)
{
	if (mode == 0)
	{
		RS0;
	}
	else
	{
		RS1;
	}
	
	unsigned char hc = 0;
	hc = c >> 4;
	
	LCD_sendhalfbyte(hc);
	LCD_sendhalfbyte(c);
}

//отправка символа в дисплей
void LCD_sendchar(unsigned char c)
{
	LCD_sendbyte(c, 1);
}

//отправка символа по usart
static int LCD_putchar(char c, FILE *stream)
{
	if (c == '\n')
	{
		LCD_putchar('\r', stream);
	}
	
	LCD_sendchar(c);
	
	return 0;
}

//установка координат каретки на дисплее
void LCD_setpos(unsigned char x, unsigned y)
{
	switch(y)
	{
		case 0:
		LCD_sendbyte(x | 0x80, 0);
		break;
		case 1:
		LCD_sendbyte((0x40 + x) | 0x80, 0);
		break;
		case 2:
		LCD_sendbyte((0x14 + x) | 0x80, 0);
		break;
		case 3:
		LCD_sendbyte((0x54 + x) | 0x80, 0);
		break;
	}
}

//инициализация дисплея
void LCD_init(void)
{
	stdout = &lcd;
	
	_delay_ms(15);
	LCD_sendhalfbyte(0b00000011);
	_delay_ms(4);
	LCD_sendhalfbyte(0b00000011);
	_delay_us(100);
	LCD_sendhalfbyte(0b00000011);
	_delay_ms(1);
	LCD_sendhalfbyte(0b00000010);
	_delay_ms(1);
	LCD_sendbyte(0b00101000, 0);
	_delay_ms(1);
	LCD_sendbyte(0b00001100, 0);
	_delay_ms(1);
	LCD_sendbyte(0b00000110, 0);
	_delay_ms(1);
}

//отчистка дисплея
void LCD_clear(void)
{
	LCD_sendbyte(0b00000001, 0);
	_delay_us(1500);
}

//инициализация usart
void USART_init()
{
	stderr = &usart;
	
	UBRRL = F_CPU / BAUDRATE / 16 - 1;	//8 000 000 / 9600 / 16 - 1 = 51
	UCSRB = (1<<TXEN)|(1<<RXEN);		//разрешение приема и передачи
	UCSRC =	(1<<URSEL)|(3<<UCSZ0);		//8 бит
	UCSRB |= (1<<RXCIE);				//разрешение прерывания при передаче
}

//отправка символа по usart
static int USART_putchar(char c, FILE *stream)
{
	if (c == '\n')
	{
		USART_putchar('\r', stream);
	}
	
	while(!(UCSRA & (1<<UDRE)));
	UDR = c;
	
	return 0;
}

//прием данных по usart
void USART_receiving()
{
	memset(data, 0, sizeof data);
	
	int i = 0;
	do
	{
		while(!(UCSRA&(1<<RXC)));
		data[i] = UDR;
		i++;
		
	} while (data[i-1] != '\r');
}


//прерывание usart
ISR(USART_RXC_vect)
{
	USART_receiving();
	
	count = ((data[0]&0b00001111) * 10) + (data[1]&0b00001111);
	
	PORTD |= (1<<PD2);
	fprintf(stderr, "A%d\r", count + 1);
	PORTD &= ~(1<<PD2);
	
	LCD_clear();
	LCD_setpos(0, 0);
	fprintf(stdout, "A%d", count + 1);
}


//нажатие кнопки
void button()
{
	if (!(B_PIN&(0x01<<BUT)))
	{
		while (!(B_PIN&(0x01<<BUT)));
		PORTD |= (1<<PD2);
		fprintf(stderr, "get count\r");
		PORTD &= ~(1<<PD2);
	}
}

int main(void)
{
	LCD_DDR = 0b11111100;
	LCD_PORT = 0x00;
	
	B_DDR = 0x00;
	B_PORT = 0xff;
	
	DDRD |= (1<<PD2);
	//PORTD = 0x00;
	
	USART_init();
	LCD_init();
	
	sei();
	
    while (1) 
    {
		 button();
    }
}

