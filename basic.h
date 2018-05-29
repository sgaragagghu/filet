#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>
#include "list.h" // header della lista che ho implementato
#include "echo_io.h" // header del "vecchio" echo_io che ho -in parte- riutilizzato e modificato

//funziona solo con architetture con il char a 8 bit (avrei potuto (dovuto)usare int8_t ma non ero abbastanza esperto, comunque nelle architetture dove il char non è  8 bit probabilmente(se non certamente) non esiste un tipo di dato a 8 bit)
#if CHAR_BIT != 8
#error "I require CHAR_BIT == 8"
#endif


#define SERV_PORT	5193 // porta del server
//#define BACKLOG		10
#define MAXLINE		1024 // massima grandezza, usata in varie parti del programma
#define PACKETSIZE	4 //massima grandezza dei pacchetti in byte
#define PROBERRSEND     5 //probabilita che un pacchetto non venga spedito (di nascosto)
#define PROBERRREC	0 //inutile, e equivalente avere perdite nell invio. non e stato implementato il funzionamento perche speculare ad un paccheto che non viene spedito di nascosto.
#define TIME_OUT 5

#define CONST_FREE_THREAD 50//il programma manterra sempre 50 thread pronti, nel caso di molti client che vogliono connettersi in un brevissimo lasso di tempo
#define CONST_MAX_ZOMBIE_THREAD 5//alcuni thread che hanno temrinato il loro lavoro rimarranno in vita per servire un nuovo client, serve solo a risparmiare nella creazione di nuovi thread
#define WINDOW 10//grandezza della finestra
#define WINLEN 4//lunghezza della massima apertura della fienstra
#define PATHLEN 200//grandezza massima(bytes) del percorso di un file
//#define NAMEMAXLEN 50// 
#define VERBOSE 0//stampare molte informazioni per seguire l esecuzione del programma
#define VERBOSELIST 0//stampare molte informazioni per seguire l esecuzione relativamente alla lista

#ifndef SO_REUSEPORT //in alcuni sistemi ho avuto il problema che questa definizione non e definita di default
#define SO_REUSEPORT 15
#endif
