#include <sys/mman.h>
#include <assert.h>

void __attribute__((constructor)) constr(void)
{
  assert(MAP_FAILED != mmap(1u<<31, (1u<<31) - (1u<<20), PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
}
