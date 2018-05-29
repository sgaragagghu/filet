#include "utils.h"
//per sapere se il file esiste.... in realta ha dei falsi negativi, per esempio se on abbiamo il pemresso di lettura di quel file..
unsigned char file_exists(char* path){
	FILE *fp = fopen (path, "r");
   	if (fp==NULL)
		return 0;
	if(fclose(fp)!=0){
		perror("errore in fclose");
		exit(EXIT_FAILURE);
	}
   	return 1;
}
uintmax_t maxnum(size_t n){
	if (n==1)
		return 256;
	uintmax_t i=256*maxnum(n-1);
	if(n>0 && i==0){//ha wrappato
		return i-1;
	}
	else
		return i;
}

unsigned char isitbigendian(){
	int i=1;
	if(*(unsigned char*)&i==0)
		return 1;
	else
		return 0;
}
void change_endian(unsigned char* buf,size_t n){
	unsigned char swap;
	size_t i;
	for(i=0;i<n/2;++i){
		swap=buf[i];
		buf[i]=buf[n-i-1];
		buf[n-i-1]=swap;
	}
	
}
