#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <netdb.h>

#include "func.h"


/**Definim la mida màxima del missatge*/
#define REQUEST_MSG_SIZE	1024

/**Indiquem el port per defecte del servidor web*/
#define SERVER_PORT_NUM_WEB		80

/**Establim la mida de la resposta*/
#define REPLY_MSG_SIZE 1024

/**Indiquem el port per defecte per smtp*/
#define SERVER_PORT_NUM_SMTP 25


int http_get(char *nom_servidor, char *cadena_URI)
{
	char resposta_header[4256];
	char resposta_data[5256];

	struct		sockaddr_in serverAddr;
	char		serverName[32];					//Adreça IP on està el servidor web

	sprintf(serverName, "%s", nom_servidor);	//Posem al array "serverName" el que teníem a "nom_servidor".

	int		sockAddrSize;
	int		sFd;
	int		result;
	char	buffer[4256];
	char	missatge[REQUEST_MSG_SIZE];

	sprintf(missatge, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", cadena_URI, nom_servidor);	//Completem el missatge amb la cadena URI sel·leccionada anteriorment.

	struct hostent *he;

	he = gethostbyname(nom_servidor);

	if (he == NULL) {
		herror("gethostbyname");
		exit(1);
	}

	sFd = socket(AF_INET, SOCK_STREAM, 0);

	if (sFd == -1) {
		perror("socket");
		exit(1);
	}

	/**Construir l'adreça del servidor web*/
	sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char *)&serverAddr, sockAddrSize);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT_NUM_WEB);
	serverAddr.sin_addr = *((struct in_addr *)he->h_addr);

	/** Establir connexió*/
	result = connect(sFd, (struct sockaddr *) &serverAddr, sockAddrSize);

	if (result < 0) {
		printf("Error en establir la connexió\n");
		exit(-1);
	}

	printf("\nConnexió establerta amb el servidor: adreça %s, port %d\n",	inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));

	/**Enviar la petició al servidor*/
	strcpy(buffer, missatge);
	result = write(sFd, buffer, strlen(buffer));
	printf("Missatge enviat a servidor(bytes %d): %s\n", result, missatge);

	/**Rebre la resposta del servidor*/
	result = read(sFd, buffer, 4256);
	printf("Missatge rebut del servidor(bytes %d): %s\n", result, buffer);

	/**Tancar el socket de la connexió establerta*/
	close(sFd);

 	//Separem les dades rebudes del servidor en dos blocs METADATA i DATA, amb la informació que ens interessa 
	for (int caracter = 0; caracter < strlen(buffer); caracter++) {	//Dins del missatge rebut del servidor, busquem un final de línia seguit d'un "carriage return" que separa METADATA de DATA.
		if (buffer[caracter - 1] == '\n' && buffer[caracter] == '\r') {
			for (int index_metadata = 0; index_metadata < caracter + 1; index_metadata++)
				resposta_header[index_metadata] = buffer[index_metadata];


			int index = 0;

			for (int index_data = caracter + 2; index_data <= strlen(buffer); index_data++) {
				resposta_data[index] = buffer[index_data];
				index++;
			}
		}
	}

	printf("METADATA: %s\n\n", resposta_header);
	printf("DATA: %s\n", resposta_data);

	return 0;
}

int email(char *nom_servidor, char *email_destinatari, char *email_remitent, char *text_email)
{
	struct sockaddr_in serverAddr;
	int sockAddrSize;
	int sFd;
	int result;
	char BufRead[REPLY_MSG_SIZE];
	char BufWrite[REPLY_MSG_SIZE];


	/**Crear el socket de comunicació amb el servidor de correu*/
	sFd = socket(AF_INET, SOCK_STREAM, 0);

	if (sFd < 0) {
		perror("socket");
		exit(1);
	}

	/**Construeix l'adreça*/
	sockAddrSize = sizeof(struct sockaddr_in);
	bzero((char *)&serverAddr, sockAddrSize);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT_NUM_SMTP);
	serverAddr.sin_addr.s_addr = inet_addr(nom_servidor);

	/**Establint la connexió*/
	result = connect(sFd, (struct sockaddr *) &serverAddr, sockAddrSize);

	if (result < 0) {
		printf("Error en establir la connexió\n");
		exit(-1);
	}

	printf("\nConnexió establerta amb el servidor: adreça %s, port %d\n", inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));


	//Presentació del servidor
	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);

	if (result < 0) {
		perror("Read presentacio:");
		exit(1);
	}

	printf("Rebut(%d): %s\n", result, BufRead);

	//HELO
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "HELO pi25.iotlab.euss.es\n");	//Iniciem la conversació amb HELO.
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));

	if (result < 0) {
		perror("Write");
		exit(1);
	}

	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);
	printf("Rebut(%d): -> %s\n", result, BufRead);


	//MAIL FROM
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "MAIL FROM: %s\n", email_remitent);
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));

	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);
	printf("Rebut(%d): %s\n", result, BufRead);


	//RCPT TO
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "RCPT TO: %s\n", email_destinatari);
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));

	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);
	printf("Rebut(%d): %s\n", result, BufRead);


	//DATA
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "DATA\n");
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));
	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);
	printf("Rebut(%d): %s\n", result, BufRead);


	//Subject
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "Subject: Prova mail Roger Ferré\n");
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));	//No esperem cap resposta de Mercuri.


	//From
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "From: %s\n", email_remitent);
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));	//Tampoc esperem cap resposta de Mercuri.


	//To
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "To: %s\n", email_destinatari);
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));	//Tampoc esperem cap resposta de Mercuri.


	//Text
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "%s\n.\n", text_email);
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));

	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);
	printf("Rebut(%d): %s\n", result, BufRead);


	//QUIT
	memset(BufWrite, 0, REPLY_MSG_SIZE);
	sprintf(BufWrite, "QUIT\n");
	printf("Enviat: %s\n", BufWrite);
	result = write(sFd, BufWrite, strlen(BufWrite));

	memset(BufRead, 0, REPLY_MSG_SIZE);
	result = read(sFd, BufRead, REPLY_MSG_SIZE);
	printf("Rebut(%d): %s\n", result, BufRead);


	close(sFd);

	return 0;
}

