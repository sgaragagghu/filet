#include "basic.h"

ssize_t writen(int fd, const void *buf, size_t n)
{
	size_t nleft;
	ssize_t nwritten;
	const char *ptr;

	ptr = buf;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			if ((nwritten < 0) && (errno == EINTR))
				nwritten = 0;
			else
				return (-1);	/* errore */
		}
		if(VERBOSE==1){
			printf("scrivo: ");
			int i;
			for(i=0;i<nwritten;i++){
				printf("%u-",(unsigned int)*((unsigned char*)ptr+i));
			}
			printf("\n");
		}
		nleft -= nwritten;
		ptr += nwritten;
	}
	return (n - nleft);
}

int readn(int fd, void *vptr, int maxlen)
{
	int rc;
	if(VERBOSE==1){
		printf("attendo lettura\n");
	}

	if ((rc = read(fd, vptr, maxlen)) <= 0) {
		if (rc < 0){
			return -1;	/* errore */
		}
		else{
			printf("successo ma 0\n");
			return 0;
		}
	}
	if(VERBOSE==1){
		printf("letto: ");
		int i;
		for(i=0;i<rc;i++){
			printf("%u-",(unsigned char)*((unsigned char*)vptr+i));
		}
		printf("\n");
	}
	return (rc);		/* restituisce il numero di byte letti */
}
