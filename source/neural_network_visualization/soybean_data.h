#pragma once

#include <gensou/core.h>

/* just a helper to load our soybean data */
inline std::vector<float> load_soybean_series(const std::string& path, bool normalize, uint32_t obervationCount = 0)
{
    auto gsData = gs::system::load_file(path);
    if(!gsData)
        std::vector<float>();

    auto streamBuffer = gsData->data_as_buffer_stream();
    std::istream csv_stream(&streamBuffer);

    if (csv_stream.fail())
        LOG(error, "failed to load file soybean data");

    std::string line;

    std::vector<float> outData;
    outData.reserve(obervationCount ? obervationCount : 1024);

    while (std::getline(csv_stream, line))
        outData.emplace_back(std::stof(line));

    /* release any extra memory if needed */
    outData.shrink_to_fit();

    if(normalize)
    {
        float min = std::numeric_limits<float>::max();
        float max = 0.0f;

        for(auto f : outData)
        {
            min = std::min(f, min);
            max = std::max(f, max);
        }
        
        for(auto& f : outData)
            f = (f - min) / (max - min);
    }

    LOG(trace, "loaded data size == %zu", outData.size());

    return outData;
}
