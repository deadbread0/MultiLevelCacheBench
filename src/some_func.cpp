#include "../include/all_caches.hpp"

Page slow_get_page(PageId key) { return key; }

bool read_integer(long long &value) 
{
  if (!(std::cin >> value))
    return false;
    
  const auto next = std::cin.peek();

  return !std::cin.bad() && 
         (next == std::char_traits<char>::eof() || 
          std::isspace(static_cast<unsigned char>(next)));
}

long long *GetNums()
{
    long long m, n;

    if (!read_integer(m) || !read_integer(n) || m < 0 || n < 0 ||
        static_cast<unsigned long long>(m) > std::numeric_limits<std::size_t>::max())
    { 
        std::cerr << "Expected nonnegative cache size and request count\n";
        return nullptr;
    }

    long long *prms = (long long*)calloc(n + 2, sizeof(long long));
    if (!prms) return nullptr;
    
    prms[0] = m;
    prms[1] = n;

    for (long long i = 0; i < n; i++)
    {
        if (!read_integer(prms[i + 2]))
        {
            std::cerr << "Not enough numbers entered\n";
            free(prms);
            return nullptr;
        }
    }

    return prms;
}

// only for 1-level-cache (tests)
#if defined(LFU_TEST) || defined(LFU) || defined(LIRS_TEST) || defined(LIRS) || defined(ARC_TEST) || defined(ARC) || defined(TWO_Q_TEST) || defined(TWO_Q)

int RunFunc(long long *prms, long long *hits) 
{
    if (!prms) return 1;
    long long m = prms[0], n = prms[1];

    if (m < 0 || n < 0) 
    { 
        std::cerr << "Expected nonnegative cache size and request count\n";
        return 1;
    }

    caches::cache_t<Page, PageId> cache{static_cast<std::size_t>(m)};

    for (long long i = 0; i < n; i++) 
    {
        PageId key = prms[i + 2];
        if (!key) 
        {
            std::cerr << "Expected a page key\n";
            return 1;
        }
        
        auto res = cache.lookup_update(key, std::nullopt);
        if (res.is_hit) 
        {
            (*hits)++;
            cache.lookup_update(key, key); 
        }
        else 
        {
            Page page = slow_get_page(key);
            cache.lookup_update(key, page);
        }
    }
    return std::cout ? 0 : 1;
}

#else

// for level-cache
int RunFunc(long long *prms, long long *hits) {
    (void)prms;
    (void)hits;
    return 0;
}

#endif


levels_t* GetConfig(int cache_size)
{
    char* buf = PutDataFromFileToBuffer("../src/config.txt");

    int amount_of_levels = atoi(buf);
    buf += CountAmountOfDigits(amount_of_levels);

    levels_t* all_levels = (levels_t*)calloc(amount_of_levels, sizeof(levels_t));

    for (int i = 0; i < amount_of_levels; i++)
    {
        char* word = GetWordFromBuf(buf);
        buf += (strlen(word) + 1);

        for (int c = 0; c < amount_of_levels_cmp; c++)
        {
            if (strncmp(word, levels_arr[c].name, MAX_SIZE_OF_WORD) == 0)
                all_levels[i].level_num = levels_arr[c].level_enum;
        }
    }

    for (int i = 0; i < cache_size; i++)
    {
        int iterator = i % amount_of_levels;

        all_levels[iterator].size++;
    }

    return all_levels;
}

char* PutDataFromFileToBuffer(const char* input_file)
{
    int size = GetSizeOfInputFile(input_file);

    char* buffer = (char*)calloc(size + 1, sizeof(char));

    if (!buffer)
    {
        printf("memory allocation error, try again\n");
        return nullptr;
    }

    FILE* filee = fopen(input_file, "r");
    fread(buffer, sizeof(char), size + 1, filee);

    if (!buffer)
    {
        printf("problems with reading file, try again\n");
        return nullptr;
    }

    fclose(filee);
    return buffer;
}

int GetSizeOfInputFile(const char* file_name)
{
    assert(file_name != nullptr);

    struct stat StructStat;
    stat(file_name, &StructStat);
    return StructStat.st_size;
}

char* GetWordFromBuf(char* buf)
{
    int i = 0, j = 0;
    char* word = (char*)calloc(MAX_SIZE_OF_WORD, sizeof(char));

    while (buf[i] == ' ')
        i++;

    while (isalnum(buf[i]))
    {
        word[j] = buf[i];
        i++; j++;
    }

    return word;
}

int CountAmountOfDigits(int num)
{
    int c = 0;
    while (num > 0)
    {
        num = num / 10;
        c++;
    }
    return c;
}
