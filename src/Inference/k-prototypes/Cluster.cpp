//
// Created by jan on 18.08.16.
//

#include "Inference/k-prototypes/Cluster.h"
#include <Energy/EnergyFunction.h>

void Cluster::updateMean()
{
    mean = accumFeature / size;
}

void Cluster::updateLabel()
{
    std::vector<float> cost(numClasses, 0.f);
    size_t i = 0;
    auto computeDist = [&]()
    {
        float dist = 0;
        for (Label l = 0; l < numClasses; ++l)
        {
            if (l != i)
                dist += labelFrequencies[l] * classDistance(l, i);
        }
        ++i;
        return dist;
    };
    std::generate(cost.begin(), cost.end(), computeDist);
    label = std::distance(cost.begin(), std::min_element(cost.begin(), cost.end()));
}

