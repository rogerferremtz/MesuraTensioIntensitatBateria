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

/*
 * ADC MC3008 example
 *
 * Cross-compile with   arm-linux-gnueabi-gcc nom.c -o nom_executable
 *
 * Copyrigth (C) EUSS 2018  ( http://www.euss.cat )
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
#include <sqlite3.h>
#include <wiringPi.h>

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

int cridarsql(float, float, int, int);

// -----------------------------------------------------------------------------------------------

static void pabort(const char *s)
{
	perror(s);
	abort();
}

static int callback_tensio(void *punter, int argc, char **argv, char **azColName)
{
	int i, id = -1;

	for (i = 0; i < argc; i++) {
		//printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
		id = atoi(argv[i]);
	}
	if (id == 0)
		return 1;
	else
		printf("Sensor Tensio REGISTRAT\n");

	return 0;
}

static int callback_intensitat(void *punter, int argc, char **argv, char **azColName)
{
	int i, id = -1;

	for (i = 0; i < argc; i++) {
		//printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
		id = atoi(argv[i]);
	}
	if (id == 0)
		return 1;
	else
		printf("Sensor Intensitat REGISTRAT\n");

	return 0;
}

static int callback_id(void *punter, int argc, char **argv, char **azColName)
{
	int i, id = -1;
	int *punterint = punter;

	for (i = 0; i < argc; i++) {
		//printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
		id = atoi(argv[i]);
	}
	*punterint = id;
	return 0;
}

// -----------------------------------------------------------------------------------------------

static void spiadc_config_tx(int conf, uint8_t tx[3])
{
	int i;

	uint8_t tx_dac[3] = {0x01, 0x00, 0x00};
	uint8_t n_tx_dac = 3;

	for (i = 0; i < n_tx_dac; i++) {
		tx[i] = tx_dac[i];
	}

// Estableix el mode de comunicació en la parta alta del 2n byte
	tx[1] = conf << 4;
	if (verbose) {
		for (i = 0; i < n_tx_dac; i++) {
			//printf("spi tx dac byte:(%02d)=0x%02x\n",i,tx[i] );
		}
	}
}

// -----------------------------------------------------------------------------------------------
static int spiadc_transfer(int fd, uint8_t bits, uint32_t speed, uint16_t delay, uint8_t tx[3], uint8_t *rx, int len)
{
	int ret, i;

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len*sizeof(uint8_t),
		.delay_usecs = delay,
		.speed_hz = speed,
		.bits_per_word = bits,
	};

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

	if (verbose) {

		for (i = 0; i < len; i++) {
			//printf("0x%02x ", rx[i]);
		}
		//value = ((rx[1] & 0x0F) << 8) + rx[2];
		//printf("-->  %d\n", value);
	}
	return ret;
}

// -----------------------------------------------------------------------------------------------

static int spiadc_config_transfer(int conf, int *value)
{
	int ret = 0;
	int fd;
	uint8_t rx[3];
	char buffer[255];

	/* SPI parameters */
	char *device = cntdevice;
	//uint8_t mode = SPI_CPOL; //No va bé amb aquesta configuació, ha de ser CPHA
	uint8_t mode = SPI_CPHA;
	uint8_t bits = 8;
	uint32_t speed = 500000; //max 1500KHz
	uint16_t delay = 0;

	/* Transmission buffer */
	uint8_t tx[3];

	/* open device */
	fd = open(device, O_RDWR);
	if (fd < 0) {
		sprintf(buffer, "can't open device (%s)", device);
		pabort(buffer);
	}

	/* spi mode */
	ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (ret == -1)
		pabort("can't set spi mode");

	ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (ret == -1)
		pabort("can't get spi mode");

	/* bits per word */
	ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't set bits per word");

	ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (ret == -1)
		pabort("can't get bits per word");

	/* max speed hz */
	ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't set max speed hz");

	ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		pabort("can't get max speed hz");

	/* build data to transfer */
	spiadc_config_tx(conf, tx);

	/* spi adc transfer */
	ret = spiadc_transfer(fd, bits, speed, delay, tx, rx, 3);
	if (ret == 1)
		pabort("can't send spi message");

	close(fd);

	*value = ((rx[1] & 0x03) << 8) + rx[2];

	return ret;
}

// -----------------------------------------------------------------------------------------------

int sensor_nom(float valor_llegit, int id_tensio, int id_intensitat)
{
	float value_volts;

	printf("Valor llegit (0-1023) %.f\n", valor_llegit);

	value_volts = 3.3*valor_llegit/1023;

	printf("Voltatge a la sortida del divisor de tensions: %.3f V\n", value_volts);

	float Vfont = (value_volts*(780+220))/220;

	printf("Voltatge de la font: %.3f V\n", Vfont);

	float intensitat = ((Vfont - value_volts) / 780) * 1000;
	printf("Intensitat total %.3f mA\n", intensitat);

	/*wiringPiSetup();
	pinMode (3, OUTPUT);
	for(;;){
		digitalWrite(0, HIGH);
		delay (500);
		digitalWrite(0, LOW);
	}*/

	//cridarsql(id_sensor, id_intensitat, value_volts, intensitat);
	cridarsql(value_volts, intensitat, id_tensio, id_intensitat);
	return 0;
}

// -----------------------------------------------------------------------------------------------



int cridarsql(float tensio, float intensitat, int id_tensio, int id_intensitat)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

	rc = sqlite3_open("basedades_adstr.db", &db);

	if (rc != SQLITE_OK) {
		  fprintf(stderr, "Cannot open database.\n");
		  return 1;
	}

	char sql_tensio[1024];

	sprintf(sql_tensio, "INSERT INTO mesures (id_sensor, valor) VALUES (%d, %f);", id_tensio, tensio);

	printf("SQLITE3: %s\n", sql_tensio);

	rc = sqlite3_exec(db, sql_tensio, 0, 0, &zErrMsg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return 1;
	}

	char sql_intensitat[1024];

	sprintf(sql_intensitat, "INSERT INTO mesures (id_sensor, valor) VALUES (%d, %f);", id_intensitat, intensitat);

	printf("SQLITE3: %s\n\n", sql_intensitat);

	rc = sqlite3_exec(db, sql_intensitat, 0, 0, &zErrMsg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return 1;
	}
		sqlite3_close(db);
		return 0;
}

// -----------------------------------------------------------------------------------------------

int main (int argc, char *argv[])
{
	int ret = 0, value_int;

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	int id_tensio, id_intensitat;

	rc = sqlite3_open("basedades_adstr.db", &db);

	if (rc != SQLITE_OK) {
		  fprintf(stderr, "Cannot open database.\n");
		  return 1;
	}

	char checksensortensio[1024] = "SELECT EXISTS (SELECT id_sensor FROM sensors WHERE nom_sensor = 'Sensor_Tensio');";
	char checksensorintensitat[1024] = "SELECT EXISTS (SELECT id_sensor FROM sensors WHERE nom_sensor = 'Sensor_Intensitat');";
	char demanar_id_tensio[1024] = "SELECT id_sensor FROM sensors WHERE nom_sensor = 'Sensor_Tensio';";
	char demanar_id_intensitat[1024] = "SELECT id_sensor FROM sensors WHERE nom_sensor = 'Sensor_Intensitat';";

	rc = sqlite3_exec(db, checksensortensio, callback_tensio, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		printf("El sensor anomenat 'Sensor_Tensio' no està registrat, procedim a enregistrar-lo.\n");
		char entrar_sensor_t[1024] = "INSERT INTO sensors (nom_sensor, descripcio) VALUES('Sensor_Tensio','Sensor per mesurar tensio');";

		rc = sqlite3_exec(db, entrar_sensor_t, 0, 0, &zErrMsg);
	}
		rc = sqlite3_exec(db, demanar_id_tensio, callback_id, &id_tensio, &zErrMsg);
		printf("Id_Tensio es: %d\n\n", id_tensio);





	rc = sqlite3_exec(db, checksensorintensitat, callback_intensitat, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		printf("El sensor anomenat 'Sensor_Intensitat' no està registrat, procedim a enregistrar-lo.\n");
		char entrar_sensor_i[1024] = "INSERT INTO sensors (nom_sensor, descripcio) VALUES('Sensor_Intensitat','Sensor per mesurar intensitat');";

		rc = sqlite3_exec(db, entrar_sensor_i, 0, 0, &zErrMsg);
	}
	rc = sqlite3_exec(db, demanar_id_intensitat, callback_id, &id_intensitat, &zErrMsg);
	printf("Id_Intensitat es: %d\n\n\n", id_intensitat);





	while (1) {
		ret = spiadc_config_transfer(SINGLE_ENDED_CH2, &value_int);

		sensor_nom(value_int, id_tensio, id_intensitat);

		sleep(2);
	}


	return ret;
}
