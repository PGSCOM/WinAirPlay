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
	//*output_status = MFT_OUTPUT_STATUS_SAMPLE_READY;
	return hr;
}

HRESULT AACDecoderMFImpl::init(void) {
	// init MF decoder, using Microsoft hard wired GUID
	HRESULT hrError = S_OK;
	hrError = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	hrError = MFStartup(MF_VERSION, MFSTARTUP_FULL);
	hrError = CoCreateInstance(CLSID_CMSAACDecMFT, 0, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void**)&pDecoder);

	// Set Input Type
	hrError = MFCreateMediaType(&pMediaTypeIn);
	if (FAILED(hrError))
		return hrError;
	
	hrError = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio); //MFMediaType_Audio only supports MF_MT_AAC_PAYLOAD_TYPE 0, 1, 3
	if (FAILED(hrError))
		return hrError;
	
	hrError = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC); //MFAudioFormat_AAC==MEDIASUBTYPE_MPEG_HEAAC, MFAudioFormat_ADTS, MEDIASUBTYPE_RAW_AAC1
	if (FAILED(hrError))
		return hrError;

	//https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder
	//optional 0x29(default), 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33
	/*hrError = pMediaTypeIn->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x2A);
	  if (FAILED(hrError))
		  return hrError;
	*/
	hrError = pMediaTypeIn->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);// 0 = Raw, 1 = ADTS, 2=ADIF, 3=LOAS; default value 0
	if (FAILED(hrError))
		return hrError;

	hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, (UINT32)bitsDepth); //must be fixed 16
	if (FAILED(hrError))
		return hrError;
	
	//hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_CHANNEL_MASK, (unChannel == 2 ? 0x03 : 0x3F));
	
	hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)channelNum); //1 (mono) or 2 (stereo), or 6 (5.1).
	if (FAILED(hrError))
		return hrError;
	
	hrError = pMediaTypeIn->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)sampling);//only support 44100 (44.1 KHz) 48000 (48 KHz)
	if (FAILED(hrError))
		return hrError;

	// prepare additional user data (according to MS documentation for AAC decoder)
	// https://docs.microsoft.com/zh-tw/windows/win32/medfound/aac-decoder
	// https://docs.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-heaacwaveinfo
	// https://www.twblogs.net/a/5d486dabbd9eee541c303816
	byte arrUser[] = { 0x00, 0x00/*wPayloadType  0 = Raw, 1 = ADTS, 2=ADIF, 3=LOAS*/,
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
	hrError = pMediaTypeIn->SetBlob(MF_MT_USER_DATA, arrUser, 14);// set to 2 bytes AudioSpecificConfig() if MEDIASUBTYPE_RAW_AAC1
	if (FAILED(hrError))
		return hrError;

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

	hrError = pOutType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM); //MFAudioFormat_Float, MFAudioFormat_PCM, MFAudioFormat_AAC
	if (FAILED(hrError))
		return hrError;

	hrError = pOutType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, (UINT32)bitsDepth);
	if (FAILED(hrError))
		return hrError;

	hrError = pOutType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)sampling);
	if (FAILED(hrError))
		return hrError;

	hrError = pOutType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32)channelNum);
	if (FAILED(hrError))
		return hrError;

	hrError = pDecoder->SetOutputType(0, pOutType, 0);
	if (FAILED(hrError))
		return hrError;
	
	hrError = pOutType->Release();
	if (FAILED(hrError))
		return hrError;
	
	//Read aac file
	/*
	IMFMediaType* partialMediaType = nullptr;
	hrError = MFCreateSourceReaderFromURL(aacFile.c_str(), 0, &pReader);
	if (FAILED(hrError))
		return hrError;

	hrError = MFCreateMediaType(&partialMediaType);
	if (FAILED(hrError))
		return hrError;

	hrError = partialMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	if (FAILED(hrError))
		return hrError;

	hrError = partialMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);		// create AAC from AAC -> frame reader
	if (FAILED(hrError))
		return hrError;
	
	hrError = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, partialMediaType);
	if (FAILED(hrError))
		return hrError;
	
	hrError = partialMediaType->Release();
	if (FAILED(hrError))
		return hrError;
	*/
	hrError = pDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
	if (FAILED(hrError))
		return hrError;
	
	hrError = pDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
	if (FAILED(hrError))
		return hrError;

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
	HRESULT hr = S_OK;
	IMFSample* pSampleIn = nullptr;

	// Create a MF buffer from our data
	IMFMediaBuffer* in_buffer = nullptr;
	//int32_t bufferSize = std::max<uint32_t>(uint32_t(mInputStreamInfo.cbSize), buffer_size);
	UINT32 alignment = (mInputStreamInfo.cbAlignment > 1) ? mInputStreamInfo.cbAlignment - 1 : 0;
	hr = MFCreateAlignedMemoryBuffer(bufferSize, alignment, &in_buffer);
	if (FAILED(hr))
		return hr;

	DWORD max_len, cur_len;
	BYTE* data = nullptr;
	hr = in_buffer->Lock(&data, &max_len, &cur_len);
	if (FAILED(hr))
		return hr;

	memcpy(data, buffer, bufferSize);

	hr = in_buffer->Unlock();
	if (FAILED(hr)) {
		in_buffer->Release();
		return hr;
	}
	
	hr = in_buffer->SetCurrentLength(bufferSize);
	if (FAILED(hr)) {
		in_buffer->Release();
		return hr;
	}

	hr = MFCreateSample(&pSampleIn);
	if (FAILED(hr)) {
		in_buffer->Release();
		return hr;
	}

	hr = pSampleIn->AddBuffer(in_buffer);
	if (FAILED(hr)) {
		in_buffer->Release();
		pSampleIn->Release();
		return hr;
	}
	/*
	LONGLONG timestamp;
	DWORD actualStreamIndex;
	hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &actualStreamIndex, &dwFlags, &timestamp, &pSampleIn);
	if (dwFlags == MF_SOURCE_READER_FLAG::MF_SOURCE_READERF_ENDOFSTREAM) {
		return MF_E_END_OF_STREAM;
	}
	*/
	hr = pDecoder->ProcessInput(0, pSampleIn, 0);
	if (hr == MF_E_NOTACCEPTING) {
		// MFT *already* has enough data to produce a sample. Retrieve it.
		//hr = pDecoder->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
		in_buffer->Release();
		pSampleIn->Release();
		return MF_E_NOTACCEPTING;
	}
	else if (FAILED(hr)) {
		in_buffer->Release();
		pSampleIn->Release();
		return hr;
	}

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
	IMFSample *pSampleOut = nullptr;
	MFT_OUTPUT_STREAM_INFO outputInfo;
	MFT_OUTPUT_DATA_BUFFER arrOutput[1];
	BYTE* pAudioData = nullptr;
	HRESULT hrError;
	DWORD outputStatus;

	pSampleOut = nullptr;
	IMFMediaBuffer* pBufferOut = nullptr;

	while (SUCCEEDED(hrError = GetOutputStatus(pDecoder, &outputStatus)) &&
		   outputStatus == MFT_OUTPUT_STATUS_SAMPLE_READY) {
		HRESULT hrError = pDecoder->GetOutputStreamInfo(0, &outputInfo);
		if (FAILED(hrError))
			return hrError;

		int32_t bufferSize = outputInfo.cbSize;
		hrError = MFCreateSample(&pSampleOut);
		if (FAILED(hrError))
			return hrError;

		UINT32 alignment = (outputInfo.cbAlignment > 1) ? outputInfo.cbAlignment - 1 : 0;
		hrError = MFCreateAlignedMemoryBuffer(bufferSize, alignment, &pBufferOut);
		if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}

		hrError = pBufferOut->SetCurrentLength(0);
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
		if (hrError == MF_E_TRANSFORM_NEED_MORE_INPUT || hrError== MF_E_TRANSFORM_STREAM_CHANGE) {
			/* can return MF_E_TRANSFORM_NEED_MORE_INPUT or
			   MF_E_TRANSFORM_STREAM_CHANGE (entirely acceptable) */
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}
		else if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}

#ifdef DUMP_AUDIO
		IMFMediaBuffer* pBuffer = nullptr;
		DWORD dwLength = 0;
		hrError = pBufferOut->GetCurrentLength(&dwLength);
		if (dwLength == 0 || FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}
		DWORD curLen = 0, maxLen = 0;
		hrError = pSampleOut->ConvertToContiguousBuffer(&pBuffer);
		if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}
		hrError = pBuffer->Lock(&pAudioData, &maxLen, &curLen);
		if (FAILED(hrError)) {
			pBufferOut->Release();
			pSampleOut->Release();
			return hrError;
		}
		// write lpcm file
		if (dwLength > 0 && file_lpcm != NULL) {
			fwrite(pAudioData, sizeof(char), curLen, file_lpcm);
			fflush(file_lpcm);
		}
		pBuffer->Unlock();
		pBuffer->Release();
#endif
		pBufferOut->Release();
		pSampleOut->Release();
	}

	return hrError;
}

}
}