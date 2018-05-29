/*
/   First byte 11111111 -> connection request.
/   First byte 11111110 -> command information.
	ls: 11111110 11111111
/   Other first byte conf -> packet number(-> file transfert status) -> max window 0-252
*/

#include "basic.h"
#include "protocollo.h"
#include "noise.h"
#include <sys/time.h>
#include "utils.h"

//funzione che usa il client per richiedere  la connessione
int connection_request(int sockd){
	//mando il codice comando per la connessione
	unsigned char  sendline[1];
	sendline[0]=(unsigned char)255;
	if ((writen(sockd, sendline, 1)) < 0) {
		fprintf(stderr, "errore in write");
		exit(EXIT_FAILURE);
	}
	//attendo la risposta... two way handshake
	int letti=0;
	sendline[0]=0;
	fd_set set;
	struct timeval timeout,time;
	int rv;
	FD_ZERO(&set); /* clear the set */
	FD_SET(sockd, &set); /* add our file descriptor to the set */
	timeout.tv_sec = TIME_OUT;
	timeout.tv_usec = 0;
	if(gettimeofday(&time,NULL)!=0){
		perror("errore in gettimeofday connection request");
		exit(EXIT_FAILURE);
	}
	while(sendline[0]!=(unsigned char)255){
		printf("attendo risposta del server\n");
		rv = select(sockd + 1, &set, NULL, NULL, &timeout);//attendo con TIMEOUT!
		if(rv == -1){
			perror("errore in select"); /* an error accured */
			exit(EXIT_FAILURE);
		}
		else if(rv == 0){
			perror("Il server non ha risposto entro il TIMEOUT");//timeout scaduto
			return EXIT_FAILURE;
		}
		struct timeval timenow;
		if(gettimeofday(&timenow,NULL)!=0){
			perror("errore in gettimeofday connection request");
			exit(EXIT_FAILURE);
		}
	        	if ((timenow.tv_sec - time.tv_sec) >TIME_OUT) {
				perror("Il server non ha risposto entro il TIMEOUT");//timeout scaduto (il server ha risposto con qualcosa che non era il codice di risposta evidentemente e poi il tempo totale ha superato il timeout)
				return EXIT_FAILURE;
			}
		letti=readn(sockd, sendline, 1);
   		if (letti< 0) {
       			 perror("errore in write");
    		   	 exit(EXIT_FAILURE);
    		}

	}
	printf("risposta ottenuta\n");
	return EXIT_SUCCESS;
}

//quasta funzione e utilizzata dal server per attendere una connessione
int accept_connections(int sockfd, void *buff, size_t len, int flags,  struct sockaddr *from, socklen_t * fromlen)
{
	flags=flags;//no warning (ho voluto mantenere la firma)
	while (1) {
		if ((recvfrom(sockfd, buff, len, 0, from, fromlen)) < 0) {//attendo di ricevere
			perror("errore in recvfrom");
			exit(EXIT_FAILURE);
		}
		//se ho ricevuto il codice di connessione allora rispondo per fare il two way handshake
		if (*((unsigned char *)buff) == (unsigned char)255) {
			if (connect(sockfd, from, *fromlen) < 0) {
				perror("errore in connection");
				exit(EXIT_FAILURE);
			}
			int bytes;
			while((bytes = writen(sockfd,buff,sizeof(unsigned char)))<=0 ){
				if(bytes<0){
					perror("errore in writen");
					exit(EXIT_FAILURE);
				}
			}
			return 0;
		}
	}
}

//funzione per scrivere
ssize_t ptc_write(int fd, const void *original_buffer, size_t n,size_t* scritti,unsigned char force_endian)//l ultimo argomento e un puntatore a un size_t, la funzione scrivera li i byte scritti durante l esecuzione della funzione. Serve a far sapere all utente a che punto siamo arrivati con la trasmissione.
{	
	//se e big endian riscrivo al contario il buffer- perche ho scelto come standard network little endian-
	unsigned char* buf=NULL;
	if(isitbigendian()==1 && force_endian==0){
		buf=malloc(n);
		if(buf==NULL){
			perror("errore in malloc ptc write");
			exit(EXIT_FAILURE);
		}
		memcpy(buf,original_buffer,n);
		change_endian(buf,n);
	}
	else
		buf=(unsigned char*)original_buffer;
	size_t* scritto;
	unsigned char tofree=0;//sapere se devo liberare la memoria di scritti (cioe se l ho allocato io allora devo liberarlo)
	srand(time(NULL));//nuova sequenza pseudo random
	fd_set set;
	struct timeval timeout;
	list lista;
	unsigned char	resendlimiter[WINDOW];
	memset(resendlimiter,(unsigned char)5,WINDOW);
	struct timeval times[WINDOW];
	unsigned char ptrarr[WINDOW];
	unsigned char ack[WINDOW]; memset(ack,0,sizeof(unsigned char)*WINDOW);
	int ptr = 0, dim = 0, bytes = 0, letti =
	    0, ptrletto = 0, ptrinf = 0, ptrsup = WINLEN-1, finestrafinita =
	    0;// ptr e l attuale pacchetto della finestra, ptrinf e ptrsup definiscono gli estremi della finestra.
	int byte_scritti[WINDOW], byte_inf[WINDOW];//tengo ricordo di ogni pacchetto della finestra in modo che se la devo rispedire so gia cosa devo mandare (avevo considerato casi in cui i pacchetti potevano avere dimensioni differenti... in realta avendo tutti (tranne l ultimo dimensine uguale avrei potuto fare un sistema per trovare i byte da spedire senza sprecare questa memoria). byte scritti e la dimensione, byte inf e la base.
	unsigned char buffer[PACKETSIZE];//memoria dove preparo il pacchetto per spedirlo
	struct timeval nowtime;//memorizzo l orologio di sistema per confrontarlo con l orologio relativo al momento di spedizione del pacchetto
	node* arr;//array dei nodi della lista
	//ogni volta che invio un pacchetto lo inserisco nella lista con il tempo
	//mantengo informazioni si tutti i pacchetti tramite questo array, e una lista atipica.
	//perche quando ricevo un pacchetto, vorrei rimuoverlo dalla lista senza effettuare una ricerca.
	//supposto che la finestra non sia gigante, non dovrebbe esserci un overhead di memoria troppo grande.
	if((arr=(node*)malloc(WINDOW*sizeof(node)))==NULL){//LIBERATO
		perror("errore in malloc node");
		exit(EXIT_FAILURE);
		
	}
	memset(arr,0,WINDOW*sizeof(node));
	list_init(&lista,arr, WINDOW);
	intmax_t time_out=TIME_OUT*1000000;//timeout in microsecondi
	if(VERBOSE==1){
		printf("time out iniziale s:%ju,ms:%ju \n",time_out/1000000,time_out/1000);
	}
	//se il chiamante non ha necessita di sapere l andamento del trasferimento e quindi non ha fornito un puntatore a un size_t (o meglio ha fornito il puntatore a NULL) allora devo creare qui lo spazio.
	if(scritti==NULL){
			tofree=1;
			scritto=malloc(sizeof(size_t));
			if(scritto==NULL){
				perror("errore in ptc write");
				exit(EXIT_FAILURE);
			}
	}
	else
		scritto=scritti;
	
	*scritto=0;
	for (;;) {
		if ((finestrafinita == 0)&&(n-*scritto>0)) {//finche la finestra non e ci sono ancora byte da scrivere
			buffer[0] = (unsigned char)ptr;
			if (n - *scritto >= PACKETSIZE) {
				byte_inf[ptr] = *scritto;
				byte_scritti[ptr] = PACKETSIZE - 1;
				dim = PACKETSIZE-1;
			// ultimo paccheeto sara probabilmente piu piccolo della dimensione massima
			} else if (n - *scritto < PACKETSIZE) {
				byte_inf[ptr] = *scritto;
				dim = n - *scritto;
				byte_scritti[ptr] = n - *scritto;
			}
			//preparo il pacchetto, il primo byte e il numero di pacchetto
			memcpy(buffer + 1, buf + *scritto, dim);
			for (;;) {
				//invio il pacchetto
				if ((bytes = noisy_writen(fd, buffer, dim+1)) < 0) {
					perror("errore in writen");
						exit(1);
					}	else if (bytes == dim+1)
							break;
				}
			//aggiorno il contatore dei byte scritti
			*scritto = *scritto + dim;
			if(gettimeofday(&times[ptr],NULL)!=0){
				perror("errore in gettimeofday ptc write");
				exit(EXIT_FAILURE);
			}
			ptrarr[ptr]=(unsigned char)ptr;
			//inserisco nella lista (con il tempo e il numero di paccchetto corrispondente)
			list_put(&lista,(void*) &times[ptr], &ptrarr[ptr], ptr);
			if (ptr == ptrsup)
				finestrafinita = 1;
			else {
				ptrinc(&ptr);
			}
		}
		int rv;
	
		FD_ZERO(&set); /* clear the set */
		FD_SET(fd, &set); /* add our file descriptor to the set */
		timeout.tv_sec = 0;
		timeout.tv_usec = 500;

		for(;;){
			//attendo timeout 500usec (altrimenti andava il loop)
			rv = select(fd + 1, &set, NULL, NULL, &timeout);
			if(rv == -1){
				perror("errore in select"); /* an error accured */
				exit(EXIT_FAILURE);
			}
			else if(rv == 0){
				break;
			}
			//se e arrivato qualcosa leggo
			letti=noisy_readn(fd, buffer, PACKETSIZE);
   			if (letti< 0) {
       				 perror("errore in read ptc write");
    			   	 exit(EXIT_FAILURE);
    			}
			ptrletto = (unsigned char)buffer[0];
			if(ptrletto==(unsigned char)254){
				//questo e il codice che indica che il client ha il buffer pieno, chiudo
				if(buffer[1]==(unsigned char)250){
					unsigned char  sendline[2];
					sendline[0]=(unsigned char)254;
					sendline[1]=(unsigned char)250;
					if ((noisy_writen(fd, sendline, 2)) < 0) {
							fprintf(stderr, "errore in write");
					exit(EXIT_FAILURE);
					}

					//libero le risorse e ritorno
					free(arr);
					size_t ritorno=*scritto;
					if(tofree==1){
						free(scritto);
					}
					if(maxnum(sizeof(size_t))<ritorno){
						fprintf(stderr,"errore overflow ptc write");
						exit(EXIT_FAILURE);
					}
					return	ritorno;
				}
				else
					perror("errore in ptc read pacchetto fuori finestra");
					exit(EXIT_FAILURE);
			}
			//controllo se e' fuori finestra
			if(checklist(&lista,ptrletto)==0){
				continue;
			}
			RECV:
			// se e arrivato il pacchetto corrispondente all ultimo pacchetto della finestra allora significa che posso avanzare la finestra
			if (ptrletto == ptrinf) {
				unsigned char i=ptrletto;
				//sicuramente avanzo una volta e continuo finche l ultimo pacchetto della finestra e gia ackato
				do {
					if(VERBOSE==1){
						printf("avanzo la finestra\n");
					}
					ptrinc(&ptrinf);
					ptrinc(&ptrsup);
					ack[i]=0;
					i+=(unsigned char)1;
					i%=WINDOW;
					if(finestrafinita==1){
						ptrinc(&ptr);
						finestrafinita =0;
					}
				}while( (i!=ptrsup) && (ack[i]==1));
			}
			else
				ack[ptrletto]=(unsigned char)1; //se il pacchetto arrivato non e l ultimo della finestra allora segno che e stato ackato e non avanzo la finestra
			if(gettimeofday(&nowtime,NULL)!=0){
				perror("errore in gettimeofday connection request");
				exit(EXIT_FAILURE);
			}
			//rimuovo il pacchetto ackato dalla lista
			if(resendlimiter[ptrletto]!=0){//se è uguale significa che sono entrato con il jump e quindi il pacchetto  non c'e' nella lista!
				if(list_empty(&lista)==0){
					if(nowtime.tv_sec>times[ptrletto].tv_sec){
						time_out=(time_out/4)*3 +(((nowtime.tv_sec- times[ptrletto].tv_sec)/4)*1000000) + (nowtime.tv_usec - times[ptrletto].tv_usec)/4;
				}//Va spesso in overflow, comunque non e' necessario valutare il timeout fino al millisecondo. Ho commetato questa parte.
				/*
					else if((nowtime.tv_sec==times[ptrletto].tv_sec)&&(nowtime.tv_usec>times[ptrletto].tv_usec)){	
						time_out=(time_out/4)*3 +(nowtime.tv_usec - times[ptrletto].tv_usec)/4;
					}*/
				}
				list_remove(&lista, ptrletto);
			}
			else{
				time_out=TIME_OUT*1000000; //il client si e disconnesso o disincronizzato meglio  meglio abbassare drasticamente il timeout
			}
			resendlimiter[ptrletto]=5;
			if(VERBOSE==1){
				printf("timeout sec:%ju,msec:%ju\n",time_out/1000000,time_out/1000);
                        	printf("finestra finita:%u,ptr:%u,ptrinf:%u,ptrsup:%u\n",(unsigned int)finestrafinita,ptr,ptrinf,ptrsup);
                	}
			//se la lista e vuota e ho scritto tutto allora posso uscire
			if((n-*scritto<=0)&&(list_empty(&lista)==1)){
				if(VERBOSE==1){
					printf("fine scrittura\n");
				}
				//two way handshake per fine trasmissione
				unsigned char  sendline[2];
				int letti=0;
				memset(sendline,0,2*sizeof(unsigned char));
				fd_set set;
				struct timeval timeout,time;
				int rv;
				FD_ZERO(&set); /* clear the set */
				FD_SET(fd, &set); /* add our file descriptor to the set */
				if(gettimeofday(&time,NULL)!=0){
					perror("errore in gettimeofday connection request");
					exit(EXIT_FAILURE);
				}
				while(sendline[0]!=(unsigned char)254 && sendline[1]!=(unsigned char)250){
					timeout.tv_sec = TIME_OUT;
					timeout.tv_usec = 0;
					printf("attendo risposta del server\n");
					rv = select(fd + 1, &set, NULL, NULL, &timeout);
					if(rv == -1){
						perror("errore in select"); /* an error accured */
						exit(EXIT_FAILURE);
					}
					else if(rv == 0){
						perror("Il server non ha risposto entro il TIMEOUT");
						return EXIT_FAILURE;
					}
					struct timeval timenow;
					if(gettimeofday(&timenow,NULL)!=0){
						perror("errore in gettimeofday connection request");
						exit(EXIT_FAILURE);
					}
				       		 if ((timenow.tv_sec - time.tv_sec) >TIME_OUT) {
							perror("Il server non ha risposto entro il TIMEOUT2");
							return EXIT_FAILURE;
						}
					letti=noisy_readn(fd, sendline, 2);
   					if (letti< 0) {
       						 perror("errore in write");
    					   	 exit(EXIT_FAILURE);
    					}
			
			}
			sendline[0]=(unsigned char)254;
			sendline[1]=(unsigned char)250;
			if ((noisy_writen(fd, sendline, 2)) < 0) {
				fprintf(stderr, "errore in write");
				exit(EXIT_FAILURE);
			}
			size_t ritorno=*scritto;
			if(tofree==1){
				free(scritto);
			}
			if(maxnum(sizeof(size_t))<ritorno){
				fprintf(stderr,"errore overflow ptc write");
				exit(EXIT_FAILURE);
			}
			free(arr);
			return *scritto;
			}
		}

	//finche c e un pacchetto scaduto in lista lo tolgo dalla lista, lo rispedisco e lo reinserisco con orologio aggiornato	
		if ((list_scaduto(&lista,&time_out) == 0)) {
			unsigned char* ptrerrorcheck = (unsigned char*)list_popvalue2ptr(&lista);
			if(ptrerrorcheck==NULL){
				perror("errore ptrcheckerror list scaduti");
				exit(EXIT_FAILURE);
			}
			ptrletto=*ptrerrorcheck;
			//se l'ho rimandato più di n (5) volte e non ricevo ack, può darsi che il client sia più avanti...(può accadere con la SR) quindi provo ad andare comunque avanti
			--resendlimiter[ptrletto];
			if(resendlimiter[ptrletto]==0)
				goto RECV;
			buffer[0] = (unsigned char)ptrletto;
			memcpy(buffer + 1, buf + byte_inf[ptrletto], byte_scritti[ptrletto]);
			for (;;) {
				if ((bytes = noisy_writen(fd, buffer, byte_scritti[ptrletto]+1)) < 0) {
					perror("errore in writen");
					exit(1);
				} else if (bytes == byte_scritti[ptrletto]+1)
					break;
			}
			if(gettimeofday(&times[ptrletto],NULL)!=0){
				perror("errore in gettimeofday connection request");
				exit(EXIT_FAILURE);
			}
			time_out=time_out/2 + TIME_OUT/2;// alzo il timeout perche sto rimandando lo stesso pacchetto...
			list_put(&lista,(void*) &times[ptrletto],(void*)&ptrarr[ptrletto] ,ptrletto);
		}
	}
}

//funzione per la ricezione
ssize_t ptc_read(int fd, void *buf, size_t n, size_t* scritti,unsigned char force_endian){
	srand(time(NULL));
    	unsigned char* pacchettiptr[WINDOW];//questo array di puntatori diventera una matrice dove pero ho la liberta di scambiare le righe senza dover copiare ma semplicemente scambiando gli indirizzi in questo array.
	unsigned char* pacchetti;//puntato all area di memoria che effettivamente conterra la matrice dei pacchettiXfinestra
	unsigned char startwindow=0, endwindow=WINLEN-1;
	unsigned char alreadyreceived[WINDOW];//ricordare se e gia stato ricevuto e ackato ma ancora non e stato inserito nel buffer. Inserisco nel buffer solo quando la finestra avanza
	unsigned char numberreceivedpacket=0;
	memset((void*)alreadyreceived,0,sizeof(unsigned char[WINDOW]));
	size_t bytes[WINDOW];
	ssize_t letti=0;
	size_t* bytescrittisubuffer;
	if((pacchetti=(unsigned char*)malloc(WINDOW*PACKETSIZE*sizeof(unsigned char)))==NULL){//LIBERATO
       	 perror("errore in malloc");
       	 exit(EXIT_FAILURE);
    	}
	unsigned char* swappertofree;//puntatore ad area di memoria che accogliera il primo pacchetto in ingresso, a questo punto si scambiera di posto con una riga della matrice e cosi via... alla fine quando devo deallocare la memoria devo pero tenere conto dell inidirizzo iniziale.
	if((swappertofree=(unsigned char*)malloc(PACKETSIZE*sizeof(unsigned char)))==NULL){//LIBERATO
        	perror("errore in malloc");
		exit(EXIT_FAILURE);
    	}
	unsigned char* swapper = swappertofree;//nel programma usero swapper cosi swappertofree conterra l indirizzo originale dell area da deallocare.

	memset(swapper,0,PACKETSIZE*sizeof(unsigned char));
	int i;
	// riempio l array di indirizzi
	for(i=0;i<WINDOW;i++){
		pacchettiptr[i]=pacchetti+i*PACKETSIZE;
	}
	//speculare al write, serve per sapere l andamento del trasferimento
  	if(scritti==NULL){
		bytescrittisubuffer=malloc(sizeof(size_t));
		if(bytescrittisubuffer==NULL){
			perror("errore in ptc write");
			exit(EXIT_FAILURE);
		}
		*bytescrittisubuffer=0;	
	}
	else
		bytescrittisubuffer=scritti;


  while(1){
        //leggo il pacchetto
        for (;;) {
		if ((letti=noisy_readn(fd, (void*)swapper, PACKETSIZE)) < 0) {
			perror("errore in readn");
			exit(1);
	    	}	else if (letti>0){
			break;
                	}   
	}
	//leggo il numero del pacchetto e faccio i controlli del caso
        numberreceivedpacket=swapper[0];
	if(numberreceivedpacket>(unsigned char)WINDOW ){
		perror("pacchetot fuori finestra massima");
		exit(EXIT_FAILURE);
	}
	bytes[numberreceivedpacket]=letti;
	if(((startwindow>endwindow)&&((numberreceivedpacket<startwindow)&&(numberreceivedpacket>endwindow)))||( (startwindow<endwindow)&&((numberreceivedpacket<startwindow)||(numberreceivedpacket>endwindow)))){ //fuori finestra va scartato
		continue;
	}	
	//segno che ho il pacchetto pronto ma non e ancora stato scritto sul buffer
	alreadyreceived[numberreceivedpacket]=(unsigned char)1;

	//scambio la riga della matrice corrispondere al numero del pacchetto ricevuto con lo swaper
        unsigned char* swap=NULL;
	swap=swapper;
        swapper=pacchettiptr[numberreceivedpacket];
        pacchettiptr[numberreceivedpacket]=swap;
	//mando l ack (che corrisponde solo al numero del pacchetto)
        for (;;) {
		if ((letti=noisy_writen(fd, (void*)&numberreceivedpacket, sizeof(unsigned char))) < 0) {
			perror("errore in writen");
			exit(1);
		}
		else if(letti>0){
                       	break;
       		}
	}
	//finche l ultimo pacchetto della finestra e ackato avanzo la finestra e scrivo su buffer (ultimo pacchetto della finestra e startwindow, cioe dipende dai punti di vista ovviamente)
        while(alreadyreceived[startwindow]==(unsigned char)1){
		//correggo la dimensione in modo di non andare in overflow e ci terminare la funzione
		if(*bytescrittisubuffer+bytes[startwindow]-1>n){
			bytes[startwindow]=n+1-*bytescrittisubuffer; //e occorso qualche problema evidentemente ma e normale con la selective repeat
		}
		//scrivo sul buffer
		memcpy((void*)(buf+*bytescrittisubuffer),(void*)&pacchettiptr[startwindow][1],bytes[startwindow]-1);
		//aggiono il contatore dei byte scritti
		*bytescrittisubuffer = *bytescrittisubuffer+bytes[startwindow]-1;
		if(VERBOSE==1){
			printf("Totale:%zu,byte scritti su buffer:%zu\n",n,*bytescrittisubuffer);
		}
		if(*bytescrittisubuffer==n){
			//se e big endian riscrivo al contario il buffer- perche ho scelto come standard network little endian-
			if(isitbigendian()==1 && force_endian==0){
				change_endian(buf,n);
			}
			//mando il codice di buffer pieno e two way handshake 
			unsigned char  sendline[2];
			sendline[0]=(unsigned char)254;
			sendline[1]=(unsigned char)250;
			if ((noisy_writen(fd, sendline, 2)) < 0) {
				fprintf(stderr, "errore in write");	
				exit(EXIT_FAILURE);
			}
			int letti=0;
			memset(sendline,0,2*sizeof(unsigned char));
			fd_set set;
			struct timeval timeout,time;
			int rv;
			FD_ZERO(&set); /* clear the set */
			FD_SET(fd, &set); /* add our file descriptor to the set */
			timeout.tv_sec = TIME_OUT;
			timeout.tv_usec = 0;
			if(gettimeofday(&time,NULL)!=0){
				perror("errore in gettimeofday connection request");
				exit(EXIT_FAILURE);
			}
			while(sendline[0]!=(unsigned char)254 && sendline[1]!=(unsigned char)250){
				printf("attendo risposta del server\n");
				rv = select(fd + 1, &set, NULL, NULL, &timeout);
				if(rv == -1){
					perror("errore in select"); /* an error accured */
					exit(EXIT_FAILURE);
				}
				else if(rv == 0){
					perror("Il server non ha risposto entro il TIMEOUT");
					return EXIT_FAILURE;
				}
				struct timeval timenow;
				if(gettimeofday(&timenow,NULL)!=0){
					perror("errore in gettimeofday");
					exit(EXIT_FAILURE);
				}
			       		 if ((timenow.tv_sec - time.tv_sec) >TIME_OUT) {
						perror("Il server non ha risposto entro il TIMEOUT2");
						return EXIT_FAILURE;
					}
				letti=noisy_readn(fd, sendline, 2);
   				if (letti< 0) {
       					 perror("errore in write");
    				   	 exit(EXIT_FAILURE);
    				}
	
			}
			//libero le risorse
			free(pacchetti);
			free(swappertofree);			
			return *bytescrittisubuffer;
			}
		//altrimenti se non devo uscire avanzo la finestra
		alreadyreceived[startwindow]=0;
		startwindow=(startwindow+1)%WINDOW;
		endwindow=(endwindow+1)%WINDOW;
		if(VERBOSE==1){
			printf("inizio finestra: %u, fine finesta %u\n",(unsigned int)startwindow,(unsigned int)endwindow);
		}
 
        }
      
    }
}
//funzione per incrementare il puntatore
//in realta basterebbe usare l operazione di modulo come ho usato in altre funzioni.
void ptrinc(int *ptr)
{
	if (*ptr == WINDOW - 1)
		*ptr = 0;
	else
		(*ptr)++;
}
//controllo se il primo elemento della lista e scaduto
int list_scaduto(list * myroot,intmax_t* time_out)
{
	node *mynode = myroot->head;
	struct timeval nowtime;
	if(gettimeofday(&nowtime,NULL)!=0){
		perror("errore in gettimeofday connection request");
		exit(EXIT_FAILURE);
	}
	if(mynode!=NULL && VERBOSELIST==1){
		printf("tempo:%ju\n",((intmax_t)((nowtime.tv_sec*1000000 + nowtime.tv_usec -(*(struct timeval*)(mynode->dataptr)).tv_sec*1000000 -  (*(struct timeval*)(mynode->dataptr)).tv_usec))));
	}
	if (mynode!=NULL && ((intmax_t)((nowtime.tv_sec*1000000 + nowtime.tv_usec -(*(struct timeval*)(mynode->dataptr)).tv_sec*1000000 -  (*(struct timeval*)(mynode->dataptr)).tv_usec))>=*time_out)) {
		return 0;
	}
	return 1;
}
