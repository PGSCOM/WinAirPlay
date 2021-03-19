#ifndef H264_DECODER_MF_IMPL_H
#define H264_DECODER_MF_IMPL_H

#include<windows.h>
#include<mftransform.h>
#include<mutex>
#include <wrl.h>
#include <wrl/implements.h>

#include "i420_buffer_pool.h"
#include "encoded_image.h"

using Microsoft::WRL::ComPtr;

namespace owt {
namespace base {

class H264DecoderMFImpl{
 public:
  H264DecoderMFImpl();

  virtual ~H264DecoderMFImpl();

  int InitDecode(uint32_t width, uint32_t height, uint32_t maxFramerate, 
                 int number_of_cores);

  int Decode(const webrtc::EncodedImage& input_image,
             bool missing_frames,
             int64_t /*render_time_ms*/);

  //int RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback);

  int Release();

  const char* ImplementationName() const;

 private:
  HRESULT FlushFrames(uint32_t timestamp, uint64_t ntp_time_ms);
  HRESULT EnqueueFrame(const webrtc::EncodedImage& input_image, bool missing_frames);

 private:
  ComPtr<IMFTransform> decoder_;
  webrtc::I420BufferPool buffer_pool_;

  bool inited_ = false;
  bool require_keyframe_ = true;
  uint32_t first_frame_rtp_ = 0;
  uint32_t width_;
  uint32_t height_;
  std::mutex bufferMutex;
  //webrtc::DecodedImageCallback* decode_complete_callback_;
};

} }

#endif // H264_DECODER_MF_IMPL_H
