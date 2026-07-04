// Minimal sanity test: proves the CTest wiring on both CI platforms.
// Real tests (model, .ubt serialisation, sequencer timing) come with phase 2.
#include <cstdio>

int main()
{
    static_assert (sizeof (void*) == 8, "64-bit build expected");
    std::puts ("sanity: ok");
    return 0;
}
