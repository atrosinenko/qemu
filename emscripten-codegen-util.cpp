#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <map>

extern "C" void invalidate_tb(int tb_ptr);
extern "C" void fast_invalidate_tb(int tb_ptr);

static std::map<int, int> tb_length;

extern "C" void invalidate_range(int start_, int end_);

extern "C" void on_tb_start(int tb_ptr)
{
    invalidate_tb(tb_ptr);
}
extern "C" void on_tb_end(int tb_start, int tb_ptr)
{
    if(tb_length.lower_bound(tb_start) != tb_length.lower_bound(tb_ptr - 1))
        invalidate_range(tb_start, tb_ptr);
    tb_length[tb_start] = tb_ptr - tb_start;
}

extern "C" int get_tb_start_and_length(uint8_t *ptr, uint8_t **start, int *length)
{
    std::map<int, int>::const_iterator it = tb_length.lower_bound((int)ptr);
    if(it->first > (int)ptr)
    {
        if(it == tb_length.begin())
            return 0;
        --it;
    }
    *start = (uint8_t *)it->first;
    *length = it->second;
    return (*start <= ptr) && (ptr < *start + *length);
}

extern "C" void invalidate_range(int start_, int end_)
{
    std::map<int, int>::const_iterator start = tb_length.lower_bound(start_);
    std::map<int, int>::const_iterator end = tb_length.lower_bound(end_);
    if(start->first != start_ && start != tb_length.begin() && start != tb_length.end())
        --start;
    for(std::map<int, int>::const_iterator it = start; it != end; ++it)
        fast_invalidate_tb(it->first);
}
