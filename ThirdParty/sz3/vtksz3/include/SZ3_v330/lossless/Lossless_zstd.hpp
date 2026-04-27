//
// Created by Kai Zhao on 4/21/20.
//

#ifndef SZ3_V330_LOSSLESS_ZSTD_HPP
#define SZ3_V330_LOSSLESS_ZSTD_HPP

#include <stdexcept>

#include "SZ3_v330/def.hpp"
#include "SZ3_v330/lossless/Lossless.hpp"
#include "SZ3_v330/utils/MemoryUtil.hpp"
#include "zstd.h"

namespace SZ3_v330 {
class Lossless_zstd : public concepts::LosslessInterface {
   public:
    Lossless_zstd() = default;

    Lossless_zstd(int comp_level) : compression_level(comp_level) {}

    /**
     * Attention
     * When dstCap is smaller than the space needed, ZSTD will not throw any errors.
     * Instead, it will write a portion of the compressed data to dst and stops.
     * This behavior is not desirable in SZ, as we need the whole compressed data for decompression.
     * Therefore, we need to check if the dst buffer (dstCap) is large enough for zstd
     */
    size_t compress(const uchar *src, size_t srcLen, uchar *dst, size_t dstCap) override {
        write(srcLen, dst);
        dstCap -= sizeof(size_t);  // reserve space for srcLen
        if (dstCap < ZSTD_compressBound(srcLen)) {
            throw std::length_error(SZ3_V330_ERROR_COMP_BUFFER_NOT_LARGE_ENOUGH);
        }
        size_t dstLen = ZSTD_compress(dst, dstCap, src, srcLen, compression_level);
        return dstLen + sizeof(size_t);
    }

    size_t decompress(const uchar *src, const size_t srcLen, uchar *&dst, size_t &dstLen) override {
        read(dstLen, src);
        if (dst == nullptr) {
            dst = static_cast<uchar *>(malloc(dstLen));
        }
        return ZSTD_decompress(dst, dstLen, src, srcLen - sizeof(dstLen));
    }

   private:
    int compression_level = 3;  // default setting of level is 3
};
}  // namespace SZ3_v330
#endif  // SZ_LOSSLESS_ZSTD_HPP
