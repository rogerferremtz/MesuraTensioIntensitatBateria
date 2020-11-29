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
#include <sqlite3.h>
#include <wiringPi.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>


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

//Defines necessaris per encendre LEDs, més colors al PDF d'eussternet.
#define EXPORT "/sys/class/gpio/export"
#define UNEXPORT "/sys/class/gpio/unexport"
#define LEDYELLOW "/sys/class/gpio/gpio27/value"
#define D_LEDYELLOW "/sys/class/gpio/gpio27/direction"

/*
 * #define LEDWHITE "/sys/class/gpio/gpio22/value"
 * #define LEDRED "/sys/class/gpio/gpio17/value"
 * #define LEDYELLOW "/sys/class/gpio/gpio27/value"
 * 
 * #define D_LEDWHITE "/sys/class/gpio/gpio22/direction"
 * #define D_LEDRED "/sys/class/gpio/gpio17/direction"
 * #define D_LEDYELLOW "/sys/class/gpio/gpio27/direction"
 */



//Prototipus funcions.
int cridarsql(float, float, int, int);
typedef void (timer_callback) (union sigval);

// -----------------------------------------------------------------------------------------------


int set_timer(timer_t * timer_id, float delay, float interval, timer_callback * func, int * data) 
{
    int status =0;
    struct itimerspec ts;
    struct sigevent se;

    se.sigev_notify = SIGEV_THREAD;
    se.sigev_value.sival_ptr = data;
    se.sigev_notify_function = func;
    se.sigev_notify_attributes = NULL;

    status = timer_create(CLOCK_REALTIME, &se, timer_id);

    ts.it_value.tv_sec = abs(delay);
    ts.it_value.tv_nsec = (delay-abs(delay)) * 1e09;
    ts.it_interval.tv_sec = abs(interval);
    ts.it_interval.tv_nsec = (interval-abs(interval)) * 1e09;

    status = timer_settime(*timer_id, 0, &ts, 0);
    return 0;
}


//Funció per encendre el LED.
void led_on(char addr[])
{
	int fd;
	char m[] = "1";
	fd = open(addr, O_WRONLY);
	if (fd < 0) {
		perror("Error a l'obrir el dispositiu\n");
		exit(-1);
	}
	write(fd,m,1);
	close(fd);
}



//Funció per apagar el LED.
void led_off(char addr[])
{
	int fd;
	char m[] = "0";
	fd = open(addr, O_WRONLY);
	if (fd < 0) {
		perror("Error a l'obrir el dispositiu\n");
		exit(-1);
	}
	write(fd,m,1);
	close(fd);
}



//Posta a punt dels LEDs.
void wfv(char addr[], char message[])
{
	int fd;
	fd = open(addr, O_WRONLY);
	char m_error[200];
	sprintf(m_error,"Error a l'obrir el dispositiu %s\n",addr);
	if (fd < 0) {
		perror(m_error);
		exit(-1);
	}
	write(fd,message,strlen(message));
	// printf("missatge %s, mida %d\n",message, strlen(message));
	close(fd);
}



//Posta a punt del canal de comunicació GPIO per comunicar-nos amb el LED.
void setup_gpio ()
{
	int fdtest,n=10000;
	wfv(EXPORT,"27");	//Port LED Groc.
	//wfv(EXPORT,"22");	//Port LED Blanc.
	//wfv(EXPORT,"17");	//Port LED Vermell.


	//En cas d'haver-hi més LEDs, cal repetir el DO-WHILE tres cops, un per cada LED.
	do{
		n--;
		fdtest = open(LEDYELLOW, O_WRONLY);
	} while((n>0) && (fdtest<0));

	printf("---> Export Ok %d %d \n",n, fdtest);




	//Definim la direcció de sortida dels LEDs
	wfv(D_LEDYELLOW,"out");
	//wfv(D_LEDWHITE,"out");
	//wfv(D_LEDRED,"out");
}



//Alliberem el/s canal/s GPIO.
void free_gpio ()
{
	wfv(UNEXPORT,"27");
	//wfv(UNEXPORT,"22");
	//wfv(UNEXPORT,"17");
}



static void pabort(const char *s)
{
	perror(s);
	abort();
}



//Aquest Callback ens diu si el sensor de TENSIÓ està registrat.
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



//Aquest Callback ens diu si el sensor d'INTENSITAT està registrat.
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



//Aquest callback ens retorna el valor del ID del sensor que li haguem demanat.
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





// ---------------------------Configuració del ADC MCP 3008.---------------------------------------



//
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



//Funció encarregada de llegir el valor que proporciona el ADC. Aquesta funció s'executa un cop co-
//neixem el ID del sensor. La funció al final crida a una altra funció encarregada d'introduïr els 
//valors dels sensors a la base de dades.
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

	//cridarsql(id_sensor, id_intensitat, value_volts, intensitat);
	cridarsql(value_volts, intensitat, id_tensio, id_intensitat);
	return 0;
}



//Funció encarregada d'introduïr els valors dels sensors a la base de dades.
int cridarsql(float tensio, float intensitat, int id_tensio, int id_intensitat) {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

	rc = sqlite3_open("basedades_adstr.db", &db);

	if (rc != SQLITE_OK) {
		  fprintf(stderr, "Cannot open database.\n");
		  return 1;
	}


	//Introduïm el valor de tensió.
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


	//Introduïm el valor d'intensitat.
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





void callback(union sigval si)
{
	float value_int;
	int ret = 0;

	int * msg = (int *) si.sival_ptr;

	int id_tensio = msg[0];
	int id_intensitat = msg[1];

	ret = spiadc_config_transfer(SINGLE_ENDED_CH2, &value_int);

	sensor_nom(value_int, id_tensio, id_intensitat);

	setup_gpio();
	printf("Encenem LED groc.\n");
	led_on(LEDYELLOW);
	sleep(1);

	printf("Apaguem LED groc.\n");
	led_off(LEDYELLOW);
	free_gpio;
}



//Funció main, la primera part d'aquesta s'encarrega de revisar que els sensors estiguin registrats a la
//base de dades, en cas que no ho siguin, els registra. Un cop registrats demana el seu ID. La segona part
//executa el bucle en el qual cridem a la funció que configura el ADC, cridem a la funció de llegeis els 
//valors dels sensors, i finalment encenem i apaguem el LED.
int main (int argc, char *argv[])
{
	//int ret = 0, value_int;

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


	int ids[2];
	ids[0] = id_tensio;
	ids[1] = id_intensitat;


	timer_t tick;
	set_timer(&tick, 3, 5, callback, (int *) ids );
	getchar();


	/*while (1) {
		ret = spiadc_config_transfer(SINGLE_ENDED_CH2, &value_int);

		sensor_nom(value_int, id_tensio, id_intensitat);

		setup_gpio();
		printf("Encenem LED groc.\n")
		led_on(LEDYELLOW);
		sleep(1);

		printf("Apaguem LED groc.\n")
		led_off(LEDYELLOW);
		free_gpio;

		sleep(2);
	}*/


	return 0;
}
