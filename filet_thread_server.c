#include "basic.h"
#include "filet_thread_server.h"
#include "protocollo.h"
#include "noise.h"
#include "utils.h"

//variabili globali che servono a tutti i thread.
//questa parte di programma l ho scritta molto prima del client e non ero molto pratico del C.
//non penso che sia sbagliato utilizzare le variabili globali visto e considerato che ogni thread le utilizzera.
int exit_failure = EXIT_FAILURE, exit_success = EXIT_SUCCESS;
unsigned long int actual_total_thread = 0;//numero totale di thread
unsigned int actual_free_thread = 0;///numero di thread che non stanno lavorando
unsigned int actual_zombie_thread = 0;//numero di thread "morti" ma non deallocati (joinati)
unsigned char esci=0;//ricevuto il comando di uscta, lo propago a tutti i thread
pthread_t threadtoclose=0;//thread id del thread che ha finito il proprio compito e attende di essere joinato
pthread_mutex_t mutex_accept = PTHREAD_MUTEX_INITIALIZER;//un thread alla volta accettera connessione ovvero solamente il thread che ha questo mutex
//mutex per accesso esclusivo (per modificarle)alle rispettive variabili
pthread_mutex_t mutex_actual_free_thread = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_actual_zombie_thread = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_tojoin = PTHREAD_MUTEX_INITIALIZER;
//condition e mutex per il thread clieaner, attendera finche non ci sara un thread che attende di essere joinato e che lo svegliera
pthread_cond_t cond_tojoin = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_cond_tojoin = PTHREAD_MUTEX_INITIALIZER;

// questro thread ha il compito di joinare i thread che hanno terminato
void *thread_cleaner(void *arg)
{
        arg = arg;
        printf("Thread cleaner %lu creato \n", (unsigned long)pthread_self());
	for(;;){
		//dorme finche un thread che deve essere joinato non lo sveglia.
		if(pthread_mutex_lock(&mutex_cond_tojoin)!=0){
			perror("errore in mutex cond tojoin thread cleaner");
			exit(EXIT_FAILURE);
		}
		if(esci!=0){
		pthread_exit(&exit_success);
		}
		if(pthread_cond_wait(&cond_tojoin,&mutex_cond_tojoin)!=0){
			perror("errore in cond wait thread cleaner");
			exit(EXIT_FAILURE);
		}
		pthread_t thread=threadtoclose;
		threadtoclose=0;//il thread che attende di essere joinato capisce che il thread cleaner e quasi certamente pronto a joinare
		if(pthread_join(thread,NULL)!=0){
			perror("errore in pthread close thread cleander");
			exit(EXIT_FAILURE);
		}
		if(pthread_mutex_unlock(&mutex_cond_tojoin)!=0){
			perror("errore in mutex cond tojoin thread cleaner");
			exit(EXIT_FAILURE);
		}
	}
        pthread_exit(&exit_failure);
}

//nel aso del thread init o thread init zombie, vanno aggiornati i contatori ed eventualmente creati altri thread
void thread_init()
{
	pthread_mutex_lock(&mutex_actual_free_thread);
	actual_free_thread++;
	pthread_mutex_unlock(&mutex_actual_free_thread);
	if (actual_free_thread + actual_zombie_thread < (int)CONST_FREE_THREAD) {
		pthread_t mainthread;
		if (pthread_create(&mainthread, NULL, thread_function, NULL)) {
			printf("error creating server thread \n");
			pthread_exit(&exit_failure);
		}
	}
}

void thread_init_zombie()
{
	pthread_mutex_lock(&mutex_actual_zombie_thread);
	actual_zombie_thread++;
	pthread_mutex_unlock(&mutex_actual_free_thread);
}

void *thread_function(void *arg)
{
	arg = arg;
	thread_init();
	pthread_t tid = pthread_self();
	int connsd;
	struct sockaddr_in servaddr, cliaddr;
	unsigned int len;
	char buff[MAXLINE];
	int zombie = 0;
	int optval = 1;

	printf("Thread id %lu creato \n", (unsigned long)tid);
	// apro il  socket
	if ((connsd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("errore in socket");
		pthread_exit (&exit_failure);
	}
	//imposto le opzioni (nella relazione si parlera molto di SO_REUSEPORT)
	if (setsockopt(connsd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval))< 0) {
		perror("errore in setsockopt");
		pthread_exit (&exit_failure);
	}

	memset((void *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	for (;;) {
		//solo un thread alla volta puo accedere a questa porzione di codice, solo un thread alla volta puo accettare la connessione di un client
		pthread_mutex_lock(&mutex_accept);
		//assegno l indirizzo(cioe da qualsiasi indirizzo e porta in questo caso) al socket
		if ((bind(connsd,(struct sockaddr *)&servaddr, sizeof(servaddr))) < 0) {
			perror("errore in bind");
			return (&exit_failure);
		}
		len = sizeof(cliaddr);
		printf("attesa di connessione\n");
		if ((accept_connections(connsd, buff, MAXLINE, 0, (struct sockaddr *)&cliaddr,&len)) < 0) {
			perror("errore in accept");
			return (&exit_failure);
		}
		pthread_mutex_unlock(&mutex_accept);
		//se ci sono abbastanza client liberi ma pochi zombie, diventera uno zombie, altrimenti si chiudera e attendera di essere joinato dal thread cleaner
		if ((zombie == 0) && (actual_free_thread > 0)) {
			pthread_mutex_lock(&mutex_actual_free_thread);
			if (actual_free_thread > 0) {
				actual_free_thread--;
			}
			pthread_mutex_unlock(&mutex_actual_free_thread);
		} else if ((zombie == 1) && (actual_zombie_thread > 0)) {
			pthread_mutex_lock(&mutex_actual_zombie_thread);
			if (actual_zombie_thread > 0) {
				actual_zombie_thread--;
			}
			pthread_mutex_unlock(&mutex_actual_zombie_thread);
		}

		printf("Tid:%lu - %s:%d connesso\n", (unsigned long)tid,
		//ip in binario e cambia indianess porta
		inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
		str_srv_echo(connsd);	/* svolge il lavoro del server */

		if (close(connsd) == -1) {
			perror("errore in close");
			return (&exit_failure);
		}
		if (actual_free_thread < (int)CONST_FREE_THREAD) {
			zombie = 0;
			thread_init();
		} else if (actual_zombie_thread < (int)CONST_MAX_ZOMBIE_THREAD) {
			zombie = 1;
			thread_init_zombie();
		}
	}
	if(pthread_mutex_lock(&mutex_tojoin)!=0){
		perror("errore in mutex cond tojoin thread cleaner");
		exit(EXIT_FAILURE);
	}
	threadtoclose=tid;
	if(pthread_cond_broadcast(&cond_tojoin)!=0){
		perror("errore in cond wait thread cleaner");
		exit(EXIT_FAILURE);
	}
	//devo esser certo che il thread cleaner abbia finito prima di liberare il mutex tojoin. il polling che ho usato non mi piace molto ma  non penso possa essere un collo di bottiglia.
	while(threadtoclose!=0){
		usleep(10000);
	}
	if(pthread_mutex_unlock(&mutex_tojoin)!=0){
		perror("errore in mutex cond tojoin thread cleaner");
		exit(EXIT_FAILURE);
	}
	pthread_exit(&exit_success);
}
//funzione per la disconnessione, manda il codice di disconnessione
void comandodisconnetti(int sockd){
	unsigned char comandofine[2];
        comandofine[0]= (unsigned char)254;
        comandofine[1]=(unsigned char)250;
	noisy_writen(sockd,&comandofine,2*sizeof(unsigned char));
}
//client principale
void str_srv_echo(int sockd){
	//path principale
	char *path=malloc(PATHLEN*sizeof(char));
	if(path==NULL){
		perror("errore in path str srv echo");
		pthread_exit(&exit_failure);

	}
	memcpy(path,"./\0",3*sizeof(char));
	//pthread_t* workingthread=NULL;
	for(;;){
		unsigned char comando;
		for(;;){
			//menu
			getcommand(sockd,(unsigned char*)&comando);//funzione che attende la ricezione di un comando dal thread
			if(VERBOSE==1){
				printf("entro nello switch con comando %u\n",(unsigned int)comando);
			}
			switch(comando){
				case (unsigned char)255:
					if(VERBOSE==1){
						printf("ricevuto comando ls\n");
					}
					comandols(sockd,path);
					break;
				case (unsigned char)254:
					comandoput(sockd,path);
					break;
				case (unsigned char)253:
					if(VERBOSE==1){
						printf("ricevuto comando get\n");
					}
					comandoget(sockd,path);
					break;
				case (unsigned char)252:
					comandocd(sockd,path);
					break;
				case (unsigned char)251:
					return;
				default:
					continue;
			}
		}
	}
}

//funzione per cambiare il path attuale del server
void comandocd(int sockfd,char* path){
	if(VERBOSE==1){
		printf("entro in comandocd\n");
	}
	if(PATHLEN-strlen(path)-1<1){//controllo se c e ancora spazio 
		perror("errore in comando PATHLEN comandocd");
		pthread_exit(&exit_failure);
	}
	//ricevo lunghezza del path
	uint32_t pathlength;
	ptc_read(sockfd,(void*)&pathlength,sizeof(uint32_t),NULL,0);
	// 2 byte in piu per aggiunge / e \0
	char directory[pathlength+2];
	if((int)PATHLEN-(int)strlen(path)-1-(int)pathlength-1<0){
		perror("overflow PATHLEN in comandocd");
	}
	//ricevo il nome
	if(maxnum(sizeof(size_t))<pathlength){
		perror("errore overflow comando put 1");
		exit(EXIT_FAILURE);
	}
	ptc_read(sockfd, directory, (size_t)pathlength, NULL,1);	
	directory[pathlength]='/';
	directory[pathlength+1]='\0';
	memcpy(path+strlen(path),directory,pathlength+1);	
}
//funzione per ls
void comandols(int sockd,char* path){
	if(VERBOSE==1){
		printf("entro in comandols\n");
	}
	char stringa[PATHLEN+3] ;
	strcat(stringa,"ls ");
	strcat(stringa, path);
	//popen in lettura per leggere il ritorno del ls.
	FILE* f;
	if((f=popen(stringa ,"r"))==NULL){ //CHIUSO
		comandodisconnetti(sockd);
		perror("errore in popen comandols");
		pthread_exit(&exit_failure);
	}
	unsigned char* buffer=malloc(MAXLINE*sizeof(unsigned char));//LIBERATO
	if(buffer==NULL){
		comandodisconnetti(sockd);
		perror("errore in buffer comandols");
		exit(EXIT_FAILURE);
	}
	uint32_t i;
	uint32_t massimo=MAXLINE;
	int errorcheck;
	//devo prima svuotare la pipe per sapere quanto e grande!
	for(i=0;((errorcheck=fgetc(f))!=EOF);++i){ //TODO check errore fgetc
		*(buffer+i)=(unsigned char)errorcheck;
		if(i==massimo-1){
			if(realloc(buffer,(i+massimo)*sizeof(unsigned char))==NULL){//LIBERATO
				comandodisconnetti(sockd);
				perror("errore in realloc comandols");
				pthread_exit(&exit_failure);
			}
			massimo=i+massimo;
		}
	}	
	//chiude la pipe
	if(pclose(f)==-1){
		comandodisconnetti(sockd);
		perror("errore in pclose comandols");
		pthread_exit(&exit_failure);
	}
	//scrivo la grandezza
	ptc_write(sockd,(void*)&i,sizeof(uint32_t),NULL,0);
	//scrivo la pipe (che ho messo nel buffer)
	ptc_write(sockd,buffer,i,NULL,1);
	free(buffer);
}

//la funzione che legge il comando che viene mandato dal client
void getcommand(int sockfd, unsigned char* ptr){
		char buff[2] ;
	ptc_read(sockfd,buff,2*sizeof(char),NULL,1);
	*ptr = buff[1];
}
//funzione che gestisce il put (VISTO DA PARTE DEL CLIENT!)
//speculare alla funzione presente nel client
void comandoput(int sockd, char* path){ // ovvero il client vuole mandare  un file
//	size_t* scritti=malloc(sizeof(size_t));//LIBERATO
//	if(scritti==NULL){
//		comandodisconnetti(sockd);
//		perror("errore in thread get malloc");
//		pthread_exit(&exit_failure);
	
//	}
//	*scritti=0;

	uint32_t namelength;
	ptc_read(sockd,(void*)&namelength,sizeof(uint32_t),NULL,0);
	char nomefile[namelength];
	if(maxnum(sizeof(size_t))<namelength){
		perror("errore overflow comando put 1");
		exit(EXIT_FAILURE);
	}
	ptc_read(sockd,nomefile,(size_t)namelength,NULL,1);
	uint32_t length=0;
	ptc_read(sockd,(char*)&length,sizeof(uint32_t),NULL,0);
	if(namelength+strlen(path)+3>PATHLEN){
		comandodisconnetti(sockd);
		perror("overflor max path name comandoput");
		pthread_exit(&exit_failure);
	}
	char *filepath;
	char arrayfilepath[namelength+strlen(path)+1];
	filepath=(char*)arrayfilepath;
	memcpy(filepath,path,strlen(path));
	memcpy(filepath+strlen(path),nomefile,namelength);
	filepath[namelength+strlen(path)]='\0';
        unsigned char esiste=(unsigned char)0;
        while(file_exists(filepath)==1){
                char* tofree=filepath;
                char* array=malloc(strlen(filepath)+3);//LIBERATO
                if(array==NULL){
			comandodisconnetti(sockd);
                        perror("errore in nomefile ptr maloc thread get");
                        pthread_exit(&exit_failure);

                }
                memcpy(array,filepath,strlen(filepath));
                array[strlen(filepath)]='_';
                array[strlen(filepath)+1]='2';
                array[strlen(filepath)+2]='\0';
                filepath=array;
                if(esiste==1){
                        free(tofree);
                }
                esiste=(unsigned char)1;
        }
	int fd = open(filepath,O_CREAT | O_RDWR, 0666);//CHIUSO
	if (fd==-1){
		comandodisconnetti(sockd);
		perror("errore open comandoget");	
		pthread_exit(&exit_failure);
	}
        if(ftruncate(fd,length)!=0){
		comandodisconnetti(sockd);
                perror("errore in ftruncate");
                pthread_exit(&exit_failure);
        }
	char* buffer;
	if((buffer=(char*)mmap(NULL,length,PROT_READ | PROT_WRITE,MAP_SHARED,fd,0))==MAP_FAILED){//LIBERATO
		comandodisconnetti(sockd);
		perror("errore in mmap comandoget");
		pthread_exit(&exit_failure);
	}
        if(maxnum(sizeof(size_t))<length){// controllo overflow
                fprintf(stderr,"overflow in thread put");
                exit(EXIT_FAILURE);
        }
	ptc_read(sockd,buffer,(size_t)length, NULL,1);
	printf("Success\n");
	
	if(munmap(buffer,length)!=0){
		comandodisconnetti(sockd);
		perror("errore in munmap comandoput");
		pthread_exit(&exit_failure);
	}
	if(close(fd)==-1){
		comandodisconnetti(sockd);
		perror("errore in close comandoput");
		pthread_exit(&exit_failure);
	}
//	free(scritti);
	if(esiste==1)
		free(filepath);
}


//funzione per il get (VISTO DA PARTE DEL CLIENT)
//Speculare alla funziona put del client
void comandoget(int sockd,char* path){ // ovvero il client ha richiesto un file
//	size_t* scritti=malloc(sizeof(size_t));//LIBERATO
//	if(scritti==NULL){
//		comandodisconnetti(sockd);
///		perror("errore in thread get malloc");
//		pthread_exit(&exit_failure);
//	}
//	*scritti=0;
	uint32_t namelength;
	ptc_read(sockd,(void*)&namelength,sizeof(uint32_t),NULL,0);
	char nomefile[strlen(path)+namelength+1];
	 if(maxnum(sizeof(size_t))<namelength){// controllo overflow
                fprintf(stderr,"overflow in thread get");
                exit(EXIT_FAILURE);
        }
	ptc_read(sockd,nomefile+strlen(path),(size_t)namelength,NULL,1);
	nomefile[strlen(path)+namelength]='\0';
	memcpy(nomefile,path,strlen(path));
	struct stat sb;
	int fd = open(nomefile, 400 );
	if(fd==-1){
		comandodisconnetti(sockd);
		perror("errore in open compandoget");
		pthread_exit(&exit_failure);
	}
	if(fstat(fd,&sb)==-1){
		comandodisconnetti(sockd);
		perror("errore in fstat comandoget");
		pthread_exit(&exit_failure);
	}
        if(maxnum(sizeof(size_t))<(uintmax_t)sb.st_size){// controllo overflow nel casting di un off_t come size_t
             //   if(VERBOSE==1){
//			printf("maxnum:%ju,filesize:%ju \n",maxnum(sizeof(size_t)),(uintmax_t)sb.st_size);
//		}
		fprintf(stderr,"overflow in thread get2\n");
                exit(EXIT_FAILURE);
        }
        uint32_t sbstsize=(uint32_t)sb.st_size;
        if(maxnum(sizeof(uint32_t))<(size_t)sb.st_size || sb.st_size<0){
                perror("overflow thread get3");
                exit(EXIT_FAILURE);
        }
	ptc_write(sockd,(char*)&sbstsize,sizeof(uint32_t),NULL,0);
	char* buffer;
	if((buffer=(char*)mmap(NULL,sb.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED){//LIBERATO
		comandodisconnetti(sockd);
		perror("errore in mmap comandoget");
		pthread_exit(&exit_failure);
	}
	ptc_write(sockd,buffer,sb.st_size,NULL,1);
	printf("Success");
	if(munmap(buffer,sb.st_size)!=0){
	comandodisconnetti(sockd);	
		comandodisconnetti(sockd);
		perror("errore in munmap comandoget");
		pthread_exit(&exit_failure);
	}
	if(close(fd)==-1){
		comandodisconnetti(sockd);
		perror("errore in close comandoget");
		pthread_exit(&exit_failure);
	}
//	free(scritti);
}
