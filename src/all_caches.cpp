#include "../include/all_caches.hpp"

#if !defined(ARC_TEST) && !defined(LIRS_TEST) && !defined(LFU_TEST) && !defined(TWO_Q_TEST)
int main() 
{
    std::vector<long long> prms = GetNums();
    if (prms.empty()) return 1;
    if (prms[0] == 0)
    {
        std::cout << 0 << std::endl;
        return 0;
    }
    
    std::vector<levels_t> config = GetConfig(prms[0]);
    
    long long hits = 0;
    int error = RunLevelCache(config.data(), prms.data(), &hits);
    std::cout << hits << std::endl;
    return error;
}
#endif
