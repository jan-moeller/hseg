//
// Created by jan on 29.08.16.
//

#include <BaseProperties.h>
#include <Energy/WeightsVec.h>
#include <Energy/UnaryFile.h>
#include <Energy/LossAugmentedEnergyFunction.h>
#include <helper/image_helper.h>
#include <Inference/InferenceIterator.h>
#include <Inference/k-prototypes/Clusterer.h>

PROPERTIES_DEFINE(Train,
                  PROP_DEFINE(size_t, numClusters, 300)
                  PROP_DEFINE(size_t, numIter, 100)
                  PROP_DEFINE(float, learningRate, 1.f)
                  PROP_DEFINE(float, C, 1.f)
                  PROP_DEFINE(float, pairwiseSigmaSq, 0.05f)
                  PROP_DEFINE(std::string, imageListFile, "")
                  PROP_DEFINE(std::string, groundTruthListFile, "")
                  PROP_DEFINE(std::string, unaryListFile, "")
                  PROP_DEFINE(std::string, imageBasePath, "")
                  PROP_DEFINE(std::string, groundTruthBasePath, "")
                  PROP_DEFINE(std::string, unaryBasePath, "")
                  PROP_DEFINE(std::string, imageExtension, ".jpg")
                  PROP_DEFINE(std::string, gtExtension, ".png")
                  PROP_DEFINE(std::string, out, "out/weights.dat")
)

std::vector<std::string> readFileNames(std::string const& listFile)
{
    std::vector<std::string> list;
    std::ifstream in(listFile, std::ios::in);
    if (in.is_open())
    {
        std::string line;
        while (std::getline(in, line))
            list.push_back(line);
        in.close();
    }
    return list;
}

int main()
{
    // Read properties
    TrainProperties properties;
    properties.read("properties/training.info");
    std::cout << "----------------------------------------------------------------" << std::endl;
    std::cout << "Used properties: " << std::endl;
    std::cout << properties << std::endl;
    std::cout << "----------------------------------------------------------------" << std::endl;

    size_t const numClasses = 21;
    size_t const numClusters = properties.numClusters;
    helper::image::ColorMap const cmap = helper::image::generateColorMapVOC(std::max(256ul, numClasses));
    helper::image::ColorMap const cmap2 = helper::image::generateColorMap(properties.numClusters);
    WeightsVec curWeights(numClasses, 1, 0, 0, 0, 0, 0, 0); // Start with the result from the unary only
    WeightsVec oneWeights(numClasses, 1, 1, 1, 1, 1, 1, 1);

    // Load filenames of all images
    std::vector<std::string> colorImageFilenames = readFileNames(properties.imageListFile);
    std::vector<std::string> gtImageFilenames = readFileNames(properties.groundTruthListFile);
    std::vector<std::string> unaryFilenames = readFileNames(properties.unaryListFile);
    if (colorImageFilenames.size() != gtImageFilenames.size() || gtImageFilenames.size() != unaryFilenames.size())
    {
        std::cerr << "File lists don't match up!" << std::endl;
        return -1;
    }
    std::vector<size_t> indices(colorImageFilenames.size());
    std::iota(indices.begin(), indices.end(), 0);
    size_t T = properties.numIter;
    size_t N = colorImageFilenames.size();

    // Iterate T times
    for(size_t t = 0; t < T; ++t)
    {
        std::shuffle(indices.begin(), indices.end(), std::default_random_engine());
        // Iterate over all images
        for(size_t n = 0; n < N; ++n)
        {
            auto colorImgFilename = colorImageFilenames[indices[n]];
            auto gtImageFilename = gtImageFilenames[indices[n]];
            auto unaryFilename = unaryFilenames[indices[n]];

            // Load images etc...
            RGBImage rgbImage, groundTruthRGB, groundTruthSpRGB;
            rgbImage.read(properties.imageBasePath + colorImgFilename + properties.imageExtension);
            groundTruthRGB.read(properties.groundTruthBasePath + gtImageFilename + properties.gtExtension);
            if (rgbImage.width() != groundTruthRGB.width() || rgbImage.height() != groundTruthRGB.height())
            {
                std::cerr << "Image " << colorImageFilenames[n] << " and its ground truth don't match." << std::endl;
                continue;
            }
            CieLabImage cieLabImage = rgbImage.getCieLabImg();
            LabelImage groundTruth = helper::image::decolorize(groundTruthRGB, cmap);

            UnaryFile unary(properties.unaryBasePath + unaryFilename + "_prob.dat");
            if(unary.width() != rgbImage.width() || unary.height() != rgbImage.height() || unary.classes() != numClasses)
            {
                std::cerr << "Invalid unary scores " << unaryFilenames[n] << std::endl;
                continue;
            }

            // Predict with loss-augmented energy
            LossAugmentedEnergyFunction energy(unary, curWeights, properties.pairwiseSigmaSq, groundTruth);
            InferenceIterator inference(energy, properties.numClusters, numClasses, cieLabImage);
            InferenceResult result = inference.run(2);

            // Compute energy without weights on the ground truth
            EnergyFunction normalEnergy(unary, oneWeights, properties.pairwiseSigmaSq);
            EnergyFunction curWEnergy(unary, curWeights, properties.pairwiseSigmaSq);
            Clusterer clusterer(curWEnergy);
            clusterer.run(numClusters, numClasses, cieLabImage, groundTruth);
            auto gtEnergy = normalEnergy.giveEnergyByWeight(groundTruth, cieLabImage, clusterer.clustership(), clusterer.clusters());

            // Compute energy without weights on the prediction
            auto predEnergy = normalEnergy.giveEnergyByWeight(result.labeling, cieLabImage, result.superpixels, result.clusterer.clusters());

            std::cout << "<<< " << t << "/" << n << " >>>" << std::endl;

            // Update step
            gtEnergy -= predEnergy;
            gtEnergy *= properties.C / N;
            gtEnergy += curWeights;
            gtEnergy *= properties.learningRate / (t + n + 1);
            curWeights -= gtEnergy;

            if(!curWeights.write(properties.out))
                std::cerr << "Couldn't write weights to file " << properties.out << std::endl;
            std::cout << curWeights << std::endl;
        }
    }

    return 0;
}