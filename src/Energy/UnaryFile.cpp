//
// Created by jan on 17.08.16.
//

#include <fstream>
#include <cstring>
#include <boost/endian/conversion.hpp>
#include "Energy/UnaryFile.h"

UnaryFile::UnaryFile(std::string const& filename)
{
    read(filename);
}

Label UnaryFile::maxLabelAt(Coord x, Coord y) const
{
    Label maxLabel = 0;
    float maxVal = std::numeric_limits<float>::min();
    for (Label l = 0; l < m_classes; ++l)
    {
        if (at(x, y, l) > maxVal)
        {
            maxVal = at(x, y, l);
            maxLabel = l;
        }
    }
    return maxLabel;
}

LabelImage UnaryFile::maxLabeling() const
{
    LabelImage labeling(m_width, m_height);
    for (Coord x = 0; x < m_width; ++x)
        for (Coord y = 0; y < m_height; ++y)
            labeling.at(x, y) = maxLabelAt(x, y);
    return labeling;
}

bool UnaryFile::read(std::string const& filename)
{
    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open())
    {
        long int fileSize = file.tellg();
        char fileHeader[12];
        file.seekg(0, std::ios::beg);
        file.read(fileHeader, 12);

        if (std::strncmp(fileHeader, "PROB", 4) != 0)
        {
            m_valid = false;
            file.close();
            return false;
        }
        m_height = static_cast<Coord>(boost::endian::little_to_native(*reinterpret_cast<int*>(fileHeader + 4)));
        m_width = static_cast<Coord>(boost::endian::little_to_native(*reinterpret_cast<int*>(fileHeader + 8)));

        m_data.resize((static_cast<size_t>(fileSize) - 12) / sizeof(float));
        file.seekg(12, std::ios::beg);
        file.read(reinterpret_cast<char*>(m_data.data()), fileSize - 12);
        file.close();

        m_valid = true;
    }
    return m_valid;
}

bool UnaryFile::write(std::string const& filename)
{
    if(!m_valid)
        return false;

    std::ofstream file(filename, std::ios::out | std::ios::binary | std::ios::ate);
    if(file.is_open())
    {
        file.write("PROB", 4);
        int width = boost::endian::native_to_little(m_width), height = boost::endian::native_to_little(m_height);
        file.write(reinterpret_cast<char const*>(&height), sizeof(height));
        file.write(reinterpret_cast<char const*>(&width), sizeof(width));
        file.write(reinterpret_cast<char const*>(m_data.data()), sizeof(m_data[0]) * m_data.size());
        file.close();
        return true;
    }
    return false;
}

void UnaryFile::rescale(float factor)
{
    std::vector<cv::Mat> layers;
    layers.reserve(m_classes);

    for(Label c = 0; c < m_classes; ++c)
    {
        cv::Mat img(m_height, m_width, CV_32FC1);
        for (Coord y = 0; y < m_height; ++y)
        {
            for (Coord x = 0; x < m_width; ++x)
            {
                img.at<float>(y, x) = at(x, y, c);
            }
        }

        cv::Mat resized;
        cv::resize(img, resized, cv::Size(), factor, factor, cv::INTER_LINEAR);
        layers.push_back(resized);
    }

    for (Label c = 0; c < layers.size(); ++c)
    {
        m_width = static_cast<Coord>(layers[c].cols);
        m_height = static_cast<Coord>(layers[c].rows);
        m_data.resize(m_width * m_height * m_classes, 0);

        for (Coord y = 0; y < m_height; ++y)
        {
            for (Coord x = 0; x < m_width; ++x)
            {
                auto score = layers[c].at<float>(y, x);
                at(x, y, c) = score;
            }
        }
    }
}

Cost UnaryFile::minScore() const
{
    return *std::min_element(m_data.begin(), m_data.end());
}

Cost UnaryFile::maxScore() const
{
    return *std::max_element(m_data.begin(), m_data.end());
}

void UnaryFile::makePositiveBaseZero()
{
    Cost min = minScore();
    for(auto& d : m_data)
        d -= min;
}

void UnaryFile::makeNegativeBaseZero()
{
    Cost max = maxScore();
    for(auto& d : m_data)
        d -= max;
}
