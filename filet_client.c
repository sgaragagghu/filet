
#include "basic.h"
#include "filet_thread_client.h"

int main(int argc, char **argv)
{
	argc = argc;//evitare warning
	argv = argv;//evitare warning
	struct thread_arg_t* thread_arg;// struct che condivideranno tutti i thread che fanno da client
	thread_arg=(struct thread_arg_t*)malloc(sizeof(struct thread_arg_t));//alloco lo spazio per la struct sull heap
	if(thread_arg==NULL){ // controllo errore
		perror("errore in malloc main");
		exit(EXIT_FAILURE);
	}
	thread_arg->activethreadid=(size_t*)malloc(sizeof(size_t));//alloco la memoria per l activethreadid. activethredid contiene il numero (cardinale che segue l ordine temporale con il quale i thread sono stati avviati) del thread che attualmente ha il 'possesso' del stdout e stdin. NON E L ID!
	if(thread_arg->activethreadid==NULL){//controllo errore
		perror("errore in malloc threadid main");
		exit(EXIT_FAILURE);
	}
	thread_arg->threadidarraysize=(size_t*)malloc(sizeof(size_t));//array che contiene la grandezza del array che contiene i pthread_t
	if(thread_arg->threadidarraysize==NULL){//contorllo errore
		perror("errore in malloc threadid arraysizemain");
		exit(EXIT_FAILURE);
	}
	thread_arg->threadidarrayptr=(size_t*)malloc(sizeof(size_t));//puntatore all indice dell array(a che punto e stato riempito)
	if(thread_arg->threadidarrayptr==NULL){//controllo errore
		perror("errore in malloc threadid main");
		exit(EXIT_FAILURE);
	}
/*
	thread_arg->threadmutex=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));//
	if(thread_arg->threadmutex==NULL){
		perror("errore in malloc threadmutex main");
		exit(EXIT_FAILURE);
	}
*/
	thread_arg->condmutex=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));// mutex che utilizzero sempre con la condizione sottostante
	if(thread_arg->condmutex==NULL){
		perror("errore in malloc threadmutex main");
		exit(EXIT_FAILURE);
	}
	thread_arg->threadcond=(pthread_cond_t*)malloc(sizeof(pthread_cond_t));// quando si creera o cambiera thread, il thread andra a dormire attendendo questa condizione.quando verra svegliato leggera la variabile activethreadid per capire se tocca a lui prendere il comando.
	if(thread_arg->threadcond==NULL){//controllo errore
		perror("errore in malloc threadcond main");
		exit(EXIT_FAILURE);
	}
	thread_arg->exit=(char*)malloc(sizeof(char));//quando un client ricevera il comando di uscita, verra propagato a tutti i thread tramite questa variabile
	if(thread_arg->exit==NULL){//controllo errore
		perror("errore in malloc exit main");
		exit(EXIT_FAILURE);
	}
	*thread_arg->exit=(char)0;//finche e zero il programma non esce
	*thread_arg->activethreadid=0;//inizialmente il primo thread e quelo attivo
	thread_arg->threadidarray=(pthread_t*)malloc(50*sizeof(pthread_t));//inizialmente l array potra contenere al piu 50 thread
	if(thread_arg->threadidarray==NULL){ // 50 come valore iniziale poi per necessita' verra raddoppiato quando l'array sara' completo.
		perror("error in malloc threadisarray \n");
		exit(EXIT_FAILURE);
	}	
	*thread_arg->threadidarraysize=50;// dimensione attuale dell array
	*thread_arg->threadidarrayptr=0;//inizialmente l array e occupato solo fino al primo posto
/*
	if(pthread_mutex_init(thread_arg->threadmutex,NULL)!=0)
	{
		perror("error in mutex init \n");
		exit(EXIT_FAILURE);
	
	}
*/

	//inizializzo i vari mutex e condition
	if(pthread_mutex_init(thread_arg->condmutex,NULL)!=0)
	{
		perror("error in condmutex init \n");
		exit(EXIT_FAILURE);
	
	}
	if(pthread_cond_init(thread_arg->threadcond,NULL)!=0)
	{
		perror("error in cond init \n");
		exit(EXIT_FAILURE);
	
	}

		
	if (pthread_create(&thread_arg->threadidarray[0], NULL, thread_client, thread_arg)) {
		fprintf(stderr,"error creating first thread \n");
		exit(EXIT_FAILURE);
	}
	

	//attendo tramite il condition finche un thread riceve l exit, imposta la variabile e manda un cond broadcast
	while(*thread_arg->exit==(char)0){
		if(pthread_mutex_lock(thread_arg->condmutex)!=0){
			perror("errore in exit mutex lock");
			exit(EXIT_FAILURE);
		}
		if(pthread_cond_wait(thread_arg->threadcond,thread_arg->condmutex)!=0){
			perror("error in cond wait \n");
			exit(EXIT_FAILURE);
		}
		if(pthread_mutex_unlock(thread_arg->condmutex)!=0){
			perror("errore in exit mutex lock");
			exit(EXIT_FAILURE);
		}
	}
	//joino tutti i thread non veramente necessario in questo caso in realta.
	size_t i;
	for(i=0;i<=*thread_arg->threadidarrayptr;++i){
		if(pthread_join(thread_arg->threadidarray[i],NULL)!=0){
			perror("errore in pthread join exit");
			exit(EXIT_FAILURE);
		}
	}
	printf("Thread principale ha ricevuto comando di uscita\n");
	exit(EXIT_SUCCESS);
}
