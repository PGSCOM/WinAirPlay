#include "h264_decoder_mf_impl.h"

#include <Windows.h>
#include <codecapi.h>
#include <mfapi.h>
#include <Mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <ppltasks.h>
#include <robuffer.h>
#include <iomanip>

#include "video_error_codes.h"
#include "video_frame_type.h"
#include "video_rotation.h"

#ifdef _DEBUG
#define ON_SUCCEEDED(act)                      \
  if (SUCCEEDED(hr)) {                         \
    hr = (act);                                \
    if (FAILED(hr)) {                          \
      __debugbreak();                          \
    }                                          \
  }
#else
#define ON_SUCCEEDED(act)                      \
  if (SUCCEEDED(hr)) {                         \
    hr = (act);                                \
    if (FAILED(hr)) {                          \
      RTC_LOG(LS_WARNING) << "ERROR:" << #act; \
    }                                          \
  }
#endif

namespace owt {
namespace base {

H264DecoderMFImpl::H264DecoderMFImpl()
    : buffer_pool_(false, 300), /* max_number_of_buffers*/
      width_(0u),
      height_(0u)/*,
      decode_complete_callback_(nullptr)*/ {
  HRESULT hr = S_OK;
  //the yuvplayer should show in 688*972 NV12 format
#ifdef DUMP_VIDEO
  fopen_s(&pFile, "H264Output.yuv", "wb+");
#endif
  ON_SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
}

H264DecoderMFImpl::~H264DecoderMFImpl() {
  OutputDebugStringW(L"H264DecoderMFImpl::~H264DecoderMFImpl()\n");
#ifdef DUMP_VIDEO
  if (pFile != nullptr) {
      fclose(pFile);
      pFile = nullptr;
  }
#endif
  HRESULT hr = S_OK;
  Release();
  ON_SUCCEEDED(MFShutdown());
}

HRESULT ConfigureOutputMediaType(ComPtr<IMFTransform> decoder, GUID media_type, bool* type_found) {
  HRESULT hr = S_OK;
  *type_found = false;

  int type = 0;
  while (true) {
    ComPtr<IMFMediaType> output_media;
    ON_SUCCEEDED(decoder->GetOutputAvailableType(0, type, &output_media));
    if (hr == MF_E_NO_MORE_TYPES)
      return S_OK;

    GUID cur_type;
    ON_SUCCEEDED(output_media->GetGUID(MF_MT_SUBTYPE, &cur_type));
    if (FAILED(hr))
      return hr;

    if (cur_type == media_type) {
      hr = decoder->SetOutputType(0, output_media.Get(), 0);
      ON_SUCCEEDED(*type_found = true);
      return hr;
    }

    type++;
  }
}

HRESULT CreateInputMediaType(IMFMediaType** pp_input_media,
                             UINT32 img_width,
                             UINT32 img_height,
                             UINT32 frame_rate) {
  HRESULT hr = MFCreateMediaType(pp_input_media);

  IMFMediaType* input_media = *pp_input_media;
  ON_SUCCEEDED(input_media->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
  ON_SUCCEEDED(input_media->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));
  ON_SUCCEEDED(
      MFSetAttributeRatio(input_media, MF_MT_PIXEL_ASPECT_RATIO, 1, 1));
  ON_SUCCEEDED(input_media->SetUINT32(
      MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive));

  if (frame_rate > 0u) {
    ON_SUCCEEDED(MFSetAttributeRatio(input_media, MF_MT_FRAME_RATE,
                                     frame_rate, 1));
  }

  if (img_width > 0u && img_height > 0u) {
    ON_SUCCEEDED(MFSetAttributeSize(input_media, MF_MT_FRAME_SIZE,
                                    img_width, img_height));
  }

  return hr;
}

int H264DecoderMFImpl::InitDecode(uint32_t width, uint32_t height, uint32_t maxFramerate,
                                  int number_of_cores) {
  //RTC_LOG(LS_INFO) << "H264DecoderMFImpl::InitDecode()\n";

  width_ = width;
  height_ = height;

  HRESULT hr = S_OK;

  ON_SUCCEEDED(CoCreateInstance(CLSID_MSH264DecoderMFT, nullptr,
                                CLSCTX_INPROC_SERVER, IID_IUnknown,
                                (void**)&decoder_));

  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR) << "Init failure: could not create Media Foundation H264 "
      //                   "decoder instance.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Try set decoder attributes
  ComPtr<IMFAttributes> decoder_attrs;
  ON_SUCCEEDED(decoder_->GetAttributes(decoder_attrs.GetAddressOf()));

  if (SUCCEEDED(hr)) {
    ON_SUCCEEDED(decoder_attrs->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE));
    if (FAILED(hr)) {
      //RTC_LOG(LS_WARNING)
          //<< "Init warning: failed to set low latency in H264 decoder.";
      hr = S_OK;
    }

    ON_SUCCEEDED(
        decoder_attrs->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE));
    if (FAILED(hr)) {
      //RTC_LOG(LS_WARNING)
          //<< "Init warning: failed to set HW accel in H264 decoder.";
    }
  }

  // Clear any error from try set attributes
  hr = S_OK;

  ComPtr<IMFMediaType> input_media;
  ON_SUCCEEDED(CreateInputMediaType(
      input_media.GetAddressOf(), width_, height_,
      maxFramerate));

  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR) << "Init failure: could not create input media type.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Register the input type with the decoder
  ON_SUCCEEDED(decoder_->SetInputType(0, input_media.Get(), 0));

  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR)
        //<< "Init failure: failed to set input media type on decoder.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Assert MF supports NV12 output
  bool suitable_type_found;
  ON_SUCCEEDED(ConfigureOutputMediaType(decoder_, MFVideoFormat_NV12,
                                        &suitable_type_found));

  if (FAILED(hr) || !suitable_type_found) {
    //RTC_LOG(LS_ERROR) << "Init failure: failed to find a valid output media "
                         //"type for decoding.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  DWORD status;
  ON_SUCCEEDED(decoder_->GetInputStatus(0, &status));

  // Validate that decoder is up and running
  if (SUCCEEDED(hr)) {
    if (MFT_INPUT_STATUS_ACCEPT_DATA != status)
      // H.264 decoder MFT is not accepting data
      return WEBRTC_VIDEO_CODEC_ERROR;
  }

  ON_SUCCEEDED(decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL));
  ON_SUCCEEDED(
      decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL));
  ON_SUCCEEDED(
      decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL));

  inited_ = true;
  return SUCCEEDED(hr) ? WEBRTC_VIDEO_CODEC_OK : WEBRTC_VIDEO_CODEC_ERROR;
}

/**
 * Workaround [MF H264 bug: Output status is never set, even when ready]
 *  => For now, always mark "ready" (results in extra buffer alloc/dealloc).
 */
HRESULT GetOutputStatus(ComPtr<IMFTransform> decoder, DWORD* output_status) {
  HRESULT hr = decoder->GetOutputStatus(output_status);

  // Don't MFT trust output status for now.
  *output_status = MFT_OUTPUT_STATUS_SAMPLE_READY;
  return hr;
}

/**
 * Note: expected to return MF_E_TRANSFORM_NEED_MORE_INPUT and
 *       MF_E_TRANSFORM_STREAM_CHANGE which must be handled by caller.
 */
HRESULT H264DecoderMFImpl::FlushFrames(uint32_t rtp_timestamp,
                                       uint64_t ntp_time_ms) {
  HRESULT hr;
  DWORD output_status;

  while (SUCCEEDED(hr = GetOutputStatus(decoder_, &output_status)) &&
         output_status == MFT_OUTPUT_STATUS_SAMPLE_READY) {
    // Get needed size of our output buffer
    MFT_OUTPUT_STREAM_INFO strm_info;
    ON_SUCCEEDED(decoder_->GetOutputStreamInfo(0, &strm_info));
    if (FAILED(hr)) {
      //RTC_LOG(LS_ERROR) << "Decode failure: failed to get output stream info.";
      return hr;
    }

    // Create output sample
    ComPtr<IMFMediaBuffer> out_buffer;
    ON_SUCCEEDED(MFCreateMemoryBuffer(strm_info.cbSize, &out_buffer));
    if (FAILED(hr)) {
      //RTC_LOG(LS_ERROR)
        //  << "Decode failure: output image memory buffer creation failed.";
      return hr;
    }

    ComPtr<IMFSample> out_sample;
    ON_SUCCEEDED(MFCreateSample(&out_sample));
    if (FAILED(hr)) {
      //RTC_LOG(LS_ERROR) << "Decode failure: output in_sample creation failed.";
      return hr;
    }

    ON_SUCCEEDED(out_sample->AddBuffer(out_buffer.Get()));
    if (FAILED(hr)) {
      //RTC_LOG(LS_ERROR)
          //<< "Decode failure: failed to add buffer to output in_sample.";
      return hr;
    }

    // Create output buffer description
    MFT_OUTPUT_DATA_BUFFER output_data_buffer[1];
    output_data_buffer[0].dwStatus = 0;
    output_data_buffer[0].dwStreamID = 0;
    output_data_buffer[0].pEvents = nullptr;
    output_data_buffer[0].pSample = out_sample.Get();

    // Invoke the Media Foundation decoder
    // Note: we don't use ON_SUCCEEDED here since ProcessOutput returns
    //       MF_E_TRANSFORM_NEED_MORE_INPUT often (too many log messages).
    DWORD status;
    hr = decoder_->ProcessOutput(0, 1, output_data_buffer, &status);

    if (FAILED(hr))
      return hr; /* can return MF_E_TRANSFORM_NEED_MORE_INPUT or
                    MF_E_TRANSFORM_STREAM_CHANGE (entirely acceptable) */

    // Copy raw output sample data to video frame buffer.
    ComPtr<IMFMediaBuffer> src_buffer;
    ON_SUCCEEDED(out_sample->ConvertToContiguousBuffer(&src_buffer));
    if (FAILED(hr)) {
      //RTC_LOG(LS_ERROR) << "Decode failure: failed to get contiguous buffer.";
      return hr;
    }

    uint32_t width, height, buffer_width, buffer_height;
    {
      // Get the output media type
      ComPtr<IMFMediaType> output_type;
      ON_SUCCEEDED(
          decoder_->GetOutputCurrentType(0, output_type.GetAddressOf()));

      // Query the actual buffer size to get the stride and vertical padding, so
      // the UV plane can be accessed at the right offset.
      ON_SUCCEEDED(MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE,
                                      &buffer_width, &buffer_height));

      // Query the visible area of the frame, which is the data that must be
      // returned to the caller. Sometimes this attribute is not present, and in
      // general this means that the frame size is not padded.
      MFVideoArea video_area;
      if (SUCCEEDED(output_type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                                         (UINT8*)&video_area,
                                         sizeof(video_area), nullptr))) {
        width = video_area.Area.cx;
        height = video_area.Area.cy;
      } else {
        // Fallback to buffer size
        width = buffer_width;
        height = buffer_height;
      }

      // Update cached values
      width_ = width;
      height_ = height;
    }

    // Create a new I420 buffer to pass to WebRTC
    rtc::scoped_refptr<webrtc::I420Buffer> buffer =
        buffer_pool_.CreateBuffer(width, height);
    if (!buffer.get()) {
      // Pool has too many pending frames.
      //RTC_LOG(LS_WARNING) << "Decode warning: too many frames. Dropping frame.";
      return 1;
    }

    DWORD cur_length;
    ON_SUCCEEDED(src_buffer->GetCurrentLength(&cur_length));
    if (FAILED(hr)) {
      //RTC_LOG(LS_ERROR) << "Decode failure: could not get buffer length.";
      return hr;
    }

    if (cur_length > 0) {
      BYTE* src_data = nullptr;
      DWORD max_len, cur_len;
      ON_SUCCEEDED(src_buffer->Lock(&src_data, &max_len, &cur_len));
      if (FAILED(hr)) {
        //RTC_LOG(LS_ERROR) << "Decode failure: could lock buffer for copying.";
        return hr;
      }
#ifdef DUMP_VIDEO
      fwrite(src_data, sizeof(char), cur_len, pFile);
      fflush(pFile);
#endif
      // Convert NV12 to I420. Y and UV sections have same stride in NV12
      // (width). The size of the Y section is the size of the frame, since Y
      // luminance values are 8-bits each.
      const int src_stride_y = buffer_width;
      const int src_stride_uv = buffer_width;
      const uint8_t* src_uv = src_data + (src_stride_y * buffer_height);
      /*
      libyuv::NV12ToI420(
          src_data, src_stride_y, src_uv, src_stride_uv, buffer->MutableDataY(),
          buffer->StrideY(), buffer->MutableDataU(), buffer->StrideU(),
          buffer->MutableDataV(), buffer->StrideV(), width, height);
      */
      ON_SUCCEEDED(src_buffer->Unlock());
      if (FAILED(hr))
        return hr;
    }
  }

  return hr;
}

/**
 * Note: acceptable to return MF_E_NOTACCEPTING (though it shouldn't since
 * last loop should've flushed)
 */
HRESULT H264DecoderMFImpl::EnqueueFrame(const webrtc::EncodedImage& input_image,
                                        bool missing_frames) {
  HRESULT hr = S_OK;

  // Create a MF buffer from our data
  ComPtr<IMFMediaBuffer> in_buffer;
  ON_SUCCEEDED(MFCreateMemoryBuffer(input_image.size(), &in_buffer));
  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR)
        //<< "Decode failure: input image memory buffer creation failed.";
    return hr;
  }

  DWORD max_len, cur_len;
  BYTE* data = nullptr;
  ON_SUCCEEDED(in_buffer->Lock(&data, &max_len, &cur_len));
  if (FAILED(hr))
    return hr;

  memcpy(data, input_image.data(), input_image.size());

  ON_SUCCEEDED(in_buffer->Unlock());
  if (FAILED(hr))
    return hr;

  ON_SUCCEEDED(in_buffer->SetCurrentLength(input_image.size()));
  if (FAILED(hr))
    return hr;

  // Create a sample from media buffer
  ComPtr<IMFSample> in_sample;
  ON_SUCCEEDED(MFCreateSample(&in_sample));
  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR) << "Decode failure: input in_sample creation failed.";
    return hr;
  }

  ON_SUCCEEDED(in_sample->AddBuffer(in_buffer.Get()));
  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR)
        //<< "Decode failure: failed to add buffer to input in_sample.";
    return hr;
  }

  int64_t sample_time_ms;
  if (first_frame_rtp_ == 0) {
    first_frame_rtp_ = input_image.Timestamp();
    sample_time_ms = 0;
  } else {
    // Convert from 90 khz, rounding to nearest ms.
    sample_time_ms =
        (static_cast<uint64_t>(input_image.Timestamp()) - first_frame_rtp_) /
            90.0 +
        0.5f;
  }

  ON_SUCCEEDED(in_sample->SetSampleTime(
      sample_time_ms *
      10000 /* convert milliseconds to 100-nanosecond unit */));
  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR)
        //<< "Decode failure: failed to set in_sample time on input in_sample.";
    return hr;
  }

  // Set sample attributes
  ComPtr<IMFAttributes> sample_attrs;
  ON_SUCCEEDED(in_sample.As(&sample_attrs));

  if (FAILED(hr)) {
    //RTC_LOG(LS_ERROR)
        //<< "Decode warning: failed to set image attributes for frame.";
    hr = S_OK;
  } else {
    if (input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey &&
        input_image._completeFrame) {
      ON_SUCCEEDED(sample_attrs->SetUINT32(MFSampleExtension_CleanPoint, TRUE));
      hr = S_OK;
    }

    if (missing_frames) {
      ON_SUCCEEDED(
          sample_attrs->SetUINT32(MFSampleExtension_Discontinuity, TRUE));
      hr = S_OK;
    }
  }

  // Enqueue sample with Media Foundation
  ON_SUCCEEDED(decoder_->ProcessInput(0, in_sample.Get(), 0));
  return hr;
}

int H264DecoderMFImpl::Decode(const webrtc::EncodedImage& input_image,
                              bool missing_frames,
                              uint64_t /*render_time_ms*/) {
  HRESULT hr = S_OK;

  if (!inited_) {
      return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  if (input_image.data() == NULL && input_image.size() > 0) {
      return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // Discard until keyframe.
  if (require_keyframe_) {
    if (input_image._frameType != webrtc::VideoFrameType::kVideoFrameKey ||
        !input_image._completeFrame) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    } else {
      require_keyframe_ = false;
    }
  }

  // Enqueue the new frame with Media Foundation
  ON_SUCCEEDED(EnqueueFrame(input_image, missing_frames));
  if (hr == MF_E_NOTACCEPTING) {
    // For robustness (shouldn't happen). Flush any old MF data blocking the
    // new frames.
    hr = decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);

    if (input_image._frameType == webrtc::VideoFrameType::kVideoFrameKey) {
      ON_SUCCEEDED(EnqueueFrame(input_image, missing_frames));
    } else {
      require_keyframe_ = true;
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }

  if (FAILED(hr))
      return WEBRTC_VIDEO_CODEC_ERROR;

  // Flush any decoded samples resulting from new frame, invoking callback
  hr = FlushFrames(input_image.Timestamp(), input_image.ntp_time_ms_);

  if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    // Output media type is no longer suitable. Reconfigure and retry.
    bool suitable_type_found;
    hr = ConfigureOutputMediaType(decoder_, MFVideoFormat_NV12,
                                  &suitable_type_found);

    if (FAILED(hr) || !suitable_type_found)
        return WEBRTC_VIDEO_CODEC_ERROR;

    // Reset width and height in case output media size has changed (though it
    // seems that would be unexpected, given that the input media would need to
    // be manually changed too).
    width_ = 0;
    height_ = 0;

    hr = FlushFrames(input_image.Timestamp(), input_image.ntp_time_ms_);
  }

  if (SUCCEEDED(hr) || hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return WEBRTC_VIDEO_CODEC_OK;
  }

  return WEBRTC_VIDEO_CODEC_ERROR;
}

int H264DecoderMFImpl::Release() {
  OutputDebugStringW(L"H264DecoderMFImpl::Release()\n");
  HRESULT hr = S_OK;
  inited_ = false;

  // Release I420 frame buffer pool
  buffer_pool_.Release();

  if (decoder_ != NULL) {
    // Follow shutdown procedure gracefully. On fail, continue anyway.
    ON_SUCCEEDED(decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0));
    ON_SUCCEEDED(decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL));
    ON_SUCCEEDED(decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL));
    decoder_ = nullptr;
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

const char* H264DecoderMFImpl::ImplementationName() const {
  return "H264_MediaFoundation";
}

} 
}