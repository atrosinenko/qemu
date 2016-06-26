#include <stdlib.h>

#define Error void
#define NetdevTapOptions void

int tap_open(char *ifname, int ifname_size, int *vnet_hdr,
             int vnet_hdr_required, int mq_required, Error **errp)
{
    abort();
}

void tap_set_sndbuf(int fd, const NetdevTapOptions *tap, Error **errp)
{
    abort();
}

int tap_probe_vnet_hdr(int fd)
{
    abort();
}

int tap_probe_has_ufo(int fd)
{
    abort();
}

int tap_probe_vnet_hdr_len(int fd, int len)
{
    abort();
}

void tap_fd_set_vnet_hdr_len(int fd, int len)
{
    abort();
}

int tap_fd_set_vnet_le(int fd, int is_le)
{
    abort();
}

int tap_fd_set_vnet_be(int fd, int is_be)
{
    abort();
}

void tap_fd_set_offload(int fd, int csum, int tso4,
                        int tso6, int ecn, int ufo)
{
    abort();
}

int tap_fd_enable(int fd)
{
    abort();
}

int tap_fd_disable(int fd)
{
    abort();
}

int tap_fd_get_ifname(int fd, char *ifname)
{
    abort();
}
