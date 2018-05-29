#include "basic.h"
#include "filet_thread_server.h"

int main(int argc, char **argv)
{
	argc = argc;//per non avere warning
	argv = argv;//per non avere warning
	pthread_t mainthread;//pthread_t del thread principare. In realta non e utile, non c e un thread principale. Questo thread creera a sua volta altri thread di cui non si avranno informazioni.Il tutto per minimazzare l utilizzo di risorse.
	pthread_t cleanerthread;//thread che si occupera di joinare i thread che si sono spenti.
	if (pthread_create(&mainthread, NULL, thread_function, NULL)) { //creazione del thread e relativo controllo errore
		fprintf(stderr,"error creating first thread \n");
		exit(EXIT_FAILURE);
	}
	if (pthread_create(&cleanerthread, NULL, thread_cleaner, NULL)) { // creazione del thread e relativo controllo errore
		fprintf(stderr,"error creating cleaner thread \n");
		exit(EXIT_FAILURE);
	}

	char stringa[10]; // stringa per contenere il comando da stdin
	do { //questo blocco andra avanti fino alla fine del progamma, si potra terminare l esecuzione solamente da qui scrivendo in stdin exit
		printf("%s", "exit per terminare \n");
		if (scanf("%s", stringa) == -1) {
			fprintf(stderr, "errore in scanf thread principale\n");
			exit(EXIT_FAILURE);
		}
	}
	while (strcmp(stringa, "exit") != 0);
	exit(EXIT_SUCCESS);
}
