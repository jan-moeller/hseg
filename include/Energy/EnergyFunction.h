//
// Created by jan on 20.08.16.
//

#ifndef HSEG_ENERGYFUNCTION_H
#define HSEG_ENERGYFUNCTION_H

#include <Image/FeatureImage.h>
#include "Weights.h"
#include "typedefs.h"

/**
 * Provides functionality to compute (partial) energies of the target energy function.
 * @note This will ignore pixels that have been assigned labels not in the range [0, classMax].
 */
class EnergyFunction
{
public:
    /**
     * Constructor
     * @param weights Weights to use. The pointer must stay valid as long as this object persists.
     * @param numClusters Amount of clusters to use
     */
    EnergyFunction(Weights const* weights, ClusterId numClusters);

    /**
     * Computes the overall energy
     * @param features Feature image
     * @param labeling Labeling of the image
     * @return The energy of the given configuration
     */
    Cost giveEnergy(FeatureImage const& features, LabelImage const& labeling) const;

    /**
     * Computes the overall energy by weights. I.e. to compute the actual energy, the result needs to be multiplied by
     * the weights vector.
     * @param features Feature image
     * @param labeling Labeling of the image
     * @return The energy of the given configuration by weights
     */
    Weights giveEnergyByWeight(FeatureImage const& features, LabelImage const& labeling) const;

    /**
    * Computes the unary energy by weights
    * @param features Feature image
    * @param labeling Class labeling
    * @param[out] energyW The unary energy of the given configuration will be stored here by (unary) weights
    * @return The unary energy of the given configuration
    */
    void computeUnaryEnergyByWeight(FeatureImage const& features, LabelImage const& labeling, Weights& energyW) const;

    /**
     * Computes the pairwise energy by weights
     * @param features Feature image
     * @param labeling Class labeling
     * @param[out] energyW The pairwise energy of the given configuration will be stored here by (pairwise) weights
     */
    void computePairwiseEnergyByWeight(FeatureImage const& features, LabelImage const& labeling, Weights& energyW) const;

    /**
     * @return Amount of classes
     */
    inline Label numClasses() const
    {
        return static_cast<Label>(m_pWeights->numClasses());
    }

    /**
     * @return Amount of clusters
     */
    inline ClusterId numClusters() const
    {
        return m_numClusters;
    }

    /**
     * Computes the cost of a unary-class label combination
     * @param i Site
     * @param f Feature to consider
     * @param l Class label to compute score for
     * @return The cost of assigning class label \p l to pixel \p i
     */
    inline Cost unaryCost(SiteId /*i*/, Feature const& f, Label l) const
    {
        assert(l < numClasses());

        auto const& w = m_pWeights->unary(l);
        auto const& wHead = w.head(w.size() - 1);
        return wHead.dot(f) + w(w.size() - 1);
    }

    /**
     * Computes the cost of a pairwise connection as given by the labels of the pixels
     * @param f1 First feature
     * @param f2 Second feature
     * @param l1 First label
     * @param l2 Second label
     * @return The cost
     */
    inline Cost pairwiseCost(Feature const& f1, Feature const& f2, Label l1, Label l2) const
    {
        assert(l1 < numClasses() && l2 < numClasses());

        auto const& w = m_pWeights->pairwise(l1, l2);
        auto const& wHead = w.head(f1.size());
        auto const& wTail = w.segment(f1.size(), f2.size());
        auto const& bias = w(w.size() - 1);
        return wHead.dot(f1) + wTail.dot(f2) + bias;
    }

    /**
     * Computes the cost of a pairwise connection between a pixel and a cluster
     * @param f1 Pixel feature
     * @param f2 Cluster feature
     * @param l1 Pixel label
     * @param l2 Cluster label
     * @return The cost
     */
    inline Cost higherOrderCost(Feature const& f1, Feature const& f2, Label l1, Label l2) const
    {
        assert(l1 < numClasses() && l2 < numClasses());

        auto const& w = m_pWeights->higherOrder(l1, l2);
        auto const& wHead = w.head(f1.size());
        auto const& wTail = w.segment(f1.size(), f2.size());
        auto const& bias = w(w.size() - 1);
        return wHead.dot(f1) + wTail.dot(f2) + bias;
    }

    /**
     * Computes the cost of clustering two features together
     * @param f1 First feature
     * @param f2 Second feature
     * @return The cost
     */
    inline Cost featureCost(Feature const& f1, Feature const& f2) const
    {
        auto diff = f1 - f2;
        return diff.transpose() * m_pWeights->featureSimMat() * diff;
    }

    /**
     * @return The weights
     */
    inline Weights const& weights() const
    {
        return *m_pWeights;
    }

protected:
    Weights const* m_pWeights;
    ClusterId m_numClusters;
};

#endif //HSEG_ENERGYFUNCTION_H
