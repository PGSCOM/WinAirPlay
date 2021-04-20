/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

/* 
 * AAC renderer using fdk-aac for decoding and OpenMAX for rendering
*/

#include "fdkaac_decoder.h"

namespace {
const INT time_data_size = 4 * 480;
unsigned char eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
}

namespace owt {
namespace audio {

FDKAACDecoder::~FDKAACDecoder() {
    if (decoder != nullptr) {
        aacDecoder_Close(decoder);
        decoder = nullptr;
    }

#ifdef DUMP_AUDIO
    if (file_lpcm != nullptr) {
        fclose(file_lpcm);
        file_lpcm = nullptr;
    }
#endif
}

AAC_DECODER_ERROR FDKAACDecoder::init() {
    decoder = aacDecoder_Open(TT_MP4_RAW, 1);
    if (decoder == nullptr) {
        return AAC_DEC_INVALID_HANDLE;
    }
    /* ASC config binary data */
    unsigned char*conf[] = { eld_conf };
    static unsigned int conf_len = sizeof(eld_conf);
    AAC_DECODER_ERROR ret = aacDecoder_ConfigRaw(decoder, conf, &conf_len);
    if (ret != AAC_DEC_OK) {
        return ret;
    }

    return AAC_DEC_OK;
}

AAC_DECODER_ERROR FDKAACDecoder::enqueue(void* buffer, DWORD buffer_size, uint64_t aTimestamp) {
    if (buffer_size == 0) {
        return AAC_DEC_INPUT_DATA_SIZE_NOT_ENOUGH;
    }

    AAC_DECODER_ERROR error = AAC_DEC_OK;

    UCHAR* p_buffer[1] = {static_cast<UCHAR*>(buffer)};
    UINT bytes_valid = buffer_size;
    error = aacDecoder_Fill(decoder, p_buffer, reinterpret_cast<const UINT*>(&buffer_size), &bytes_valid);
    if (error != AAC_DEC_OK) {
        return error;
    }
}

AAC_DECODER_ERROR FDKAACDecoder::decode() {
    INT_PCM* p_time_data = new INT_PCM[time_data_size]; // The buffer for the decoded AAC frames
    AAC_DECODER_ERROR error = aacDecoder_DecodeFrame(decoder, p_time_data, time_data_size, 0);
    if (error != AAC_DEC_OK) {
        return error;
    }

#ifdef DUMP_AUDIO
    if (file_lpcm != nullptr) {
        fwrite(p_time_data, time_data_size, 1, file_lpcm);
        fflush(file_lpcm);
    }
#endif
    delete[] p_time_data;
    return AAC_DEC_OK;
}

}
}