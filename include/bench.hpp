#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <random>
#include <cmath>
#include <set>
#include <unordered_map>

#include "../include/all_caches.hpp"

long long ZipfTest(int cache_size, int total_requests, std::vector<long long> prms_vec);
std::vector<long long> generate_zipf_vector(int cache_size, int total_requests, double alpha = 0.95);
long long OptZipfTest(int cache_size, int total_requests, std::vector<long long> prms_vec);