#ifndef AAC_DECODER_MF_H
#define AAC_DECODER_MF_H

#include <Windows.h>
#include <codecapi.h>
#include <mfapi.h>
#include <Mferror.h>
#include <mfidl.h>
#include <mftransform.h>
#include <ppltasks.h>
#include <robuffer.h>
#include <iomanip>
#include <wmcodecdsp.h>

#define DUMP_AUDIO

#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)

namespace owt {
namespace audio {

class AACDecoderMFImpl;
class MFAACDecoder {
public:
	static std::unique_ptr<AACDecoderMFImpl> Create() {
		return std::make_unique<AACDecoderMFImpl>();
	}
	static bool IsSupported() {
#ifdef WEBRTC_WIN
		return true;
#else
		return false;
#endif
	};
	virtual ~MFAACDecoder() = default;
};

class AACDecoderMFImpl {
public:
	AACDecoderMFImpl():dwFlags(0), unChannel(0), unSampling(0){
		memset(&mInputStreamInfo, 0, sizeof(MFT_INPUT_STREAM_INFO));
		memset(&mOutputStreamInfo, 0, sizeof(MFT_OUTPUT_STREAM_INFO));
#ifdef DUMP_AUDIO
		if (file_lpcm == NULL) {
			fopen_s(&file_lpcm, "output.wav", "w+");
		}
#endif
	};
	virtual ~AACDecoderMFImpl() {
		if (pDecoder != NULL) {
			// Follow shutdown procedure gracefully. On fail, continue anyway.
			SUCCEEDED(pDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0));
			SUCCEEDED(pDecoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL));
			SUCCEEDED(pDecoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL));
			pDecoder = nullptr;
		}
#ifdef DUMP_AUDIO
		if (file_lpcm != NULL) {
			fclose(file_lpcm);
			file_lpcm = NULL;
		}
#endif
	};
	HRESULT init(void);
	HRESULT enqueue(void* buffer, DWORD buffer_size, int64_t aTimestamp);
	HRESULT decode();

private:
	ULONG dwFlags;
	IMFTransform* pDecoder = nullptr;
	IMFMediaType* pMediaTypeIn = nullptr;
	IMFMediaType* pOutType = nullptr;
	IMFMediaType* partialMediaType = nullptr;
	unsigned int unChannel, unSampling, unBit;
#ifdef DUMP_AUDIO
	FILE* file_lpcm = NULL;
#endif
	MFT_INPUT_STREAM_INFO mInputStreamInfo{};
	MFT_OUTPUT_STREAM_INFO mOutputStreamInfo{};
};

}
}

#endif
