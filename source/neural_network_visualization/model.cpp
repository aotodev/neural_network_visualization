#include "model.h"
#include <gensou/components.h>
#include <string>

void model::on_init()
{
    biases.reserve(256);
    weights.reserve(4096);
}

void model::load(const std::string& path)
{
    auto gsData = gs::system::load_file(path);
    if(!gsData)
        return;

    auto streamBuffer = gsData->data_as_buffer_stream();
    std::istream csv_stream(&streamBuffer);

    if (csv_stream.fail())
    {
        LOG(error, "failed to load file ann model");
        return;
    }

    std::string line, temp;
    std::stringstream stream;

    std::getline(csv_stream, line);
    if(line != "layout")
    {
        LOG(error, "failed to load ann model, bad layout");
        return;
    }

    std::getline(csv_stream, line);
    stream << line;

    uint32_t count = 0;
    std::pair<uint32_t , uint32_t> pair;

    std::getline(stream, line, ',');
    pair.first = std::stoul(line);
    m_neuron_offsets.push_back(0);
    uint32_t offset = 0;

    while(std::getline(stream, line, ','))
    {
        pair.second = std::stoul(line);
        layout.push_back(pair);
        
        offset += pair.first;
        m_neuron_offsets.push_back(offset);

        pair.first = pair.second;
    }

    std::getline(csv_stream, line);
    if(line != "biases")
    {
        LOG(error, "failed to load ann model, bad biases");
        layout.clear();
        return;
    }

    std::getline(csv_stream, line);
    stream.str(std::string());
    stream.clear();
    stream << line;

    while(std::getline(stream, line, ','))
    {
        biases.push_back(std::stof(line));
    }

    std::getline(csv_stream, line);
    if(line != "weights")
    {
        LOG(error, "failed to load ann model, bad weights");
        layout.clear();
        biases.clear();
        return;
    }

    std::getline(csv_stream, line);
    stream.str(std::string());
    stream.clear();
    stream << line;

    while(std::getline(stream, line, ','))
    {
        weights.push_back(std::stof(line));
    }

    LOG(info, "layout:");
    for(auto [input, output] : layout)
        LOG(info, "[%u, %u]", input, output);

    LOG(info, "biases count == %zu, weights count == %zu", biases.size(), weights.size());
}