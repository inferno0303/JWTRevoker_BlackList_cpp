#include "MultiBloomFilter.h"

// 构造函数的实现
MBF::MultiBloomFilter::MultiBloomFilter(time_t MAX_JWT_LIFETIME,
                                        time_t FILTER_ROTATION_TIME,
                                        size_t BLOOMFILTER_SIZE,
                                        unsigned int HASH_FUNCTION_NUM)
    : MAX_JWT_LIFETIME(MAX_JWT_LIFETIME),
      FILTER_ROTATION_TIME(FILTER_ROTATION_TIME),
      BLOOMFILTER_SIZE(BLOOMFILTER_SIZE),
      HASH_FUNCTION_NUM(HASH_FUNCTION_NUM)
{
    // 计算布隆过滤器的个数
    NUM_FILTERS = std::ceil(MAX_JWT_LIFETIME / FILTER_ROTATION_TIME);

    // 初始化 BloomFilter 对象并添加到向量中
    for (unsigned int i = 0; i < NUM_FILTERS; ++i)
    {
        filters.push_back(BF::BloomFilter(BLOOMFILTER_SIZE));
    }
}

void MBF::MultiBloomFilter::revoke_jwt(const std::string &jwt_token, time_t exp_time)
{
    time_t remaining_time = exp_time - time(nullptr);
    int num_filters = std::ceil(remaining_time / FILTER_ROTATION_TIME);
    for (int i = 0; i < num_filters; ++i)
    {
        filters[i].add(jwt_token);
    }
}

bool MBF::MultiBloomFilter::is_jwt_revoked(const std::string &jwt_token, time_t exp_time)
{
    time_t remaining_time = exp_time - time(nullptr);
    int num_filters = std::ceil(remaining_time / FILTER_ROTATION_TIME);
    for (int i = 0; i < num_filters; ++i)
    {
        if (filters[i].contains(jwt_token))
        {
            return true;
        }
    }
    return false;
}

void MBF::MultiBloomFilter::rotate_filters()
{
    filters.erase(filters.begin());
    filters.push_back(BF::BloomFilter(BLOOMFILTER_SIZE));
}
