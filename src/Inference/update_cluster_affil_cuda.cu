
#include "Inference/update_cluster_affil_cuda.cuh"
#include "helper/cuda_helper.cuh"

template<typename EnergyFun>
UpdateClusterAffilCuda<EnergyFun>::UpdateClusterAffilCuda(EnergyFun const& energyFun, FeatureImage const* features)
{
    // Store weights in a cuda-friendly way
    thrust::host_vector<float> ho_weights_host;
    ho_weights_host.reserve((features->dim() + 1) * energyFun.numClasses() * energyFun.numClasses());
    for(Label l1 = 0; l1 < energyFun.numClasses(); ++l1)
    {
        for(Label l2 = 0; l2 < energyFun.numClasses(); ++l2)
        {
            auto const& w = energyFun.weights.higherOrder(l1, l2);
            ho_weights_host.insert(ho_weights_host.end(), w.data(), w.data() + w.size());
        }
    }
    thrust::host_vector<float> feat_weights_host;
    feat_weights_host.reserve(features->dim() * features->dim());
    feat_weights_host.insert(feat_weights_host.end(), energyFun.weights.featureSimMat().data(), energyFun.weights.featureSimMat().data() + energyFun.weights.featureSimMat().size());

    // Store features in a cuda-friendly way
    thrust::host_vector<float> features_host;
    features_host.reserve(features->dim() * features->width() * features->height());
    for(SiteId i = 0; i < features->width() * features->height(); ++i)
    {
        auto const& f = features->atSite(i);
        features_host.insert(features_host.end(), f.data(), f.data() + f.size());
    }

    // Copy to device
    m_features_device = features_host;
    m_ho_weights_device = ho_weights_host;
    m_feat_weights_device = feat_weights_host;

    m_numLabels = energyFun.numClasses();
    m_feat_dim = features->dim();
}

__global__
void computeBestAffiliationKernel(float const* cost, Label* outClustering, SiteId numSites, ClusterId numClusters)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    if(i < numSites)
    {
        Label* out = outClustering + i;
        float const* costBase = cost + i * numClusters;
        Label l = 0;
        float minCost = *costBase;
        for(ClusterId k = 1; k < numClusters; ++k)
        {
            float c = *(costBase + k);
            if(c < minCost)
            {
                minCost = c;
                l = k;
            }
        }
        *out = l;
    }
}

template<typename EnergyFun>
void UpdateClusterAffilCuda<EnergyFun>::updateClusterAffiliation(LabelImage& outClustering, LabelImage const& labeling,
                                                                 std::vector<Cluster> const& clusters)
{
    SiteId numSites = labeling.pixels();
    ClusterId numClusters = clusters.size();
    uint32_t featureDim = clusters.front().m_feature.size();

    // Prepare cluster features and labels
    thrust::host_vector<float> cluster_features_host;
    thrust::host_vector<Label> cluster_labels_host;
    cluster_features_host.reserve(featureDim * numClusters);
    cluster_labels_host.reserve(numClusters);
    for(auto const& c : clusters)
    {
        auto const& f = c.m_feature;
        cluster_features_host.insert(cluster_features_host.end(), f.data(), f.data() + f.size());
        cluster_labels_host.push_back(c.m_label);
    }

    // Copy data to device
    thrust::device_vector<Label> clustering_device;
    thrust::device_vector<Label> labeling_device;
    thrust::device_vector<float> cluster_features_device;
    thrust::device_vector<Label> cluster_labels_device;
    thrust::device_vector<float> cost_device(numSites * numClusters, 0);
    clustering_device.resize(numSites, 0);
    labeling_device = labeling.data();
    cluster_features_device = cluster_features_host;
    cluster_labels_device = cluster_labels_host;

    // Compute cost for every site-cluster combination
    dim3 threadsPerBlock(32);
    dim3 numBlocks(std::ceil(numSites / (float)threadsPerBlock.x));
    for(SiteId i = 0; i < numSites; ++i)
    {
        Label l_i = labeling.atSite(i);
        for(ClusterId k = 0; k < numClusters; ++k)
        {
            Label l_k = clusters[k].m_label;

            float* out_dev = cost_device.data().get() + (k + i * numClusters);
            float* f_site_dev = m_features_device.data().get() + (i * m_feat_dim);
            float* f_clu_dev = cluster_features_device.data().get() + (k * m_feat_dim);
            float* w_ho_dev = m_ho_weights_device.data().get() + (l_i + l_k * m_numLabels) * (m_feat_dim * 2 + 1);
            float* w_ho_2_dev = w_ho_dev + m_feat_dim;
            float* w_ho_bias_dev = w_ho_dev + m_feat_dim + 1;
            float* w_feat_dev = m_feat_weights_device.data().get();

            // Higher-order cost
            helper::cuda::innerProductKernel<float, true><<<numBlocks, threadsPerBlock>>>(out_dev, f_site_dev, w_ho_dev, m_feat_dim);
            helper::cuda::innerProductKernel<float, false><<<numBlocks, threadsPerBlock>>>(out_dev, f_clu_dev, w_ho_2_dev, m_feat_dim);
            helper::cuda::sumKernel<<<1, 1>>>(out_dev, out_dev, w_ho_bias_dev, 1);

            // Feature cost
            helper::cuda::mahalanobisDistance<float, false>(out_dev, f_site_dev, f_clu_dev, w_feat_dev, m_feat_dim);
        }
    }

    // Compute minimum at each site
    dim3 threadsPerBlock2(64);
    dim3 numBlocks2(std::ceil(numSites / (float)threadsPerBlock2.x));
    computeBestAffiliationKernel<<<numBlocks2, threadsPerBlock2>>>(cost_device.data().get(),
                                                                   clustering_device.data().get(),
                                                                   numSites, numClusters);

    // Copy result to host
    thrust::host_vector<Label> clustering = clustering_device;
    outClustering.data().clear();
    outClustering.data().resize(numSites);
    outClustering.data().insert(outClustering.data().end(), clustering.data(), clustering.data() + clustering.size());
}

