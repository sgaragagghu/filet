// Continuene il thread client e altri thread ausiliari

#include "basic.h"
#include "filet_thread_client.h"
#include "protocollo.h"
#include "utils.h"
#include "noise.h"
void	str_cli_echo(FILE *fd, int sockd);// thread client
void	receive_debug(FILE *fd, int sockd);//
//ssize_t	writen(int fd, const void *buf, size_t n);
//int	readline(int fd, void *vptr, int maxlen);
int exit_failure = EXIT_FAILURE, exit_success = EXIT_SUCCESS;//solamente per usarli come valore di ritorno dai thread (pthread_exit) ma in realta nel programma non viene poi mai letto.


//thread che stampera la situazione dell upload o download
void *thread_stamp(void* a){
	struct thread_arg_t2* thread_stamparg=(struct thread_arg_t2*)a; //struct passata nel thread che contiene informazioni sulla dimensione della trasmissione quali: stato,dimensione totale e byte trasmessi.
	if(thread_stamparg->completato==NULL){//controllo errore
		perror("errore in completato thread stamp");
		pthread_exit(&exit_failure);
	}//stampo (nel caso che non si abbiano le informazioni sulla dimensione totale), in realta nel programma viene sempre trasmessa la dimensione e quindi si sa a priori. E comunque nella funzione del protocollo non e stata implementata la funzione di trasmissione senza sapere a priori la grandezza (anche se inizialmente avevo in programma di aggiungerla)
	char simboli[]={'/','|','\\','-'};//girare la finta rotella \|/-\|
	int j;
	if(thread_stamparg->totale==NULL){
		for(j=0;;j=(j+1)%4){
			if(thread_stamparg->finito==1){
				printf("%*s",50," ");
				printf("[%c Transmitted:%zu]%*s\n",simboli[j],*thread_stamparg->completato,50," ");
				pthread_exit(&exit_success);
			}
			printf("%*s",50," ");//inserisco 50 volte la stringa composta da uno spazio. per spostare in avanti la scritta
			printf("[%c Transmitted:%zu] Per inserire un comando: premi invio, inserisci il comandi e premi invio entro 10 secondi%*s\r",simboli[j],*thread_stamparg->completato,50," ");
			fflush(stdout);
			sleep(10);	
			while(*thread_stamparg->activethreadid!=*thread_stamparg->chisono){
				// sleep finche' non ritocca a me.
				if(pthread_mutex_lock(thread_stamparg->condmutex)!=0){
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
		
				if(pthread_cond_wait(thread_stamparg->threadcond,thread_stamparg->condmutex)!=0){
					perror("errore in menu pthread cond broadcast");
					exit(EXIT_FAILURE);
	
				}
				if(pthread_mutex_unlock(thread_stamparg->condmutex)!=0){
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
				if(thread_stamparg->finito==1){
					break;
				}


			}
		}
	}
	else{
		for(j=0;;j=(j+1)%4){//come sopra ma con il totale da trasferire. Ho usato intmax_t perche printf non ha un tipo standard per off_t e per evitare incongruenze.. intmax e il piu grande quindi non ci saranno sicuramente problemi
			if(thread_stamparg->finito==1){//se ho finito la trasmissione esco.
				printf("%*s",50," ");
				printf("[%c Transmitted:%zu]%*s\n",simboli[j],*thread_stamparg->completato,50," ");
				pthread_exit(&exit_success);
			}

			printf("%*s",50," ");
			printf("[%c Transmitted:%zu of %jd] Per inserire un comando: premi invio, inserisci il comandi e premi invio entro 10 secondi %*s\r",simboli[j],*thread_stamparg->completato,(intmax_t)*thread_stamparg->totale,50," ");
			fflush(stdout);
			sleep(10);
			while(*thread_stamparg->activethreadid!=*thread_stamparg->chisono){
				// sleep finche' non ritocca a me.
				if(pthread_mutex_lock(thread_stamparg->condmutex)!=0){
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
		
				if(pthread_cond_wait(thread_stamparg->threadcond,thread_stamparg->condmutex)!=0){
					perror("errore in menu pthread cond broadcast");
					exit(EXIT_FAILURE);
	
				}
				if(pthread_mutex_unlock(thread_stamparg->condmutex)!=0){
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
				if(thread_stamparg->finito==1){
					break;
				}

			}
	
		}
		
	}
}
//thread che gestisce la ricezione di un file dal server.(cosi da poter cambiare thread intanto), ovvero il thread client gestisce il menu e altre funzioni base, poi delega la trasmissione ai thread.
void *thread_get(void *a){
	struct thread_arg_t3* thread_arg=(struct thread_arg_t3*)a; //struct passata al client, contiene il socket e il nome del file che bisogna ricevere dal server
	pthread_t stamp_threadid;// thread di stampa
	size_t* scritti=malloc(sizeof(size_t)); //(reminder:LIBERATO) ove necessario si potra passare alla funziona del protocollo di trasmissione in modo da avere informazioni sull andamento della trasmisione.
	if(scritti==NULL){//controllo errore
		perror("errore in thread get malloc");
		exit(EXIT_FAILURE);
	}
	*scritti=0;//inizializzo, deve partire da zero
	//array contenente il comando get e la sua inizializzazione
	unsigned char commandcode[2];
	commandcode[0]=(unsigned char)254;
	commandcode[1]=(unsigned char)253;
	//invio il comando
	ptc_write(*thread_arg->ptrsockfd,commandcode,2*sizeof(char),NULL,1);
	//variabile contenente la lunghezza del nome
	uint32_t lensize=(uint32_t)strlen(*thread_arg->comando);
	if(maxnum(sizeof(uint32_t))<strlen(*thread_arg->comando)){
		perror("overflow thread get1");
		exit(EXIT_FAILURE);
	}

	// variabile che conterra il path del file
	char* nomefile;
	//la directory di default per il download e /download.
	char arraynome[lensize+1+strlen("./download/\0")];
	nomefile=(char*)arraynome;
	memcpy(nomefile,"./download/",strlen("./download/\0"));
	memcpy(nomefile+strlen("./download/\0"),*thread_arg->comando,lensize);
	//mando la lunghezza del nome
	ptc_write(*thread_arg->ptrsockfd,(void*)&lensize,sizeof(uint32_t),NULL,0);
	// mando il nome
	ptc_write(*thread_arg->ptrsockfd,*thread_arg->comando,lensize,NULL,1);
	//variabile e ricezione della lunghezza del file
	uint32_t length=0;
	ptc_read(*thread_arg->ptrsockfd,(char*)&length,sizeof(uint32_t),NULL,0);
	nomefile[strlen(*thread_arg->comando)+strlen("./download/\0")]='\0';
	//Qui controllo se il file esiste, se esiste aggiungo _2 al nome del file
	unsigned char esiste=(unsigned char)0;
	while(file_exists(nomefile)==1){
		char* tofree=nomefile;
	char* nomefileptr=malloc(strlen(nomefile)+3);//LIBERATO
		if(nomefileptr==NULL){
			perror("errore in nomefile ptr maloc thread get");
			exit(EXIT_FAILURE);
		
		}
		memcpy(nomefileptr,nomefile,strlen(nomefile));
                nomefileptr[strlen(nomefile)]='_';
                nomefileptr[strlen(nomefile)+1]='2';
                nomefileptr[strlen(nomefile)+2]='\0';
		nomefile=nomefileptr;
		if(esiste==1){
			free(tofree);
		}	
		esiste=(unsigned char)1;
        }
	//fd del file che conterra il file che sara ricevuto
	int fd = open(nomefile,O_CREAT | O_RDWR,0666); //CHIUSO
	if (fd==-1){
		perror("errore open comandoget");	
		exit(EXIT_FAILURE);	
	}
	if(ftruncate(fd,length)!=0){//allungo il file alla dimensione
		perror("errore in ftruncate");
		exit(EXIT_FAILURE);
	}	
	char* buffer;
	//uso la mmap per comodita... ricevo e scrivo sul file... ma non so se sia efficiente in questo caso! E piu una scelta di prototipazione. Da valutare
	if((buffer=(char*)mmap(NULL,length, PROT_READ | PROT_WRITE,MAP_SHARED,fd,0))==MAP_FAILED){//LIBERATA
		perror("errore in mmap comandoget");
		exit(EXIT_FAILURE);
	}
	off_t lengthptr=0;
	lengthptr=(off_t)length;
	if(maxnum(sizeof(off_t))<length){
		perror("overflow thread get 2");
		exit(EXIT_FAILURE);
	}

	//thread per stampare l avanzamento del trasferimento
	struct thread_arg_t2 thread_stamparg={scritti,&lengthptr,(unsigned char)0,thread_arg->activethreadid,thread_arg->condmutex,thread_arg->threadcond,thread_arg->chisono};
	if (pthread_create(&stamp_threadid, NULL, thread_stamp, &thread_stamparg)) {//JOINATO
		fprintf(stderr,"error creating first thread \n");
		exit(EXIT_FAILURE);
        }
	if(maxnum(sizeof(size_t))<(uintmax_t)length){// controllo overflow nel casting di un off_t come size_t
		fprintf(stderr,"overflow in thread get");
		exit(EXIT_FAILURE);
	}

	//ricevo il file vero e proprio, nel buffer
	ptc_read(*thread_arg->ptrsockfd,buffer,(size_t)length,scritti,1);
	thread_stamparg.finito=(unsigned char)1;
	printf("Success\n");
	//libero le risorse
	if(pthread_join(stamp_threadid,NULL)!=0){
		perror("errore in pthread join thread arg in thread get");
		exit(EXIT_FAILURE);
	
	}
	if(munmap((void*)buffer,(size_t)length)!=0){
		perror("errore in munmap comandoget");
		exit(EXIT_FAILURE);
	}
	if(close(fd)==-1){
		perror("errore in close comandoget");
		exit(EXIT_FAILURE);
	}
	free(scritti);
	if(esiste==(unsigned char)1){
		free(nomefile);
	}
	free(*thread_arg->comando);
	pthread_exit(&exit_success);
}


//Speculare a thread get
void *thread_put(void *a){
	struct thread_arg_t3* thread_arg= (struct thread_arg_t3*)a;
	pthread_t stamp_threadid;
	size_t* scritti=malloc(sizeof(size_t));//LIBERATO
	if(scritti==NULL){
		perror("errore in thread get malloc");
		exit(EXIT_FAILURE);
	
	}
	*scritti=0;
	struct stat sb;
	int fd = open(*thread_arg->comando, 400 );//CHIUSO
	if(fd==-1){
		perror("errore in open compandoput");
		exit(EXIT_FAILURE);
	}
	unsigned char commandcode[2];
	commandcode[0]=(unsigned char)254;
	commandcode[1]=(unsigned char)254;
	uint32_t strsize=strlen(*thread_arg->comando);
	ptc_write(*thread_arg->ptrsockfd,commandcode,2*sizeof(char),NULL,1);
	ptc_write(*thread_arg->ptrsockfd,&strsize,sizeof(uint32_t),NULL,0);
	ptc_write(*thread_arg->ptrsockfd,*thread_arg->comando,strlen(*thread_arg->comando),NULL,1);
	if(fstat(fd,&sb)==-1){
		perror("errore in fstat comandoput");
		exit(EXIT_FAILURE);
	}
	uint32_t sbstsize=(uint32_t)sb.st_size;
	if(maxnum(sizeof(uint32_t))<(size_t)sb.st_size || sb.st_size<0){
		perror("overflow thread put");
		exit(EXIT_FAILURE);
	}
	ptc_write(*thread_arg->ptrsockfd,(char*)&sbstsize,sizeof(uint32_t),NULL,0);
	char* buffer;
	if((buffer=(char*)mmap(NULL,sb.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED){//LIBERATO
		perror("errore in mmap comandoput");
		exit(EXIT_FAILURE);
	}
	struct thread_arg_t2 thread_stamparg={scritti,&sb.st_size,(unsigned char)0,thread_arg->activethreadid,thread_arg->condmutex,thread_arg->threadcond,thread_arg->chisono};
	if (pthread_create(&stamp_threadid, NULL, thread_stamp, &thread_stamparg)) { //JOINATO
		perror("error creating first thread");
		exit(EXIT_FAILURE);
        }
	ptc_write(*thread_arg->ptrsockfd,buffer,sb.st_size,scritti,1);
	thread_stamparg.finito=(unsigned char)1;
	printf("Success");
	if(pthread_join(stamp_threadid,NULL)!=0){
		perror("errore in pthread join thread arg in thread get");
		exit(EXIT_FAILURE);
	
	}
	if(maxnum(sizeof(size_t))<(uintmax_t)sb.st_size){// controllo overflow nel casting di un off_t come size_t
		fprintf(stderr,"overflow in thread get");
		exit(EXIT_FAILURE);
	}
	if(munmap(buffer,(size_t)sb.st_size)!=0){
		perror("errore in munmap comandoget");
		exit(EXIT_FAILURE);
	}
	if(close(fd)==-1){
		perror("errore in close comandoget");
		exit(EXIT_FAILURE);
	}
	free(scritti);
	free(*thread_arg->comando);
	pthread_exit(&exit_success);


}

void *thread_client(void *a){
	pthread_t* workingthread=NULL;//puntatore a un thread get o thread put;
	struct thread_arg_t3 thread_arg3; //la struct che e condivisa tra tutti i thread client
	int sockfd=0;
	struct thread_arg_t* thread_arg = (struct thread_arg_t*)a;
	size_t* chisono = malloc(sizeof(size_t));//mi serve per tutto il tempo di esecuzione quindi non la libero
 	if(chisono==NULL){
		perror("errore malloc chisono");
		exit(EXIT_FAILURE);
	}
	*chisono=*thread_arg->threadidarrayptr;
	printf("Thread id %lu creato \n", (long unsigned)thread_arg->threadidarray[*chisono]);

	for(;;){
		//buffer che contiene i comandi ricevuti, (verra diviso in stringhe tramite carattere speciale \0 e i puntatori inseriti nell arra)
		char* buffer=malloc(MAXLINE*sizeof(char));
		char* comandi[3];
		for(;;){
			int i=0;
			for(i=0;i<4;i++){
				comandi[i]=NULL;
			}
			//se non tocca a me, vado a dormire
			while((*thread_arg->activethreadid!=*chisono)&&(*thread_arg->exit==(char)0)){
				printf("Thread id %lu va a dormire \n", (long unsigned)thread_arg->threadidarray[*chisono]);
				if(pthread_mutex_lock(thread_arg->condmutex)!=0){
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
		
				if(pthread_cond_wait(thread_arg->threadcond,thread_arg->condmutex)!=0){
					perror("errore in menu pthread cond broadcast");
					exit(EXIT_FAILURE);
	
				}
				if(pthread_mutex_unlock(thread_arg->condmutex)!=0){
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
		
			}
			//se un threda client ha ricevuto il comando di uscita
			if(*thread_arg->exit!=(char)0){
				printf("Thread id %lu distrutto\n", (long unsigned)thread_arg->threadidarray[*chisono]);
				pthread_exit(&exit_success);
			}
			//primo menu nel quale si puo connettersi e creare nuovi thread client
			printf("commands: connect <IP> - mkthread - chthread <threadid> - exit\n");
			getcommand(comandi,buffer);//funzione che si occupa di ricevere il comando e riempire il buffer
			if(comandi[0]!=NULL && (strcmp(comandi[0],"connect")==0)){
				if(comandi[1]!=NULL && strlen(comandi[1])>0){
					printf("ricevuto comando di connessione a %s \n",comandi[1]);
					if(connessione(comandi[1],&sockfd)==EXIT_SUCCESS)//mi connetto
						break;
					else{
						if(close(sockfd)!=0){
							perror("errore in shutdown comando connetti");
							exit(EXIT_FAILURE);
						}
						continue;
					}
				}
			}
			if(comandi[0]!=NULL && (strcmp(comandi[0],"exit")==0)){
				if(comandi[1]==NULL && comandi[2]==NULL){
					printf("ricevuto comando di exit\n");
					comandoexit(thread_arg,chisono);
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && (strcmp(comandi[0],"mkthread")==0)){
				if(comandi[1]==NULL && comandi[2]==NULL){
					printf("ricevuto comando mkthread\n");
					comandomkthread(thread_arg);
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && (strcmp(comandi[0],"chthread")==0)){
				if(comandi[1]!=NULL && strlen(comandi[1])>0 && comandi[2]==NULL){
					comandochthread(comandi[1],thread_arg->activethreadid,thread_arg->threadcond,thread_arg->threadidarrayptr);
					
				}
				else
					continue;
			}
	
				
		}
		for(;;){
			int i=0;
			//rinizializzo l array
			for(i=0;i<3;i++){
				comandi[i]=NULL;
			}
			//come sopra, se non tocca a me vado a dormire e se un thread riceve il comando di uscita, esco.
			while((*thread_arg->activethreadid!=*chisono)||(*thread_arg->exit!=(char)0)){
				// sleep finche' non ritocca a me.
				if(pthread_mutex_lock(thread_arg->condmutex)!=0){
					comandodisconnettierr(sockfd);
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}
		
				if(pthread_cond_wait(thread_arg->threadcond,thread_arg->condmutex)!=0){
					comandodisconnettierr(sockfd);
					perror("errore in menu pthread cond broadcast");
					exit(EXIT_FAILURE);
	
				}
				if(pthread_mutex_unlock(thread_arg->condmutex)!=0){
					comandodisconnettierr(sockfd);
					perror("errore in menu pthread mutex lock");
					exit(EXIT_FAILURE);
				}

			}
			if(*thread_arg->exit!=(char)0){
				exit(EXIT_FAILURE);
			}
			//menu post connessione
			printf("commands: ls - put <filename> - get <filename> - cd <folder> - mkthread - chthread <threadid> - disconnect - exit\n");
			getcommand(comandi,buffer);//ricevo il comando
			if(comandi[0]!=NULL && (strcmp(comandi[0],"ls")==0)){
				if(comandi[1]==NULL && comandi[2]==NULL){
					printf("ricevuto comando ls\n");
					if(workingthread!=NULL){
						printf("Attendere completamento trasmissione\n");
						pthread_join(*workingthread,NULL);
						free(workingthread);
						workingthread=NULL;
					}
					comandols(sockfd);
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"put")==0){
				//speculare al get (appena sotto)
				if(comandi[1]!=NULL && strlen(comandi[1])>0 && comandi[2]==NULL){
					if(workingthread!=NULL){
						printf("Attendere completamento trasmissione\n");
						pthread_join(*workingthread,NULL);
						free(workingthread);
						workingthread=NULL;
					}
					if((workingthread=malloc(sizeof(pthread_t)))==NULL){//LIBERATOi
						comandodisconnettierr(sockfd);
						perror("errore in malloc workingthread");
						exit(EXIT_FAILURE);
					}
					char* comando = malloc(strlen(comandi[1]+1)*sizeof(char));//LIBERATO
					if(comando==NULL){
						comandodisconnettierr(sockfd);
						perror("errore in malloc menu comando put");	
						exit(EXIT_FAILURE);
					}
					memcpy(comando,comandi[1],strlen(comandi[1])+1);
					thread_arg3.comando=&comando;
					thread_arg3.ptrsockfd=&sockfd;
					thread_arg3.activethreadid=thread_arg->activethreadid;
					thread_arg3.condmutex=thread_arg->condmutex;
					thread_arg3.threadcond=thread_arg->threadcond;
					thread_arg3.chisono=chisono;
					comandoput(workingthread,(struct thread_arg_t3*)&thread_arg3);	
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"get")==0){
				if(comandi[1]!=NULL && strlen(comandi[1])>0 && comandi[2]==NULL){
					//se e diverso da nulla significa che c e ancora un trasferimento in corso...!
					if(workingthread!=NULL){
						printf("Attendere completamento trasmissione\n");
						pthread_join(*workingthread,NULL);
						free(workingthread);
						workingthread=NULL;
					}
					if((workingthread=malloc(sizeof(pthread_t)))==NULL){//LIBERATO
						comandodisconnettierr(sockfd);
						perror("errore in malloc workingthread");//controllo errore
						exit(EXIT_FAILURE);
					}
					char* comando = malloc(strlen(comandi[1]+1)*sizeof(char));//LIBERATO
					if(comando==NULL){
						comandodisconnettierr(sockfd);
						perror("errore in malloc menu comando put");	
						exit(EXIT_FAILURE);
					}
					//copio il nome del dato e lo metto nella struct, chiamo la funzione per gestire il get
					memcpy(comando,comandi[1],strlen(comandi[1])+1);
					thread_arg3.comando=&comando;
					thread_arg3.ptrsockfd=&sockfd;
					thread_arg3.activethreadid=thread_arg->activethreadid;
					thread_arg3.condmutex=thread_arg->condmutex;
					thread_arg3.threadcond=thread_arg->threadcond;
					thread_arg3.chisono=chisono;
					comandoget(workingthread,&thread_arg3);
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"cd")==0){
				if(comandi[1]!=NULL && strlen(comandi[1])>0 && comandi[2]==NULL){
					if(VERBOSE==1){
						printf("comand cd ricevuto");
					}
					comandocd(comandi[1],sockfd);
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"disconnect")==0){
				if(comandi[1]==NULL && comandi[2]==NULL){
					if(workingthread!=NULL){
						printf("Attendere completamento trasmissione\n");
						pthread_join(*workingthread,NULL);
					}	
					comandodisconnetti(sockfd);
					break;
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"exit")==0){
				if(comandi[1]==NULL && comandi[2]==NULL){
					if(workingthread!=NULL){
						printf("Attendere completamento trasmissione\n");
						pthread_join(*workingthread,NULL);
					}
					comandodisconnetti(sockfd);
					comandoexit(thread_arg,chisono);
				}
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"mkthread")==0){
				if(comandi[1]==NULL && comandi[2]==NULL)
					comandomkthread(thread_arg);
				else
					continue;
			}
			if(comandi[0]!=NULL && strcmp(comandi[0],"chthread")==0){
				if(comandi[1]!=NULL && strlen(comandi[1])>0 && comandi[2]==NULL){
					comandochthread(comandi[1],thread_arg->activethreadid,thread_arg->threadcond,thread_arg->threadidarrayptr);
				}
				else
					continue;
			}
		}
		
	}
}

//creo il nuovo thread e chiamo la funzione per cambiare il thread che ha il "comando"
void comandomkthread(struct thread_arg_t* thread_arg){
	(*(thread_arg->threadidarrayptr))++;
	comandochthread2(thread_arg->threadidarrayptr,thread_arg->activethreadid,thread_arg->threadcond);
	if (pthread_create(&thread_arg->threadidarray[*thread_arg->threadidarrayptr], NULL, thread_client, thread_arg)) {
                fprintf(stderr,"error creating thread, number %zu \n",*(thread_arg->threadidarrayptr));
                exit(EXIT_FAILURE);

	}

}
//due funzioni leggermente differenti per cambiare il thread che ha il comando(vengono chiamate in ambiti differenti)
void comandochthread2(size_t* numero,size_t* ptr,pthread_cond_t* cond){
	*ptr=*numero;
	if(errno!=0){
		perror("errore in strtol comandochtrhead");
		exit(EXIT_FAILURE);
	}
	
	if(pthread_cond_broadcast(cond)!=0){
			perror("errore in comandochthread pthread broadcast");
			exit(EXIT_FAILURE);
	}

}

void comandochthread(char* numero,size_t* ptr, pthread_cond_t* cond,size_t* max){
	errno=0;
	long string=(int)strtol(numero,NULL,0);
	if((unsigned long)string>pow(2,sizeof(size_t))-2){ //controllo un po' rozzo
		fprintf(stderr,"overflow in comandochthread");
		exit(EXIT_FAILURE);
	}
	if((size_t)string>*max){
		fprintf(stderr,"non sai contare?");
		return;
	}
	*ptr=(size_t)string;
	if(errno!=0){
		perror("errore in strtol comandochtrhead");
		exit(EXIT_FAILURE);
	}	
	if(pthread_cond_broadcast(cond)!=0){
			perror("errore in comandochthread pthread broadcast");
			exit(EXIT_FAILURE);
	}

}
//funzione per il cambio directory (del server)
void comandocd(char* folder,int sockfd){
	if(VERBOSE==1){
		printf("entro in comandocd\n");
	}
	unsigned char commandcode[2];
	commandcode[0]=(unsigned char)254;
	commandcode[1]=(unsigned char)252;
	//mando il codice comando
	ptc_write(sockfd,commandcode,2*sizeof(char),NULL,1);
	uint32_t strsize=strlen(folder);
	if(maxnum(sizeof(uint32_t))<strlen(folder)){
		perror("overflow comando cd");
		exit(EXIT_FAILURE);
	}
	//mando la dimensione del nome
	ptc_write(sockfd,(void*)&strsize,sizeof(uint32_t),NULL,0);
	//mando il nome
	ptc_write(sockfd,folder,strlen(folder),NULL,1);
}

//funzione per il get (delega a thread)
void comandoget(pthread_t* threadid, struct thread_arg_t3* thread_arg){
	if (pthread_create(threadid, NULL, thread_get, thread_arg)) {
		comandodisconnettierr(*thread_arg->ptrsockfd);
		fprintf(stderr,"error creating first thread \n");
		exit(EXIT_FAILURE);
        }
}
//mando il codice comando per ls e poi chiamo la funzione per stampare il ritorno.
void comandols(int sockfd){
	if(VERBOSE==1){
		printf("entro in stamparitorno\n");
	}
	unsigned char commandcode[2];
	commandcode[0]=(unsigned char)254;
	commandcode[1]=(unsigned char)255;
	ptc_write(sockfd,commandcode,2*sizeof(unsigned char),NULL,1);
	stamparitorno(sockfd);
}
// comando per il put (delega a thread)
void comandoput(pthread_t*  workingthread,struct thread_arg_t3* thread_arg){
	if (pthread_create(workingthread, NULL, thread_put, thread_arg)) {
		comandodisconnettierr(*thread_arg->ptrsockfd);
		fprintf(stderr,"error creating first thread \n");
		exit(EXIT_FAILURE);
        }

}

//mando il comando di disconnessione e chiudo il socket
void comandodisconnetti(int sockfd){
	unsigned char commandcode[2];
	commandcode[0]=(unsigned char)254;
	commandcode[1]=(unsigned char)251;
	ptc_write(sockfd,commandcode,2*sizeof(char),NULL,1);

	if(shutdown(sockfd,0)!=0){
		perror("errore in shutdown comando disconnetti");
		exit(EXIT_FAILURE);
	
	}
}
//propago l errore al server
void comandodisconnettierr(int sockfd){
	unsigned char commandcode[2];
	commandcode[0]=(unsigned char)254;
	commandcode[1]=(unsigned char)250;
	noisy_writen(sockfd,commandcode,2*sizeof(char));

	if(shutdown(sockfd,0)!=0){
		perror("errore in shutdown comando disconnetti");
		exit(EXIT_FAILURE);
	
	}
}
//propago la chiusura a tutti i thread (la disconnessione e gia stata chiamata nel menu)
void comandoexit(void *a,size_t* chisonoptr){
	struct thread_arg_t* thread_arg=(struct thread_arg_t*)a;
	*thread_arg->exit=(char)1;
	if(pthread_cond_broadcast(thread_arg->threadcond)){
		perror("errore in cond broadcast comandoexit");
		exit(EXIT_FAILURE);
	}
	if(thread_arg->exit==NULL){
		perror("errore in thread_arg comandoexit");
		exit(EXIT_FAILURE);
	}
	if(chisonoptr==NULL){
		perror("errore in chisonoptr comandoexit");
		exit(EXIT_FAILURE);
	}
	printf("Thread id %lu distrutto\n", (long unsigned)thread_arg->threadidarray[*chisonoptr]);
	exit(EXIT_FAILURE);
}
// stampa il ritorno del ls
void stamparitorno(int sockfd){
	if(VERBOSE==1){
		printf("entro in stamparitorno\n");
	}
	uint32_t grandezza;
	//ricevo grandezza del ls 
	ptc_read(sockfd,(void*)&grandezza,sizeof(uint32_t),NULL,0);
/*	
	//stampo
	size_t z;
	for(z=0;z<sizeof(size_t);++z){
		printf("%u-\n",(unsigned int)*((unsigned char*)&grandezza+z));
	
	printf("\n");
*/
	unsigned char* buffer=malloc(grandezza);//LIBERATO
	if(buffer==NULL){
		comandodisconnettierr(sockfd);
		perror("errore in malloc stamparitorno");
		exit(EXIT_FAILURE);
	}
	memset(buffer,' ',grandezza*sizeof(unsigned char));
	//ricevo l ls
	ptc_read(sockfd,(void*)buffer,grandezza,NULL,1);
	size_t i;
	//stampo ls
	for(i=0;i<grandezza;++i){
		printf("%c",(char)*(char*)(buffer+i));
	}
	printf("\n");
	free(buffer);//libero il buffer
}


int connessione(char* stringa,int* sockptr){
	
	if(sockptr==NULL){
		perror("errore in sockfd");
		exit(EXIT_FAILURE); 
	}
	//creo il socket, 
  struct sockaddr_in	servaddr;
  if ((*sockptr = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("errore in socket");
    exit(1);
  }
//converto l indirizzo ip in binario
  memset((void *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SERV_PORT);
  if (inet_pton(AF_INET,stringa , &servaddr.sin_addr) <= 0) {
    fprintf(stderr, "errore in inet_pton per %s", stringa);
    exit(1);
  }    
// effettuo la connessione (non c e una connessione in UDP, semplicemente non devo riscrivere indirizzo e porta per scrivere o leggere)
  if (connect(*sockptr, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    perror("errore in connect");
    exit(1);
  }    
 return connection_request(*sockptr);
}
//funzione che legge da stdin e prepara l array comandi[]
void getcommand(char** comando, char* buffer){
	ssize_t buffersize=(ssize_t)MAXLINE;
	comando[0]=buffer;
	int i=0,j=0;
	int errorcheck;
	while((errorcheck=fgetc(stdin))!=(int)EOF){//leggo fino alla fine
		comando[i][j]=errorcheck;
		if(comando[i][j]==' '){// quanto trovo uno spazio lo sostituisco con il fine stringa e imposto quella stringa come primo elemento dell array (array comandi[] che e nel menu sopra)
			comando[i][j]='\0';
			if((i++)>3){
				perror("troppi comandi in ingresso, max 3");
				exit(EXIT_FAILURE);
			}
				
			j=0;
			if((buffersize--)<0){
				perror("errore in malloc getcommand buffer finito");
				exit(EXIT_FAILURE);
			}
			comando[i]=&buffer[MAXLINE-buffersize];
	
		}
		else if(comando[i][j]=='\n'){
			comando[i][j]='\0';
			return;
		}
		else{		
			j++;
			if((buffersize--)<0){
				perror("errore in malloc getcommand buffer finito 2");
				exit(EXIT_FAILURE);
			}
		}
	}
	perror("errore in getcommando");//di norma non si dovrebbe arrivare a 	questo punto...
	pthread_exit(&exit_failure);
}
