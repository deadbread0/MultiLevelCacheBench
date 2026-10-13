
#include <iostream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "include/cacheLFU.hpp"
#include "include/cacheLIRS.hpp"
#include "include/cacheARC.hpp"
#include "include/cache2Q.hpp"
#include "include/all_caches.hpp"

#if defined(LIRS_TEST) || defined(LFU_TEST) || defined(ARC_TEST) || defined(TWO_Q_TEST)

TEST(CacheTest, ZeroCapacity)
{
  long long prms[] = {0, 3, 1, 2, 1};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 0); 
}

TEST(CacheTest, LargeKeyValues)
{
  long long prms[] = {2, 4, 9999999999LL, 8888888888LL, 9999999999LL, 7777777777LL};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 1); 
}

TEST(CacheTest, NegativeKeyValues)
{
  long long prms[] = {2, 4, -1, -2, -1, -3};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 1); 
}

// check error
TEST(CacheTest, NegativeCacheSize)
{
  long long prms[] = {-2, 2, 2, 5};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 1);
}

TEST(CacheTest, test1)
{
  long long prms[] = {2, 3, 1, 2, 3};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 0);
}

TEST(CacheTest, test2)
{
  long long prms[] = {2, 3, 1, 2, 1};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 1);
}


TEST(CacheTest, test3)
{
  long long prms[] = {1, 5, 1, 1, 1, 2, 1};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  #ifdef LFU_TEST
  EXPECT_EQ(hits, 2); 
  #endif
  #ifdef LIRS_TEST
  EXPECT_EQ(hits, 3); // lirs counts irr
  #endif
}

TEST(CacheTest, test4)
{
  long long prms[] = {2, 8, 1, 1, 1, 1, 2, 3, 4, 1};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  #if defined(LFU_TEST)
  EXPECT_EQ(hits, 4); 
  #else
  EXPECT_GE(hits, 3); 
  #endif
}

TEST(CacheTest, test5)
{
  long long prms[] = {2, 6, 5, 5, 5, 5, 5, 5};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 5);
}

TEST(CacheTest, test6)
{
  long long prms[] = {3, 12, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(err, 0);
  #ifdef LFU_TEST
  EXPECT_EQ(hits, 0); 
  #endif
  #ifdef LIRS_TEST
  EXPECT_EQ(hits, 4); //тк lirs учитывает частоту 
  #endif
}

TEST(CacheTest, test7)
{
  long long prms[] = {3, 10, 10, 20, 1, 10, 20, 2, 10, 20, 3, 10};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 5); 
}

// testing 1 request
TEST(CacheTest, test8)
{
  long long prms[] = {1, 1, 1};
  long long hits = 0;
  int err = RunFunc(prms, &hits);
  EXPECT_EQ(err, 0);
  EXPECT_EQ(hits, 0); 
}

// testing hot data
TEST(CacheTest, test9) {
    
    std::vector<long long> prms_vec = {50, 50 + 50 + 500 + 50};
    
    for (int i = 1; i <= 50; ++i) prms_vec.push_back(i);
    for (int i = 1; i <= 50; ++i) prms_vec.push_back(i);
    for (int i = 1000; i < 1500; ++i) prms_vec.push_back(i);
    for (int i = 1; i <= 50; ++i) prms_vec.push_back(i);

    long long hits = 0;
    int err = RunFunc(prms_vec.data(), &hits);
    
    EXPECT_EQ(err, 0);
    
    #if defined(LFU_TEST)
        EXPECT_GE(hits, 90);
    #else
        EXPECT_GE(hits, 50); 
    #endif
}

// testing big amount of requests
TEST(CacheBenchmark, All_heats) {
    int cache_size = 5000;
    int total_requests = 2000000; 
    
    std::vector<long long> prms_vec;
    prms_vec.reserve(total_requests + 2);
    prms_vec.push_back(cache_size);
    prms_vec.push_back(total_requests);
    
    for (int i = 0; i < total_requests; ++i) {
        unsigned int prm = (i) % cache_size;
        prms_vec.push_back(prm + 1);
    }
    
    long long hits = 0;
    int err = RunFunc(prms_vec.data(), &hits);
    
    EXPECT_EQ(err, 0);
    EXPECT_EQ(hits, 2000000 - 5000);
}



int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::InitGoogleMock(&argc, argv);
  
  return RUN_ALL_TESTS();
}
#endif