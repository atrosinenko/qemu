#define _GNU_SOURCE
#include <resolv.h>
#include <netdb.h>

int res_query(const char *name, int class, int type, unsigned char *dest, int len)
{
	if (class != 1 || len < 512)
		return -1;
	h_errno = HOST_NOT_FOUND;
	return -1;
}

