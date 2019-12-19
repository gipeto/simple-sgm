#include "utils.h"
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

    std::cout << "Left image: " << argv[1] << std::endl;
    std::cout << "Right image: " << argv[2] << std::endl;
    std::cout << "Output image path: " << argv[3] << std::endl;

    try
    {
        auto LeftImage = utils::io::readImage(argv[1]);
        auto RightImage = utils::io::readImage(argv[2]);

        if (LeftImage != RightImage)
        {
            throw std::runtime_error("Images must have the same dimension");
        }

        sgm::SimpleImage DMap;
        if (utils::instructionset::avx2_supported())
        {
            utils::perf::PerformanceTimer timer("AVX2 accelerated sgm");
            sgm::SemiGlobalMatching<DMax, DMin, true> Sgm(std::move(LeftImage), std::move(RightImage));
            Sgm.SetPenalities(10, 80);
            DMap = Sgm.GetDisparity();
        }
        else
        {
            utils::perf::PerformanceTimer timer("Non vectorized sgm");
            sgm::SemiGlobalMatching<DMax, DMin, false> Sgm(std::move(LeftImage), std::move(RightImage));
            Sgm.SetPenalities(10, 80);
            DMap = Sgm.GetDisparity();
        }

        utils::io::saveImage(argv[3], DMap);

        return S_OK;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return S_FAIL;
    }
}
