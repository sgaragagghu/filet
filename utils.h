//#include <sys/types.h>
//#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

unsigned char file_exists(char* path);
uintmax_t maxnum(size_t n);
unsigned char isitbigendian();
void change_endian(unsigned char* buf,size_t n);

