#ifndef FDKAAC_DECODER_H
#define FDKAAC_DECODER_H

#include <Windows.h>
#include <iostream>

#include "fdk-aac/libAACdec/include/aacdecoder_lib.h"
#include "fdk-aac/libSYS/include/FDK_audio.h"

#define DUMP_AUDIO

namespace {
	const std::string outputPCM = "AAC-EldOutput.pcm";
}

namespace owt {
namespace audio {

class FDKAACDecoder {
public:
	FDKAACDecoder() :decoder(nullptr){
#ifdef DUMP_AUDIO
		if (file_lpcm == nullptr) {
			fopen_s(&file_lpcm, outputPCM.c_str(), "wb+");
		}
#endif
	};
	virtual ~FDKAACDecoder();
	AAC_DECODER_ERROR init(void);
	AAC_DECODER_ERROR enqueue(void* buffer, DWORD buffer_size, uint64_t aTimestamp);
	AAC_DECODER_ERROR decode();

private:
	HANDLE_AACDECODER decoder;
#ifdef DUMP_AUDIO
	FILE* file_lpcm = nullptr;
#endif
};

}
}
#endif