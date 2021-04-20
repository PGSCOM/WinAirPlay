#ifndef H264_ENCODER_MF_H
#define H264_ENCODER_MF_H
#include <map>

#include "common_types.h"
#include "scoped_refptr.h"
#include "video_content_type.h"
#include "video_frame_type.h"
#include "video_rotation.h"
#include "video_timing.h"

#include "h264_decoder_mf_impl.h"

namespace owt {
namespace base {

class EncodedImage {
public:
    EncodedImage();
    EncodedImage(EncodedImage&&);
    // Discouraged: potentially expensive.
    EncodedImage(const EncodedImage&);
    EncodedImage(uint8_t* buffer, size_t length, size_t capacity);

    ~EncodedImage();

    EncodedImage& operator=(EncodedImage&&);
    // Discouraged: potentially expensive.
    EncodedImage& operator=(const EncodedImage&);

    // TODO(nisse): Change style to timestamp(), set_timestamp(), for consistency
    // with the VideoFrame class.
    // Set frame timestamp (90kHz).
    void SetTimestamp(uint32_t timestamp) { timestamp_rtp_ = timestamp; }

    // Get frame timestamp (90kHz).
    uint32_t Timestamp() const { return timestamp_rtp_; }

    void SetEncodeTime(int64_t encode_start_ms, int64_t encode_finish_ms);

    int64_t NtpTimeMs() const { return ntp_time_ms_; }

    size_t size() const { return size_; }
    void set_size(size_t new_size) {
        // Allow set_size(0) even if we have no buffer.
        //RTC_DCHECK_LE(new_size, new_size == 0 ? 0 : capacity());
        new_size = (new_size == 0) ? 0 : capacity();
        size_ = new_size;
    }
    // TODO(nisse): Delete, provide only read-only access to the buffer.
    size_t capacity() const {
        return buffer_ ? capacity_ : (encoded_data_ ? encoded_data_->size() : 0);
    }

    void set_buffer(uint8_t* buffer, size_t capacity) {
        buffer_ = buffer;
        capacity_ = capacity;
    }

    void SetEncodedData(
        rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encoded_data) {
        encoded_data_ = encoded_data;
        size_ = encoded_data->size();
        buffer_ = nullptr;
    }

    void ClearEncodedData() {
        encoded_data_ = nullptr;
        size_ = 0;
        buffer_ = nullptr;
        capacity_ = 0;
    }
    /*
    std::unique_ptr<webrtc::EncodedImageBufferInterface> GetEncodedData() const {
        //RTC_DCHECK(buffer_ == nullptr);
        return encoded_data_;
    }
    */
    // TODO(nisse): Delete, provide only read-only access to the buffer.
    uint8_t* data() {
        return buffer_ ? buffer_
            : (encoded_data_ ? encoded_data_->data() : nullptr);
    }
    const uint8_t* data() const {
        return buffer_ ? buffer_
            : (encoded_data_ ? encoded_data_->data() : nullptr);
    }
    // TODO(nisse): At some places, code accepts a const ref EncodedImage, but
    // still writes to it, to clear padding at the end of the encoded data.
    // Padding is required by ffmpeg; the best way to deal with that is likely to
    // make this class ensure that buffers always have a few zero padding bytes.
    uint8_t* mutable_data() const { return const_cast<uint8_t*>(data()); }

    // TODO(bugs.webrtc.org/9378): Delete. Used by code that wants to modify a
    // buffer corresponding to a const EncodedImage. Requires an un-owned buffer.
    uint8_t* buffer() const { return buffer_; }

    // Hack to workaround lack of ownership of the encoded data. If we don't
    // already own the underlying data, make an owned copy.
    void Retain();

    uint32_t _encodedWidth = 0;
    uint32_t _encodedHeight = 0;
    // NTP time of the capture time in local timebase in milliseconds.
    // TODO(minyue): make this member private.
    int64_t ntp_time_ms_ = 0;
    int64_t capture_time_ms_ = 0;
    webrtc::VideoFrameType _frameType = webrtc::VideoFrameType::kVideoFrameDelta;
    webrtc::VideoRotation rotation_ = webrtc::VideoRotation::kVideoRotation_0;
    webrtc::VideoContentType content_type_ = webrtc::VideoContentType::UNSPECIFIED;
    bool _completeFrame = false;
    int qp_ = -1;  // Quantizer value.

    // When an application indicates non-zero values here, it is taken as an
    // indication that all future frames will be constrained with those limits
    // until the application indicates a change again.
    //webrtc::PlayoutDelay playout_delay_ = { -1, -1 };

    struct Timing {
        uint8_t flags = webrtc::VideoSendTiming::kInvalid;
        int64_t encode_start_ms = 0;
        int64_t encode_finish_ms = 0;
        int64_t packetization_finish_ms = 0;
        int64_t pacer_exit_ms = 0;
        int64_t network_timestamp_ms = 0;
        int64_t network2_timestamp_ms = 0;
        int64_t receive_start_ms = 0;
        int64_t receive_finish_ms = 0;
    } timing_;

private:
    // TODO(bugs.webrtc.org/9378): We're transitioning to always owning the
    // encoded data.
    rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> encoded_data_;
    size_t size_;  // Size of encoded frame data.
    // Non-null when used with an un-owned buffer.
    uint8_t* buffer_;
    // Allocated size of _buffer; relevant only if it's non-null.
    size_t capacity_;
    uint32_t timestamp_rtp_ = 0;
    int spatial_index_;
    std::map<int, size_t> spatial_layer_frame_size_bytes_;
    //std::unique_ptr<webrtc::ColorSpace> color_space_;
    // Information about packets used to assemble this video frame. This is needed
    // by |SourceTracker| when the frame is delivered to the RTCRtpReceiver's
    // MediaStreamTrack, in order to implement getContributingSources(). See:
    // https://w3c.github.io/webrtc-pc/#dom-rtcrtpreceiver-getcontributingsources
    //RtpPacketInfos packet_infos_;
    bool retransmission_allowed_ = true;
};

}
}
#endif //H264_ENCODER_MF_H
