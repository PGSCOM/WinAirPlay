#include "aac_decoder.h"

#include <Windows.h>
#include <memory>
#include <Mfapi.h>
#include <Mferror.h>
#include <Mfidl.h>
#include <wmcodecdsp.h>
#include <mftransform.h>
#include <Mfreadwrite.h>
#include <codecapi.h>

/*
IMFTransform* pDecoder = 0;
IMFMediaType* pMediaTypeIn = 0;
IMFMediaType* pOutType = 0;
IMFSourceReader* pReader = 0;
IMFMediaType* partialMediaType = 0;
unsigned int unChannel, unSampling;
*/

namespace owt {
namespace audio {
int64_t UsecsToHNs(int64_t aUsecs) {
	return aUsecs * 10;
}

/**
 * Workaround [MF H264 bug: Output status is never set, even when ready]
 *  => For now, always mark "ready" (results in extra buffer alloc/dealloc).
 */
HRESULT GetOutputStatus(IMFTransform* decoder, DWORD* output_status) {
	HRESULT hr = decoder->GetOutputStatus(output_status);

	// Don't MFT trust output status for now.
	*output_status = MFT_OUTPUT_STATUS_SAMPLE_READY;
	return hr;
}

HRESULT AACDecoderMFImpl::init(void) {
	// init MF decoder, using Microsoft hard wired GUID
	HRESULT hrError = S_OK;
	hrError = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	hrError = MFStartup(MF_VERSION, MFSTARTUP_FULL);
	hrError = CoCreateInstance(CLSID_CMSAACDecMFT/*IID_CMSAACDecMFT*/, 0, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void**)&pDecoder);


	// setup decoder input type, hard wired for given sample audio clip

	unChannel = 2;
	unSampling = 44100;
	unBit = 16;

	hrError = MFCreateMediaType(&pMediaTypeIn);
	hrError = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	hrError = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC); //or MFAudioFormat_ADTS

	//hrError = pMediaTypeIn->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0/* 0x2A DP4MEDIA_MP4A_AUDIO_PLI_AAC_L4*/);
	hrError = pMediaTypeIn->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);		// payload 1 = ADTS header, 0 Raw AAC
	//hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, (UINT32)unBit);
	//hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, (unChannel == 2 ? 0x03 : 0x3F));
	hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)unChannel);
	hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)unSampling);

	// prepare additional user data (according to MS documentation for AAC decoder)
	// https://docs.microsoft.com/zh-tw/windows/win32/medfound/aac-decoder
	// https://docs.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-heaacwaveinfo
	// https://www.twblogs.net/a/5d486dabbd9eee541c303816
	// hard wired for 2 channel, 44100 kHz

	byte arrUser[] = { 0x00, 0x00/*wPayloadType 0 = raw AAC*/,
					   0xFE, 0x00/*wAudioProfileLevelIndication, 0 or FE = unknown*/,
					   0x00, 0x00/*wStructType*/, 
					   0x00, 0x00/*wReserved1*/,
					   0x00, 0x00, 0x00, 0x00/*wReserved2*/,
					   0x12, 0x10 };	//last two bytes is from AudioSpecificConfig() defined by MPEG-4
										/*AudioSpecificConfig.audioObjectType = 2 (AAC LC) (5 bits)
										  AudioSpecificConfig.samplingFrequencyIndex = 4 (4 bits) (44100)
										  AudioSpecificConfig.channelConfiguration = 2 (4 bits) (2 channel)
										  GASpecificConfig.frameLengthFlag = 0 (1 bit)
										  GASpecificConfig.dependsOnCoreCoder = 0 (1 bit)
										  GASpecificConfig.extensionFlag = 0 (1 bit)
										*/
										// 00010 0100 0010 0 0 0
	pMediaTypeIn->SetBlob(MF_MT_USER_DATA, arrUser, 14);

	hrError = pDecoder->SetInputType(0, pMediaTypeIn, 0);
	if (FAILED(hrError))
		return hrError;

	hrError = pMediaTypeIn->Release();
	if (FAILED(hrError))
		return hrError;

	// set output type
	hrError = MFCreateMediaType(&pOutType);
	if (FAILED(hrError))
		return hrError;

	hrError = pOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	if (FAILED(hrError))
		return hrError;
	hrError = pOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	if (FAILED(hrError))
		return hrError;
	hrError = pOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, (UINT32)unBit);
	if (FAILED(hrError))
		return hrError;
	hrError = pOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)unSampling);
	if (FAILED(hrError))
		return hrError;
	hrError = pOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)unChannel);
	if (FAILED(hrError))
		return hrError;

	hrError = pDecoder->SetOutputType(0, pOutType, 0);
	if (FAILED(hrError))
		return hrError;
	hrError = pOutType->Release();
	if (FAILED(hrError))
		return hrError;
	
	//Set output type
	UINT32 typeIndex = 0;
	IMFMediaType* type;
	UINT32 channels, rate;
	hrError = pDecoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
	if (FAILED(hrError))
		return hrError;
	hrError = pDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	if (FAILED(hrError))
		return hrError;
	hrError = pDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
	if (FAILED(hrError))
		return hrError;

	/*
	while (type = nullptr, SUCCEEDED(pDecoder->GetOutputAvailableType(0, typeIndex++, &type))) {
		GUID subtype;
		hrError = type->GetGUID(MF_MT_SUBTYPE, &subtype);
		if (FAILED(hrError)) {
			continue;
		}
		if (subtype == MFAudioFormat_PCM) {
			hrError = pDecoder->SetOutputType(0, type, 0);
			hrError = type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
			hrError = type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
			return S_OK;
		}
	}
	*/
	hrError = pDecoder->GetInputStreamInfo(0, &mInputStreamInfo);
	if (FAILED(hrError))
		return hrError;
	hrError = pDecoder->GetOutputStreamInfo(0, &mOutputStreamInfo);
	if (FAILED(hrError))
		return hrError;

	return hrError;
}

HRESULT AACDecoderMFImpl::enqueue(void* buffer, DWORD bufferSize, int64_t aTimestamp) {
	// process input buffer
	static int count_ok = 0, count_no_accept = 0, count_fail =0;
	IMFSample* pSampleIn = nullptr;
	HRESULT hr = S_OK;

	// Create a MF buffer from our data
	IMFMediaBuffer* in_buffer;
	//int32_t bufferSize = std::max<uint32_t>(uint32_t(mInputStreamInfo.cbSize), buffer_size);
	UINT32 alignment = (mInputStreamInfo.cbAlignment > 1) ? mInputStreamInfo.cbAlignment - 1 : 0;
	hr = MFCreateAlignedMemoryBuffer(bufferSize, alignment, &in_buffer);
	if (FAILED(hr))
		return hr;

	DWORD max_len, cur_len;
	BYTE* data;
	hr = in_buffer->Lock(&data, &max_len, &cur_len);
	if (FAILED(hr))
		return hr;

	memcpy(data, buffer, bufferSize);

	hr = in_buffer->Unlock();
	if (FAILED(hr))
		return hr;

	hr = in_buffer->SetCurrentLength(bufferSize);
	if (FAILED(hr))
		return hr;

	hr = MFCreateSample(&pSampleIn);
	if (FAILED(hr))
		return hr;

	hr = pSampleIn->AddBuffer(in_buffer);
	if (FAILED(hr)) {
		in_buffer->Release();
		pSampleIn->Release();
		return hr;
	}

	hr = pSampleIn->SetSampleTime(UsecsToHNs(aTimestamp));
	if (FAILED(hr)) {
		in_buffer->Release();
		pSampleIn->Release();
		return hr;
	}

	hr = pDecoder->ProcessInput(0, pSampleIn, 0);
	if (hr == MF_E_NOTACCEPTING) {
		// MFT *already* has enough data to produce a sample. Retrieve it.
		//hr = pDecoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
		in_buffer->Release();
		pSampleIn->Release();
		count_no_accept++;
		fprintf(file_lpcm, "[Bean]ProcessIntput %d, MF_E_NOTACCEPTING\n", count_no_accept);
		return MF_E_NOTACCEPTING;
	}
	else if (FAILED(hr)) {
		in_buffer->Release();
		pSampleIn->Release();
		count_fail++;
		fprintf(file_lpcm, "[Bean]ProcessIntput %d, FAILED\n", count_fail);
		return hr;
	}
	count_ok++;
	fprintf(file_lpcm, "[Bean]ProcessIntput %d, S_OK\n", count_ok);

	in_buffer->Release();
	pSampleIn->Release();

	return hr;
}

/**
 * Note: expected to return MF_E_TRANSFORM_NEED_MORE_INPUT and
 *       MF_E_TRANSFORM_STREAM_CHANGE which must be handled by caller.
 */
HRESULT AACDecoderMFImpl::decode() {
	// get LPCM output
	DWORD dwStatus;
	IMFSample *pSampleOut;
	MFT_OUTPUT_STREAM_INFO xOutputInfo;
	MFT_OUTPUT_DATA_BUFFER arrOutput[1];
	IMFMediaBuffer* pBuffer = nullptr;
	BYTE* pAudioData = nullptr;
	DWORD cur_len;
	DWORD max_len;
	HRESULT hrError;
	DWORD output_status;
	static int count_ok = 0, count_need = 0, count_fail =0;

	pSampleOut = 0;
	IMFMediaBuffer* pBufferOut = 0;
	hrError = GetOutputStatus(pDecoder, &output_status);
	if (FAILED(hrError)) {
		printf("");
	}
	while (SUCCEEDED(hrError = GetOutputStatus(pDecoder, &output_status)) &&
		   output_status == MFT_OUTPUT_STATUS_SAMPLE_READY) {
		HRESULT hrError = pDecoder->GetOutputStreamInfo(0, &xOutputInfo);
		if (FAILED(hrError)) {
			return hrError;
		}
		hrError = MFCreateSample(&pSampleOut);
		if (FAILED(hrError)) {
			return hrError;
		}

		IMFMediaBuffer* buffer = nullptr;
		int32_t bufferSize = xOutputInfo.cbSize;
		UINT32 alignment = (xOutputInfo.cbAlignment > 1) ? xOutputInfo.cbAlignment - 1 : 0;
		hrError = MFCreateAlignedMemoryBuffer(bufferSize, alignment, &pBufferOut);
		if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}
		hrError = pBufferOut->SetCurrentLength(bufferSize);
		if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}
		hrError = pSampleOut->AddBuffer(pBufferOut);
		if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}

		arrOutput[0].dwStreamID = 0;
		arrOutput[0].dwStatus = 0;
		arrOutput[0].pEvents = nullptr;
		arrOutput[0].pSample = pSampleOut;

		dwStatus = 0;

		// Invoke the Media Foundation decoder
		// Note: we don't use ON_SUCCEEDED here since ProcessOutput returns
		//       MF_E_TRANSFORM_NEED_MORE_INPUT often (too many log messages).
		hrError = pDecoder->ProcessOutput(0, 1, arrOutput, &dwStatus);
		if (hrError == MF_E_TRANSFORM_NEED_MORE_INPUT || hrError== MF_E_TRANSFORM_STREAM_CHANGE){
			/* can return MF_E_TRANSFORM_NEED_MORE_INPUT or
			   MF_E_TRANSFORM_STREAM_CHANGE (entirely acceptable) */
			pBufferOut->Release();
			pSampleOut->Release();
			count_need++;
			fprintf(file_lpcm, "[Bean]ProcessOutput %d, need_more\n", count_need);
			return hrError;
		}
		else if (hrError == S_OK) {
			pBufferOut->Release();
			pSampleOut->Release();
			count_ok++;
			fprintf(file_lpcm, "[Bean]ProcessOutput %d, S_OK\n", count_ok);
			return hrError;
		}
		else {
			//pBufferOut->Release();
			//pSampleOut->Release();
			count_fail++;
			fprintf(file_lpcm, "[Bean]ProcessOutput %d, E_FAIL\n", count_fail);
			//return hrError;
		}

		pSampleOut->ConvertToContiguousBuffer(&pBuffer);
		pBuffer->Lock(&pAudioData, &max_len, &cur_len);
#ifdef DUMP_AUDIO
		// write wave file
		DWORD cur_length;
		hrError = pBuffer->GetCurrentLength(&cur_length);
		if (FAILED(hrError)) {
			//RTC_LOG(LS_ERROR) << "Decode failure: could not get buffer length.";
			return hrError;
		}

		if (cur_length > 0 && file_lpcm != NULL) {
			//fwrite(pAudioData, sizeof(char), cur_length, file_lpcm);
		}
#endif
		pBuffer->Unlock();

		pBuffer->Release();
		pBufferOut->Release();
		pSampleOut->Release();
	}

	return hrError;
}

}
}