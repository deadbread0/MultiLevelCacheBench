#include "../include/all_caches.hpp"

#if !defined(ARC_TEST) && !defined(LIRS_TEST) && !defined(LFU_TEST) && !defined(TWO_Q_TEST)
int main() 
{
    long long* prms = GetNums();
    if (!prms) return 1;
    
    levels_t* config = GetConfig(prms[0]);
    
    long long hits = 0;
    int error = RunLevelCache(config, prms, &hits);
    std::cout << hits << std::endl;
    free(prms);
    return error;
}
#endif
