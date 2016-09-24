#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <map>

extern "C" void invalidate_tb(int tb_ptr);

static int tb_start;
static std::map<int, int> tb_length;
extern "C" void on_tb_start(int tb_ptr)
{
    tb_start = tb_ptr;
    invalidate_tb(tb_ptr);
}
extern "C" void on_tb_end(int tb_ptr)
{
    if(tb_length.lower_bound(tb_start) != tb_length.lower_bound(tb_ptr - 1))
        abort();
    tb_length[tb_start] = tb_ptr - tb_start;
}

extern "C" int get_tb_start_and_length(int ptr, int *start, int *length)
{
    std::map<int, int>::const_iterator it = tb_length.lower_bound(ptr);
    if(it->first > ptr)
    {
        if(it == tb_length.begin())
            return 0;
        --it;
    }
    *start = it->first;
    *length = it->second;
    return (*start <= ptr) && (ptr < *start + *length);
}