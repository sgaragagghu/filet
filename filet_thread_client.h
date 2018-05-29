void *thread_client(void *arg);
void *thread_get(void *a);
void *thread_stamp(void* a);
struct thread_arg_t {
        pthread_t* threadidarray;
        size_t* threadidarraysize;
        size_t* threadidarrayptr;
        size_t* activethreadid;
        pthread_mutex_t* threadmutex;
        pthread_mutex_t* condmutex;
        pthread_cond_t* threadcond;
	char* exit;
};
struct thread_arg_t2 {
        size_t* completato;
        off_t* totale;
        unsigned char   finito;
	size_t* activethreadid;
        pthread_mutex_t* condmutex;
        pthread_cond_t* threadcond;
	size_t* chisono;

};
struct thread_arg_t3 {
        char** comando;
        int* ptrsockfd;
	size_t* activethreadid;
        pthread_mutex_t* condmutex;
        pthread_cond_t* threadcond;
	size_t* chisono;
};
void comandoget(pthread_t* threadid, struct thread_arg_t3* thread_arg);
void getcommand(char** ptr, char* buffer);
int connessione(char* stringa,int* sockptr);
void comandoexit(void *a,size_t* chisonoptr);
void comandomkthread(struct thread_arg_t* thread_arg);
void comandochthread(char* numero,size_t* ptr,  pthread_cond_t* cond,size_t* max);
void comandols(int sockfd);
void comandoput(pthread_t*  workingthread,struct thread_arg_t3* thread_arg);
void comandocd(char* folder,int sockfd);
void comandodisconnetti(int sockfd);
void stamparitorno(int sockfd);
void comandochthread2(size_t* numero,size_t* ptr, pthread_cond_t* cond);
void comandodisconnettierr(int sockfd);
