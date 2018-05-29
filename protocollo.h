int accept_connections(int sockfd, void *buff, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
ssize_t ptc_write(int fd, const void *buf, size_t n,size_t* scritti,unsigned char force_endian);
ssize_t ptc_read(int fd, void *buf, size_t n, size_t* scritti,unsigned char force_endian);
void setnonblocking(int sock);
void setblocking(int sock);
void ptrinc(int *ptr);
int list_scaduto(list *myroot,intmax_t* time_out);
int connection_request(int sockd);
//void* ptc_malloc(size_t size);
