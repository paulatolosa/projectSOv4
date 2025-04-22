#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <mysql/mysql.h>
#include <pthread.h>

typedef struct {
	char nombre[20];
	int socket;
}Conectado;

typedef struct{
	Conectado conectados[100];
	int num;
}ListaConectados;

// lista
ListaConectados milista;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex para sincronizar el acceso a la lista
int numsock; // n鷐eros de sockets
int sockets[100];

int PonConectados (ListaConectados *lista, char nombre[20], int socket)
{
	if(lista-> num == 100)
		return -1;
	else{
		strcpy(lista->conectados[lista->num]. nombre, nombre);
		lista-> num++;
		return 0;
	}
}

int DamePosicion (ListaConectados *lista, char nombre[20])
{
	int i =0;
	int encontrado =0;
	while ((i<lista-> num)&&!encontrado)
	{
		if (strcmp(lista->conectados[i].nombre, nombre))
			encontrado =1;
		if (!encontrado )
			i=i+1;
	}
	if (encontrado)
	{
		return i;
	}
	else 
		return -1;
}

int Eliminar(ListaConectados *lista, char nombre[20])
{
	int pos = DamePosicion (lista, nombre);
	if (pos == -1)
	{
		return -1;
	}
	else
		{
		int i;
		for(i=pos; i<lista->num-1; i++)
		{
			lista->conectados[i] = lista->conectados[i+1];
/*			strcpy(lista.conectados[i].nombre, lista.conectados[i+1].nombre);*/
/*			lista-> conectados[i].socket = lista-> conectados[i+1].socket ;*/
		}
		lista-> num--;
		return 0;
	}
}

void DameConectados (ListaConectados *lista, char conectados[300])
{
	sprintf (conectados, "%d,", lista->num);
	int i;
	for(i=0; i<lista->num; i++)
	{
		sprintf (conectados, "%s%s,", conectados, lista->conectados[i].nombre);
	}
	conectados[strlen(conectados)-1] = '\0';
	strcat(conectados, "/");
}


void *AtenderCliente(void *sock)
{
	int sock_conn;
	int *s;
	s = (int *) sock;
	sock_conn = *s;
	
	char peticion[600];
	char respuesta[600];
	
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	
	int ret;
	int err;
	char consulta[256];
	char insertar[256];
	
	MYSQL *conn;
	conn = mysql_init(NULL);
	
	


	
	conn = mysql_real_connect(conn, "shiva2.upc.es", "root", "mysql", "M2_BBDD", 0, NULL, 0);
	if (conn == NULL)
	{
		printf("Error al inicializar la conexi贸n: %u %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}
	
	err = mysql_query(conn, "USE M2_BBDD;");
	if(err != 0) 
	{
		printf("Error al seleccionar la base de datos: %u %s\n", mysql_errno(conn), mysql_error(conn));
		exit(1);
	}
	
	
int terminar = 0;
	while (terminar == 0) 
	{
		ret = read(sock_conn, peticion, sizeof(peticion));
		printf("Recibido\n");
		peticion[ret] = '\0';
		printf("Peticion: %s\n", peticion);
		
		
		char userID[20];
		char password[40];
		char playername[30];
		char *p = strtok(peticion, "/");
		int codigo = atoi(p);
		if ((codigo<3)&&(codigo != 0)) 
		{
			p = strtok(NULL, "/");
			if (p != NULL) strcpy(userID, p);
			p = strtok(NULL, "/");
			if (p != NULL) strcpy(password, p);
			p = strtok(NULL, "/");
			if (p != NULL) strcpy(playername, p);
			printf("Codigo: %d, UserID: %s, Password: %s, Playername: %s\n", codigo, userID, password, playername);
		}
		
		if (codigo == 0)//Desconectar
		{
			printf("se ha desconectado %s\n",userID);
			sprintf(respuesta,"0/Se ha desconectado\n");
			write (sock_conn,respuesta, strlen(respuesta));
			terminar = 1;
		}
		else if (codigo == 1) //INICIAR SESION
		{
			sprintf(consulta, "SELECT USERID, PASSWORD_P FROM player WHERE USERID = '%s' AND PASSWORD_P = '%s'", userID, password);
			err = mysql_query(conn, consulta);
			if (err != 0) {
				printf("Error al consultar datos de la base: %u %s\n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			
			resultado = mysql_store_result(conn);
			row = mysql_fetch_row(resultado);
			if (row == NULL) {
				sprintf(respuesta,"1/El usuario o la contrasena no son correctos\n");
			} else {
				sprintf(respuesta,"2/Se ha iniciado sesion correctamente, bienvenido %s\n", row[0]);
				// Bloqueamos el mutex antes de modificar la lista
				pthread_mutex_lock(&mutex);
				PonConectados(&milista, userID, sock_conn);
				pthread_mutex_unlock(&mutex); // Desbloqueamos el mutex
			}
			write (sock_conn,respuesta, strlen(respuesta));
			
			//REGISTRAR
		} else if (codigo == 2) {
			sprintf(insertar, "INSERT INTO player (PLAYERNAME, USERID, PASSWORD_P) VALUES ('%s', '%s', '%s')", playername, userID, password);
			err = mysql_query(conn, insertar);
			if (err != 0) {
				sprintf(respuesta, "Error al insertar datos en la base: %u %s\n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado = mysql_store_result(conn);
			row = mysql_fetch_row(resultado);
			sprintf(respuesta, "2/Se ha registrado correctamente: %s\n", resultado);
			write (sock_conn,respuesta, strlen(respuesta));
			//QUERY: ENCONTRAR LOS JUGADORES QUE EN UN ID PARTIDA DADA
		} else if (codigo == 3) {
			char gameID[20];
			char *c = strtok(NULL, "/");
			strcpy(gameID,c);
			printf("Dame el ID de la partida que quieres buscar: ");
			sprintf(consulta, "SELECT WINNER, LOSER FROM game WHERE GAMEID = '%s'", gameID);
			err = mysql_query(conn, consulta);
			if (err != 0) {
				printf( "Error al consultar datos de la base: %u %s\n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado = mysql_store_result(conn);
			row = mysql_fetch_row(resultado);
			if (row == NULL) {
				sprintf(respuesta, "3/No se han obtenido datos\n");
			} else {
				sprintf(respuesta, "3/Nombre de las personas con las que has jugado: %s, %s\n", row[0], row[1]);
			}
			mysql_free_result(resultado);
			write (sock_conn,respuesta, strlen(respuesta));
			////QUERY: LAS PARTIDAS QUE HE JUGADO EN EL DIA INTRODUCIDA
		} else if (codigo == 4) {
			int date;
			char *c = strtok(NULL, "/");
			date = atoi(c);
			printf("Dime el data de las partidas que quieres buscar: ");
			
			sprintf(consulta, "SELECT * FROM game WHERE GAME_DATE = %d", date);
			err = mysql_query(conn, consulta);
			if (err != 0) {
				printf("Error al consultar datos de la base: %u %s\n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado = mysql_store_result(conn);
			
			if (mysql_num_rows(resultado) == 0) {
				sprintf(respuesta,"4/No se han obtenido datos en la consulta\n");
			} else {
				while ((row = mysql_fetch_row(resultado)) != NULL) {
					char temp[256];
					sprintf(temp, "4/GAMEID: %s, GAME_DATE: %s, GAME_TIME: %s, DURATION: %s, WINNER: %s, LOSER: %s\n",
							row[0], row[1], row[2], row[3], row[4], row[5]);
					strcat(respuesta, temp);
				}
				mysql_free_result(resultado);
			}
			write (sock_conn,respuesta, strlen(respuesta));
		}
		//CONSULTAR LOS DETALLES DE LOS PARTIDOS CON EL JUGADOR INTRODUCIDO
		else if (codigo == 5) {
			char playername5[50];
			char *c = strtok(NULL, "/");
			strcpy (playername5,c);
			sprintf(consulta, "SELECT * FROM game WHERE WINNER = '%s' OR LOSER = '%s'", playername5, playername5);
			err = mysql_query(conn, consulta);
			if (err != 0) {
				printf("Error al consultar datos de la base: %u %s\n", mysql_errno(conn), mysql_error(conn));
				exit(1);
			}
			resultado = mysql_store_result(conn);
			if (resultado == NULL) {
				sprintf(respuesta, "5/No se encontraron partidas para el jugador %s\n", playername5);
			} else {
				
				while ((row = mysql_fetch_row(resultado)) != NULL) {
					sprintf(respuesta,"5/GAMEID: %s, GAME_DATE: %s, GAME_TIME: %s, DURATION: %s, WINNER: %s, LOSER: %s\n",
							row[0], row[1], row[2], row[3], row[4], row[5]);
				}
				mysql_free_result(resultado);
			}
			write (sock_conn,respuesta, strlen(respuesta));
		}
		// 
		//Invitar
		else if ((codigo == 6) || (codigo==65))
		{
			char playernam6[50];
			char resinvitado[50];
			if(codigo ==6)
			{
				char *c = strtok(NULL, "/");
				strcpy (playernam6, c);
				if(playernam6 == NULL)
				{
					printf ("Error, no ha llegado el nombre.\n");
					sprintf (respuesta, "Error, no ha llegado el nombre.\n");
				}
				else 
				{
					sprintf (respuesta,"6/%s,%s", playernam6, playername);
					printf ("la persona que quiere invitar: %s, %s\n", playernam6,respuesta );
				}
				//enviar a todos los conectados
				int j;
				for(j=0; j<numsock;j++)
				{
					write (sockets[j], respuesta, strlen(respuesta));
				}
				
			}
			else if (codigo == 65)
			{
				char *c = strtok(NULL, "/");
				strcpy (resinvitado, c);
				if(resinvitado == "yes")
				{
					printf ("Jugador %s, %s ha aceptado tu invitacion", playername, playernam6);
					sprintf (respuesta,"65/yes" );
				}
				else
				   {
					printf ( "Jugador %s, %s ha rechazado tu invitacion", playername, playernam6);
					sprintf (respuesta,"65/no");
				}
				//enviar a todos los conectados
				int j;
				for(j=0; j<numsock;j++)
				{
					write (sockets[j], respuesta, strlen(respuesta));
				}
			}
			
		}
		
		char misContactos[400];
		// Bloqueamos el mutex antes de leer la lista
		pthread_mutex_lock(&mutex);
		DameConectados(&milista, misContactos);
		pthread_mutex_unlock(&mutex); // Desbloqueamos el mutex
		char notificacion[100];
		sprintf(notificacion, "7/%s", misContactos);
		printf("%s\n",notificacion);
		int j;
		for(j=0; j<numsock;j++)
		{
			write (sockets[j], notificacion, strlen(notificacion));
		}
		//}
		
		
			
			printf ("Respuesta: %s\n", respuesta);
			// Enviamos respuesta
			
	

}
close(sock_conn);
}



int main(int argc, char *argv[])
{
    int sock_conn, sock_listen, ret;
    struct sockaddr_in serv_adr;
	int puerto = 50007;
	milista.num=0;
	
    // Crear socket de escucha
    if ((sock_listen = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
        printf("Error al crear socket\n");
        exit(1);
    }

    // Inicializar la estructura de direcci贸n del servidor
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(puerto);

    // Vincular el socket a la direcci贸n y puerto
    if (bind(sock_listen, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0)
	{
        printf("Error en el bind\n");
        exit(1);
    }

    // Escuchar conexiones entrantes
    if (listen(sock_listen, 3) < 0)
	{
        printf("Error en el listen\n");
        exit(1);
    }


	pthread_t thread[100];
	for (;;)
	{ 
		printf("Escuchando...\n");
		sock_conn = accept(sock_listen, NULL, NULL);
		printf("He recibido conexi贸n\n");
		
		sockets[numsock]= sock_conn; 
		pthread_create (&thread[numsock], NULL, AtenderCliente,&sockets[numsock]);
		numsock++;
	}
	
/*	for (i=0;i<5;i++)*/
/*	{*/
/*		pthread_join (&thread[i], NULL);*/
/*	}*/
}

