// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/**
 * Program @file mesurafont.c
 * Version   1.3
 *
 * @brief Programa de mesures elèctriques, tensió i intensitat per una font de 15V
 *
 * @author Roger Ferré Martínez
 * @author Xorxe Oural Martínez
 * 
 * Copyright (C) 2020
 *
 * License GNU/GPL, see COPYING
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 */

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>

int verbose = 1;

static char *cntdevice = "/dev/spidev0.0";

//ADC configurations segons manual MCP3008
#define SINGLE_ENDED_CH0 8
#define SINGLE_ENDED_CH1 9
#define SINGLE_ENDED_CH2 10
#define SINGLE_ENDED_CH3 11
#define SINGLE_ENDED_CH4 12
#define SINGLE_ENDED_CH5 13
#define SINGLE_ENDED_CH6 14
#define SINGLE_ENDED_CH7 15
#define DIFERENTIAL_CH0_CH1 0 //Chanel CH0 = IN+ CH1 = IN-
#define DIFERENTIAL_CH1_CH0 1 //Chanel CH0 = IN- CH1 = IN+
#define DIFERENTIAL_CH2_CH3 2 //Chanel CH2 = IN+ CH3 = IN-
#define DIFERENTIAL_CH3_CH2 3 //Chanel CH2 = IN- CH3 = IN+
#define DIFERENTIAL_CH4_CH5 4 //Chanel CH4 = IN+ CH5 = IN-
#define DIFERENTIAL_CH5_CH4 5 //Chanel CH4 = IN- CH5 = IN+
#define DIFERENTIAL_CH6_CH7 6 //Chanel CH6 = IN+ CH7 = IN-
#define DIFERENTIAL_CH7_CH6 7 //Chanel CH6 = IN- CH7 = IN+

/** Estableix sortida i error*/

static void pabort(const char *s)
{
	perror(s);
	abort();
}

// -----------------------------------------------------------------------------------------------

static void spiadc_config_tx( int conf, uint8_t tx[3] )
{
	int i;

	uint8_t tx_dac[3] = { 0x01, 0x00, 0x00 };
	uint8_t n_tx_dac = 3;

	for (i=0; i < n_tx_dac; i++) {
		tx[i] = tx_dac[i];
	}
	
/** Estableix el mode de comunicació en la parta alta del 2n byte*/
	tx[1]=conf<<4;
	
	if(verbose){
		for(i=0; i < n_tx_dac; i++){
			printf("spi tx dac byte:(%02d)=0x%02x\n",i,tx[i] );
		}
	}
		
}

/** Defineix els paràmetres de comunicació*/
static int spiadc_transfer(int fd, uint8_t bits, uint32_t speed, uint16_t delay, uint8_t tx[3], uint8_t *rx, int len )
{
	int ret, value, i;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len*sizeof(uint8_t),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

	if( verbose ) {

		for (i = 0; i < len; i++) {
			printf("0x%02x ", rx[i]);
		}
		value = ((rx[1] & 0x0F) << 8) + rx[2];
		printf("-->  %d\n", value);
	
	}

	return ret;

}



// -----------------------------------------------------------------------------------------------

static int spiadc_config_transfer( int conf, int *value )
{
	int ret = 0;
	int fd;
	uint8_t rx[3];
	char buffer[255];
	
/**Paràmetres SPI*/

	char *device = cntdevice;
	uint8_t mode = SPI_CPHA; 	//No va bé amb aquesta configuació, ha de ser CPHA
	uint8_t bits = 8;
	uint32_t speed = 500000; //max 1500KHz
	uint16_t delay = 0;
	
/**Transmissió al buffer */
	uint8_t tx[3];

	
	fd = open(device, O_RDWR); //obertura del dispositiu
	if (fd < 0) {
		sprintf( buffer, "can't open device (%s)", device );
		pabort( buffer );
	}


	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode); //Modo SPI
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");


	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits); //Bits per paraula
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");


	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed); //Velocitat màxima per Hz
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");


	spiadc_config_tx(conf, tx); //Construcció de la transferència de dades
		
	 
	ret = spiadc_transfer(fd, bits, speed, delay, tx, rx, 3); // ADC SPI transferència 
	if (ret == 1)
		pabort("can't send spi message");

	close(fd);

	*value = ((rx[1] & 0x03) << 8) + rx[2];

	return ret;
}

/**Presentació de les mesures obtingudes*/

int main(int argc, char *argv[])
{
	int ret = 0, value_int;
	float value_volts;

	ret = spiadc_config_transfer( SINGLE_ENDED_CH2, &value_int );

	printf("Valor llegit (0-1023) %d\n", value_int);
	value_volts=3.3*value_int/1023;
	
	printf("Voltatge a la sortida del divisor de tensions: %.3f V\n", value_volts);

	float Vfont = (value_volts*(780+220))/220;

	printf("Voltatge de la font: %.3f V\n", Vfont);
	printf("Intensitat total %.3f mA\n", ((Vfont - value_volts) / 780) * 1000);

	return ret;
}
