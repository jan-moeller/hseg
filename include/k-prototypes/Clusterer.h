//
// Created by jan on 18.08.16.
//

#ifndef HSEG_CLUSTERER_H
#define HSEG_CLUSTERER_H

#include <random>
#include <chrono>
#include <iostream>
#include <helper/coordinate_helper.h>
#include "Cluster.h"

class Clusterer
{
public:
    template<typename T>
    void run(size_t numClusters, size_t numLabels, Image<T,3> const& color, LabelImage const& labels);

    LabelImage const& clustership() const;

    /**
     * Computes the energy of the current configuration given a color image and a label image
     * @param color Color image
     * @param labels Label image
     * @return Energy of the current configuration
     */
    template<typename T>
    float computeEnergy(Image<T, 3> const& color, LabelImage const& labels) const;

private:
    std::vector<Cluster> m_clusters;
    LabelImage m_clustership;
    float m_gamma = 5000.f; // TODO: Find good mixing coefficient
    float m_conv = 0.001f;

    inline float delta(Label l1, Label l2) const
    {
        if(l1 == l2)
            return 0;
        else
            return 1;
    }

    template<typename T>
    void initPrototypes(Image<T, 3> const& color, LabelImage const& labels);

    template<typename T>
    void allocatePrototypes(Image<T, 3> const& color, LabelImage const& labels);

    template<typename T>
    int reallocatePrototypes(Image<T, 3> const& color, LabelImage const& labels);

    size_t findClosestCluster(Feature const& feature, Label classLabel) const;

    float computeDistance(Feature const& feature, Label label, size_t clusterIdx) const;
};

template<typename T>
void Clusterer::run(size_t numClusters, size_t numLabels, Image<T, 3> const& color, LabelImage const& labels)
{
    assert(color.pixels() == labels.pixels());

    m_clusters.resize(numClusters, Cluster(numLabels));
    m_clustership = LabelImage(color.width(), color.height());

    float energy = computeEnergy(color, labels);
    std::cout << "Energy before cluster allocation: " << energy << std::endl;

    initPrototypes(color, labels);
    allocatePrototypes(color, labels);

    energy = computeEnergy(color, labels);
    std::cout << "Energy after cluster allocation: " << energy << std::endl;

    int moves = color.pixels(), lastMoves;
    int iter = 0;
    do
    {
        iter++;
        lastMoves = moves;
        moves = reallocatePrototypes(color, labels);

        energy = computeEnergy(color, labels);
        std::cout << "Energy after iteration " << iter << ": " << energy << std::endl;

        //std::cout << iter << ": moves: " << moves << ", diff: " << std::abs(lastMoves - moves) << ", threshold: " << rgb.pixels() * m_conv << std::endl;
    } while (std::abs(lastMoves - moves) > color.pixels() * m_conv);

    std::cout << "Converged after " << iter << " iterations (up to convergence criterium)" << std::endl;
    long int lostClusters = std::count_if(m_clusters.begin(), m_clusters.end(), [](Cluster const& c) {return c.size == 0;});
    std::cout << lostClusters << " clusters are empty." << std::endl;
}

template<typename T>
void Clusterer::initPrototypes(Image<T, 3> const& color, LabelImage const& labels)
{
    // Randomly select k objects as initial prototypes
    std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<size_t> distribution(0, color.pixels() - 1);
    for (auto& c : m_clusters)
    {
        size_t site = distribution(generator);
        c.feature = c.accumFeature = Feature(color, site);
        auto coords = helper::coord::siteTo2DCoordinate(site, color.width());
        c.label = labels.at(coords.first, coords.second, 0);
    }
}

template<typename T>
void Clusterer::allocatePrototypes(Image<T, 3> const& color, LabelImage const& labels)
{
    for (size_t i = 0; i < color.pixels(); ++i)
    {
        Feature curFeature(color, i);
        Label curLabel = labels.at(i, 0);

        // Compute closest cluster
        size_t minCluster = findClosestCluster(curFeature, curLabel);

        // Assign to cluster
        m_clustership.at(i, 0) = minCluster;
        m_clusters[minCluster].size++;
        m_clusters[minCluster].accumFeature += curFeature;
        m_clusters[minCluster].feature = m_clusters[minCluster].accumFeature / m_clusters[minCluster].size;
        m_clusters[minCluster].labelFrequencies[curLabel]++;
        m_clusters[minCluster].label = std::distance(m_clusters[minCluster].labelFrequencies.begin(),
                                                     std::max_element(m_clusters[minCluster].labelFrequencies.begin(),
                                                                      m_clusters[minCluster].labelFrequencies.end()));
    }
}

template<typename T>
int Clusterer::reallocatePrototypes(Image<T, 3> const& color, LabelImage const& labels)
{
    int moves = 0;
    for (size_t i = 0; i < color.pixels(); ++i)
    {
        Feature curFeature(color, i);
        Label curLabel = labels.at(i, 0);

        // Compute closest cluster
        size_t minCluster = findClosestCluster(curFeature, curLabel);
        size_t oldCluster = m_clustership.at(i, 0);

        if (oldCluster != minCluster)
        {
            moves++;
            m_clustership.at(i, 0) = minCluster;
            m_clusters[minCluster].size++;
            m_clusters[oldCluster].size--;
            m_clusters[minCluster].accumFeature += curFeature;
            m_clusters[oldCluster].accumFeature -= curFeature;
            m_clusters[minCluster].feature = m_clusters[minCluster].accumFeature / m_clusters[minCluster].size;
            m_clusters[oldCluster].feature = m_clusters[oldCluster].accumFeature / m_clusters[oldCluster].size;
            m_clusters[minCluster].labelFrequencies[curLabel]++;
            m_clusters[oldCluster].labelFrequencies[curLabel]--;
            m_clusters[minCluster].label = std::distance(m_clusters[minCluster].labelFrequencies.begin(),
                                                         std::max_element(
                                                                 m_clusters[minCluster].labelFrequencies.begin(),
                                                                 m_clusters[minCluster].labelFrequencies.end()));
            m_clusters[oldCluster].label = std::distance(m_clusters[oldCluster].labelFrequencies.begin(),
                                                         std::max_element(
                                                                 m_clusters[oldCluster].labelFrequencies.begin(),
                                                                 m_clusters[oldCluster].labelFrequencies.end()));
        }
    }
    return moves;
}

template<typename T>
float Clusterer::computeEnergy(Image<T, 3> const& color, LabelImage const& labels) const
{
    float energy = 0;

    for (size_t i = 0; i < m_clustership.pixels(); ++i)
    {
        Feature f(color, i);
        Label l = labels.at(i, 0);
        size_t j = m_clustership.at(i, 0);
        float pxEnergy = computeDistance(f, l, j);
        energy += pxEnergy;
    }

    return energy;
}

#endif //HSEG_CLUSTERER_H
