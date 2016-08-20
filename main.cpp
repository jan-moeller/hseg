#include <iostream>
#include <UnaryFile.h>
#include <k-prototypes/Clusterer.h>
#include <helper/image_helper.h>
#include "GCoptimization.h"

int main()
{
    std::string filename = "2007_000129"; //"2007_000032";

    UnaryFile unary("data/" + filename + "_prob.dat");

    RGBImage rgb;
    rgb.read("data/" + filename  + ".jpg");
    CieLabImage cieLab = rgb.getCieLabImg();
    LabelImage maxLabeling = unary.maxLabeling();

    size_t numClusters = 100;
    Clusterer clusterer;
    clusterer.run(numClusters, unary.classes(), cieLab, maxLabeling);

    LabelImage const& spLabeling = clusterer.clustership();

    helper::image::ColorMap cmap = helper::image::generateColorMapVOC(std::max<int>(unary.classes(), numClusters));

    cv::Mat rgbMat = static_cast<cv::Mat>(rgb);
    cv::Mat labelMat = static_cast<cv::Mat>(helper::image::colorize(maxLabeling, cmap));
    cv::Mat spLabelMat = static_cast<cv::Mat>(helper::image::colorize(spLabeling, cmap));

    cv::imshow("max labeling", labelMat);
    cv::imshow("rgb", rgbMat);
    cv::imshow("sp", spLabelMat);
    cv::waitKey();

    /*GCoptimizationGridGraph* gc = new GCoptimizationGridGraph(unary.height(), unary.width(), unary.classes());
    for (int x = 0; x < unary.width(); ++x)
        for (int y = 0; y < unary.height(); ++y)
            for (int l = 0; l < unary.classes(); ++l)
                gc->setDataCost(x + y * unary.width(), l, -unary.at(x, y, l));
    gc->expansion();

    cv::Mat result(unary.height(), unary.width(), CV_8UC1);
    for (int x = 0; x < unary.width(); ++x)
        for (int y = 0; y < unary.height(); ++y)
            result.at<unsigned char>(y, x) = gc->whatLabel(x + y * unary.width());

    delete gc;*/

    /*cv::Mat result(unary.height(), unary.width(), CV_8UC1);
    for (int x = 0; x < unary.width(); ++x)
        for (int y = 0; y < unary.height(); ++y)
            result.at<unsigned char>(y, x) = unary.maxLabelAt(x, y);*/

    /*cv::equalizeHist( result, result );

    cv::imshow("result", result);
    cv::waitKey();*/

    return 0;
}