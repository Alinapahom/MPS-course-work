#define F_CPU 8000000UL //������� ������ ��
#define BAUDRATE 9600L	//�������� �������� ������ �� usart

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LCD_DDR		DDRA				//���� �������
#define LCD_PORT	PORTA				//���� �������
#define E1			PORTA |= 0b00001000	//��������� ����� E � 1
#define E0			PORTA &= 0b11110111	//��������� ����� E � 0
#define RS1			PORTA |= 0b00000100 //��������� ����� RS � 1 (������)
#define RS0			PORTA &= 0b11111011	//��������� ����� RS � 0 (�������)

#define B_DDR	DDRC
#define B_PORT	PORTC
#define B_PIN	PINC
#define BUT		0

char	data[32];		//������ �������� ��� ������ �� usart
char	buffer[32];		//������ ��� ������ �������� �� �������
uint8_t	count;			//���������� ���-�� �������

static int LCD_putchar(char c, FILE *stream);
static int USART_putchar(char c, FILE *stream);
static FILE lcd = FDEV_SETUP_STREAM(LCD_putchar, NULL, _FDEV_SETUP_WRITE);
static FILE usart = FDEV_SETUP_STREAM(USART_putchar, NULL, _FDEV_SETUP_WRITE);

//�������� �������� � �������
void LCD_sendhalfbyte(unsigned char c)
{
	c <<= 4;
	E1;						//��������� ����� �
	_delay_us(50);
	LCD_PORT &= 0b00001111; //������� ���������� �� ������ DB4-DB7, ��������� �� �������
	LCD_PORT |= c;
	E0;						//���������� ����� �
	_delay_us(50);
}

//�������� ����� � �������
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

//�������� ������� � �������
void LCD_sendchar(unsigned char c)
{
	LCD_sendbyte(c, 1);
}

//�������� ������� �� usart
static int LCD_putchar(char c, FILE *stream)
{
	if (c == '\n')
	{
		LCD_putchar('\r', stream);
	}
	
	LCD_sendchar(c);
	
	return 0;
}

//��������� ��������� ������� �� �������
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

//������������� �������
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

//�������� �������
void LCD_clear(void)
{
	LCD_sendbyte(0b00000001, 0);
	_delay_us(1500);
}

//������������� usart
void USART_init()
{
	stderr = &usart;
	
	UBRRL = F_CPU / BAUDRATE / 16 - 1;	//8 000 000 / 9600 / 16 - 1 = 51
	UCSRB = (1<<TXEN)|(1<<RXEN);		//���������� ������ � ��������
	UCSRC =	(1<<URSEL)|(3<<UCSZ0);		//8 ���
	UCSRB |= (1<<RXCIE);				//���������� ���������� ��� ��������
}

//�������� ������� �� usart
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

//����� ������ �� usart
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


//���������� usart
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


//������� ������
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

