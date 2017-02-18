#include <algorithm>
#include <cfloat>
#include <vector>

#include "caffe/layers/ssvm_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

#include "Image/FeatureImage.h"
#include <Energy/EnergyFunction.h>
#include <Inference/InferenceIterator.h>
#include <Energy/LossAugmentedEnergyFunction.h>

namespace caffe {

    template <typename Dtype>
    void SSVMLossLayer<Dtype>::LayerSetUp(
            const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {

        bool ok = weights_.read("weights.dat");
        CHECK(ok) << "Couldn't read \"weights.dat\".";
    }

    template <typename Dtype>
    void SSVMLossLayer<Dtype>::Reshape(vector<Blob<Dtype>*> const& bottom, vector<Blob<Dtype>*> const& top)
    {
        LossLayer<Dtype>::Reshape(bottom, top);
    }

    template <typename Dtype>
    void SSVMLossLayer<Dtype>::Forward_cpu(
            const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {

        // Copy over the features
        features_ = FeatureImage(bottom[0]->width(), bottom[0]->height(), bottom[0]->channels());
        for(Coord x = 0; x < bottom[0]->width(); ++x)
        {
            for(Coord y = 0; y < bottom[0]->height(); ++y)
            {
                for(Coord c = 0; c < features_.dim(); ++c)
                    features_.at(x, y)[c] = bottom[0]->data_at(0, c, y, x);
            }
        }

        // Copy over label image
        gt_ = LabelImage(bottom[1]->width(), bottom[1]->height());
        for(Coord x = 0; x < bottom[1]->width(); ++x)
        {
            for(Coord y = 0; y < bottom[1]->height(); ++y)
            {
                // Round because of float imprecision
                gt_.at(x, y) = std::round(bottom[1]->data_at(0, 0, y, x));
            }
        }
        gt_.rescale(features_.width(), features_.height(), false);

        // Find latent variables that best explain the ground truth
        EnergyFunction energy(&weights_, numClusters_);
        InferenceIterator<EnergyFunction> gtInference(&energy, &features_, eps_, maxIter_);
        gtResult_ = gtInference.runOnGroundTruth(gt_);

        // Predict with loss-augmented energy
        LossAugmentedEnergyFunction lossEnergy(&weights_, &gt_, numClusters_);
        InferenceIterator<LossAugmentedEnergyFunction> inference(&lossEnergy, &features_, eps_, maxIter_);
        predResult_ = inference.run();

        // Compute energy without weights on the ground truth
        auto gtEnergy = energy.giveEnergyByWeight(features_, gt_, gtResult_.clustering, gtResult_.clusters);
        // Compute energy without weights on the prediction
        auto predEnergy = energy.giveEnergyByWeight(features_, predResult_.labeling, predResult_.clustering, predResult_.clusters);

        // Compute upper bound on this image
        auto gtEnergyCur = weights_ * gtEnergy;
        auto predEnergyCur = weights_ * predEnergy;
        float lossFactor = LossAugmentedEnergyFunction::computeLossFactor(gt_, numClasses_);
        float loss = LossAugmentedEnergyFunction::computeLoss(predResult_.labeling, predResult_.clustering, gt_, predResult_.clusters,
                                                              lossFactor, numClasses_);
        float sampleLoss = (loss - predEnergyCur) + gtEnergyCur;

        top[0]->mutable_cpu_data()[0] = sampleLoss;
    }

    template <typename Dtype>
    void SSVMLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {

        if (propagate_down[1]) {
            LOG(FATAL) << this->type()
                       << " Layer cannot backpropagate to label inputs.";
        }
        if (propagate_down[0]) {

            EnergyFunction energy(&weights_, numClusters_);
            FeatureImage gradGt(bottom[0]->width(), bottom[0]->height(), bottom[0]->channels());
            energy.computeFeatureGradient(gradGt, gtResult_.labeling, gtResult_.clustering, gtResult_.clusters, features_);
            FeatureImage gradPred(bottom[0]->width(), bottom[0]->height(), bottom[0]->channels());
            energy.computeFeatureGradient(gradPred, predResult_.labeling, predResult_.clustering, predResult_.clusters, features_);

            // Compute gradient
            gradGt.subtract(gradPred);

            // Write back
            for(Coord x = 0; x < features_.width(); ++x)
            {
                for(Coord y = 0; y < features_.height(); ++y)
                {
                    Label l = gt_.at(x, y);
                    for(Coord c = 0; c < features_.dim(); ++c)
                    {
                        if(l < numClasses_)
                            *(bottom[0]->mutable_cpu_diff_at(0, c, y, x)) = gradGt.at(x, y)[c];
                        else
                            *(bottom[0]->mutable_cpu_diff_at(0, c, y, x)) = 0;
                    }
                }
            }
        }
    }


#ifdef CPU_ONLY
//    STUB_GPU(SSVMLossLayer);
#endif

    INSTANTIATE_CLASS(SSVMLossLayer);
    REGISTER_LAYER_CLASS(SSVMLoss);
}