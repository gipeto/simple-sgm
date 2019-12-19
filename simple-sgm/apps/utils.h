#pragma once

#include <sgm/sgm_utils.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <stdexcept>
#include <string>

namespace utils
{
namespace instructionset
{
void run_cpuid(int eax, int ecx, int* cpuInfo)
{
#if defined(_MSC_VER)
    __cpuidex(cpuInfo, eax, ecx);

#else
    int ebx = 0;
    int edx = 0;

#if defined(__i386__) && defined(__PIC__)

    /* in case of PIC under 32-bit EBX cannot be clobbered */
    __asm__("movl %%ebx, %%edi \n\t cpuid \n\t xchgl %%ebx, %%edi"
            : "=D"(ebx),
#else
    __asm__("cpuid"
            : "+b"(ebx),
#endif
              "+a"(eax), "+c"(ecx), "=d"(edx));
    cpuInfo[0] = eax;
    cpuInfo[1] = ebx;
    cpuInfo[2] = ecx;
    cpuInfo[3] = edx;
#endif
}

bool avx2_supported()
{
    const int AVX_2_SUPPORTED = (1 << 5) | (1 << 3) | (1 << 8);
    int abcd[4];
    run_cpuid(7, 0, abcd);
    return (abcd[1] & AVX_2_SUPPORTED) == AVX_2_SUPPORTED;
}
}  // namespace instructionset

namespace io
{
sgm::SimpleImage readImage(std::string filename)
{
    int width{};
    int height{};
    int channels{};
    const int desiredChannels = 1;
    auto* pixelData = stbi_load(filename.c_str(), &width, &height, &channels, desiredChannels);

    if (0 == pixelData)
    {
        throw std::runtime_error("Failed to load " + filename);
    }

    sgm::SimpleImage image{sgm::make_unique_aligned<uint8_t>(static_cast<size_t>(width * height)),
                           static_cast<size_t>(width), static_cast<size_t>(height)};

    std::memcmp(image.Buffer.get(), pixelData, static_cast<size_t>(width * height));
    stbi_image_free(pixelData);

    return image;
}

void saveImage(std::string filename, const sgm::SimpleImage& image)
{
    if (0
        == stbi_write_png(filename.c_str(), static_cast<int>(image.Width), static_cast<int>(image.Height), 1,
                          image.Buffer.get(), static_cast<int>(image.Width)))
    {
        throw std::runtime_error("Failed to write " + filename);
    }
}

}  // namespace io

}  // namespace utils