#include <ctype.h>
#include <assert.h>
#include <map>

static int tb_start;
static std::map<int, int> tb_length;
extern "C" void on_tb_start(int tb_ptr)
{
    tb_start = tb_ptr;
}
extern "C" void on_tb_end(int tb_ptr)
{
    tb_length[tb_start] = tb_ptr - tb_start;
}

extern "C" void get_tb_start_and_length(int ptr, int *start, int *length)
{
    std::map<int, int>::const_iterator it = tb_length.lower_bound(ptr);
    if(it->first > ptr)
    {
        assert(it != tb_length.begin());
        --it;
    }
    *start = it->first;
    *length = it->second;
}