// simple-sgm.cpp : Defines the entry point for the console application.

#include "utils.h"
#include <iostream>
#include <sgm/sgm.h>

namespace
{
static constexpr int S_OK = 0;
static constexpr int S_FAIL = -1;

auto static constexpr DMin = 0;
auto static constexpr DMax = 64;

}  // namespace

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        std::cout << std::endl;
        std::cout << "Computes a disparity map for the input left and right images" << std::endl;
        std::cout << "The output image is saved as png" << std::endl;
        std::cout << "Usage: simple-sgm <left-image-path> <right-image-path> <output-image-path>" << std::endl;
        std::cout << std::endl;
        return S_OK;
    }

    try
    {
        auto LeftImage = utils::io::readImage(argv[1]);
        auto RightImage = utils::io::readImage(argv[2]);

        if (LeftImage != RightImage)
        {
            throw std::runtime_error("Images must have the same dimension");
        }

        sgm::SemiGlobalMatching<DMax, DMin, true> Sgm(std::move(LeftImage), std::move(RightImage));
        Sgm.SetPenalities(10, 80);
        const auto DMap = Sgm.GetDisparity();

        utils::io::saveImage(argv[3], DMap);

        return S_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return S_FAIL;
    }
}
