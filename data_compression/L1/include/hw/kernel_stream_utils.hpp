/*
 * (c) Copyright 2019 Xilinx, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _XFCOMPRESSION_KERNEL_STREAM_UTILS_HPP_
#define _XFCOMPRESSION_KERNEL_STREAM_UTILS_HPP_

#include "hls_stream.h"

#include "ap_axi_sdata.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <ap_int.h>

namespace xf {
namespace compression {
namespace details {

template <uint32_t DATAWIDTH>
void kStreamRead(hls::stream<ap_axiu<DATAWIDTH, 0, 0, 0> >& inKStream,
                 hls::stream<ap_uint<DATAWIDTH> >& readStream,
                 uint32_t input_size) {
    /**
     * @brief Read N-bit wide data from internal streams output by compression modules
     *        and write to output axi stream. N is the template parameter DATAWIDTH.
     *
     * @tparam DATAWIDTH    data width of the kernel stream
     *
     * @param inKStream     input axi kernel stream
     * @param readStream    internal hls stream to be read for processing
     *
     */
    ap_axiu<DATAWIDTH, 0, 0, 0> tmp;
    uint8_t factor = DATAWIDTH / 8;
    uint32_t sCnt = 1 + ((input_size - 1) / factor);

    for (int i = 0; i < sCnt; i++) {
#pragma HLS PIPELINE II = 1
        tmp = inKStream.read();
        readStream << tmp.data;
    }
}

template <uint32_t DATAWIDTH>
void kStreamDataRead(hls::stream<ap_axiu<DATAWIDTH, 0, 0, 0> >& inKStream,
                     hls::stream<ap_axiu<32, 0, 0, 0> >& inKStreamSize,
                     hls::stream<ap_uint<DATAWIDTH> >& readStream,
                     hls::stream<uint32_t>& readStreamSize) {
    /**
     * @brief Read N-bit wide data from internal streams output by compression modules
     *        and write to output axi stream. N is the template parameter DATAWIDTH.
     *
     * @tparam DATAWIDTH    data width of the kernel stream
     *
     * @param inKStream     input axi kernel stream
     * @param readStream    internal hls stream to be read for processing
     *
     */
    ap_axiu<32, 0, 0, 0> tmpSize;
    for (tmpSize = inKStreamSize.read(); tmpSize.data != 0; tmpSize = inKStreamSize.read()) {
        readStreamSize << tmpSize.data;
        uint32_t input_size = tmpSize.data;
        ap_axiu<DATAWIDTH, 0, 0, 0> tmp;
        uint8_t factor = DATAWIDTH / 8;
        uint32_t sCnt = 1 + ((input_size - 1) / factor);
        for (int i = 0; i < sCnt; i++) {
#pragma HLS PIPELINE II = 1
            tmp = inKStream.read();
            readStream << tmp.data;
        }
    }
    readStreamSize << 0;
}

template <uint32_t DATAWIDTH>
void kStreamWrite(hls::stream<ap_axiu<DATAWIDTH, 0, 0, 0> >& outKStream,
                  hls::stream<ap_uint<DATAWIDTH> >& outDataStream,
                  hls::stream<bool>& byteEos,
                  hls::stream<uint32_t>& dataSize) {
    /**
     * @brief Read N-bit wide data from internal hls streams and write to axi
     *        kernel stream till the end of stream. N is the template parameter DATAWIDTH.
     *
     * @tparam DATAWIDTH    data width of the kernel stream
     *
     * @param outKStream    output kernel stream
     * @param outDataStream output data stream from internal modules
     * @param byteEos       internal stream which indicates end of data stream
     * @param dataSize      size of data in streams
     *
     */
    uint32_t outSize = 0;
    bool lastByte = false;
    ap_uint<DATAWIDTH> tmp;
    ap_axiu<DATAWIDTH, 0, 0, 0> t1;

    do {
#pragma HLS PIPELINE II = 1
        tmp = outDataStream.read();
        lastByte = byteEos.read();
        t1.data = tmp;
        t1.last = lastByte;
        outKStream.write(t1);
    } while (!lastByte);
    outSize = dataSize.read();
}

template <uint32_t DATAWIDTH>
void kStreamWriteFixedSize(hls::stream<ap_axiu<DATAWIDTH, 0, 0, 0> >& outKStream,
                           hls::stream<ap_uint<DATAWIDTH> >& outDataStream,
                           uint32_t outputSize) {
    /**
     * @brief Read N-bit wide data from internal hls streams and write to axi
     *        kernel stream for the given size. N is the template parameter DATAWIDTH.
     *
     * @tparam DATAWIDTH    data width of the kernel stream
     *
     * @param outKStream    output kernel stream
     * @param outDataStream output data stream from internal modules
     * @param dataSize      size of data in streams
     *
     */
    uint32_t outSize = 0;
    ap_uint<DATAWIDTH> tmp;
    ap_axiu<DATAWIDTH, 0, 0, 0> t1;

    uint8_t factor = DATAWIDTH / 8;
    uint32_t sCnt = 1 + ((outputSize - 1) / factor);

    for (uint32_t i = 0; i < sCnt - 1; i++) {
#pragma HLS PIPELINE II = 1
        tmp = outDataStream.read();
        t1.data = tmp;
        t1.last = false;
        outKStream.write(t1);
    }
    // last data packet
    tmp = outDataStream.read();
    t1.data = tmp;
    t1.last = true;
    outKStream.write(t1);
}

template <uint32_t DATAWIDTH>
void kStreamWriteMultiByteSize(hls::stream<ap_axiu<DATAWIDTH, 0, 0, 0> >& outKStream,
                               hls::stream<ap_axiu<32, 0, 0, 0> >& outKSizeStream,
                               hls::stream<ap_uint<DATAWIDTH> >& inDataStream,
                               hls::stream<bool>& inDataStreamEoS,
                               hls::stream<uint32_t>& inSizeStream) {
    /**
     * @brief Read N-bit wide data from internal hls streams and write to axi
     *        kernel stream for the given size. N is the template parameter DATAWIDTH.
     *
     * @tparam DATAWIDTH    data width of the kernel stream
     *
     * @param outKStream    output kernel stream
     * @param outDataStream output data stream from internal modules
     * @param dataSize      size of data in streams
     *
     */
    // read the original size
    ap_uint<DATAWIDTH> inValue;
    ap_axiu<DATAWIDTH, 0, 0, 0> outValue;
    ap_axiu<32, 0, 0, 0> decompressSize;

    bool eosFlag = inDataStreamEoS.read();

    for (; eosFlag == false; eosFlag = inDataStreamEoS.read()) {
#pragma HLS PIPELINE II = 1
        inValue = inDataStream.read();
        outValue.data = inValue;
        outValue.last = false;
        outKStream.write(outValue);
    }

    inValue = inDataStream.read();
    outValue.data = inValue;
    outValue.last = true;
    outKStream.write(outValue);

    decompressSize.data = inSizeStream.read();
    decompressSize.last = true;
    outKSizeStream.write(decompressSize);
}

} // end details
} // end compression
} // end xf

#endif // _XFCOMPRESSION_KERNEL_STREAM_UTILS_HPP_
