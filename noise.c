#include "basic.h"
//con probabilita PROBERRSEND fara "finta" di spedire il pacchetto
ssize_t noisy_writen(int fd, const void *buf, size_t n){
        if((rand()%100)<PROBERRSEND){
		if(VERBOSE==1){
			printf("burla\n");
		}
                return n;
        }
	return writen(fd,buf,n);
}
//qui non fa nulla di strano, perche perdere un pacchetto in scritto e equivalente che non riceverlo
int noisy_readn(int fd, void *vptr, int maxlen){
	return readn(fd,vptr,maxlen);
}
