// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "base/numerics/safe_conversions.h"
#include "ppapi/c/pp_array_output.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/video_encoder_resource.h"
#include "ppapi/proxy/video_frame_resource.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/media_stream_buffer_manager.h"
#include "ppapi/thunk/enter.h"

using ppapi::proxy::SerializedHandle;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_VideoEncoder_API;

namespace ppapi {
namespace proxy {

namespace {

void RunCallback(scoped_refptr<TrackedCallback>* callback, int32_t error) {
  if (!TrackedCallback::IsPending(*callback))
    return;

  scoped_refptr<TrackedCallback> temp;
  callback->swap(temp);
  temp->Run(error);
}

}  // namespace

VideoEncoderResource::ShmBuffer::ShmBuffer(uint32_t id,
                                           scoped_ptr<base::SharedMemory> shm)
    : id(id), shm(shm.Pass()) {
}

VideoEncoderResource::ShmBuffer::~ShmBuffer() {
}

VideoEncoderResource::BitstreamBuffer::BitstreamBuffer(uint32_t id,
                                                       uint32_t size,
                                                       bool key_frame)
    : id(id), size(size), key_frame(key_frame) {
}

VideoEncoderResource::BitstreamBuffer::~BitstreamBuffer() {
}

VideoEncoderResource::VideoEncoderResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance),
      initialized_(false),
      closed_(false),
      // Set |encoder_last_error_| to PP_OK after successful initialization.
      // This makes error checking a little more concise, since we can check
      // that the encoder has been initialized and hasn't returned an error by
      // just testing |encoder_last_error_|.
      encoder_last_error_(PP_ERROR_FAILED),
      input_frame_count_(0),
      input_coded_size_(PP_MakeSize(0, 0)),
      buffer_manager_(this),
      get_video_frame_data_(nullptr),
      get_bitstream_buffer_data_(nullptr) {
  SendCreate(RENDERER, PpapiHostMsg_VideoEncoder_Create());
}

VideoEncoderResource::~VideoEncoderResource() {
  Close();
}

PPB_VideoEncoder_API* VideoEncoderResource::AsPPB_VideoEncoder_API() {
  return this;
}

int32_t VideoEncoderResource::GetSupportedProfiles(
    const PP_ArrayOutput& output,
    const scoped_refptr<TrackedCallback>& callback) {
  if (TrackedCallback::IsPending(get_supported_profiles_callback_))
    return PP_ERROR_INPROGRESS;

  get_supported_profiles_callback_ = callback;
  Call<PpapiPluginMsg_VideoEncoder_GetSupportedProfilesReply>(
      RENDERER, PpapiHostMsg_VideoEncoder_GetSupportedProfiles(),
      base::Bind(&VideoEncoderResource::OnPluginMsgGetSupportedProfilesReply,
                 this, output));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoEncoderResource::GetFramesRequired() {
  if (encoder_last_error_)
    return encoder_last_error_;
  return input_frame_count_;
}

int32_t VideoEncoderResource::GetFrameCodedSize(PP_Size* size) {
  if (encoder_last_error_)
    return encoder_last_error_;
  *size = input_coded_size_;
  return PP_OK;
}

int32_t VideoEncoderResource::Initialize(
    PP_VideoFrame_Format input_format,
    const PP_Size* input_visible_size,
    PP_VideoProfile output_profile,
    uint32_t initial_bitrate,
    PP_HardwareAcceleration acceleration,
    const scoped_refptr<TrackedCallback>& callback) {
  if (initialized_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(initialize_callback_))
    return PP_ERROR_INPROGRESS;

  initialize_callback_ = callback;
  Call<PpapiPluginMsg_VideoEncoder_InitializeReply>(
      RENDERER, PpapiHostMsg_VideoEncoder_Initialize(
                    input_format, *input_visible_size, output_profile,
                    initial_bitrate, acceleration),
      base::Bind(&VideoEncoderResource::OnPluginMsgInitializeReply, this));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoEncoderResource::GetVideoFrame(
    PP_Resource* video_frame,
    const scoped_refptr<TrackedCallback>& callback) {
  if (encoder_last_error_)
    return encoder_last_error_;

  if (TrackedCallback::IsPending(get_video_frame_callback_))
    return PP_ERROR_INPROGRESS;

  get_video_frame_data_ = video_frame;
  get_video_frame_callback_ = callback;

  // Lazily ask for a shared memory buffer in which video frames are allocated.
  if (buffer_manager_.number_of_buffers() == 0) {
    Call<PpapiPluginMsg_VideoEncoder_GetVideoFramesReply>(
        RENDERER, PpapiHostMsg_VideoEncoder_GetVideoFrames(),
        base::Bind(&VideoEncoderResource::OnPluginMsgGetVideoFramesReply,
                   this));
  } else {
    TryWriteVideoFrame();
  }

  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoEncoderResource::Encode(
    PP_Resource video_frame,
    PP_Bool force_keyframe,
    const scoped_refptr<TrackedCallback>& callback) {
  if (encoder_last_error_)
    return encoder_last_error_;

  VideoFrameMap::iterator it = video_frames_.find(video_frame);
  if (it == video_frames_.end())
    // TODO(llandwerlin): accept MediaStreamVideoTrack's video frames.
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<VideoFrameResource> frame_resource = it->second;

  encode_callbacks_.insert(std::make_pair(video_frame, callback));

  Call<PpapiPluginMsg_VideoEncoder_EncodeReply>(
      RENDERER,
      PpapiHostMsg_VideoEncoder_Encode(frame_resource->GetBufferIndex(),
                                       PP_ToBool(force_keyframe)),
      base::Bind(&VideoEncoderResource::OnPluginMsgEncodeReply, this,
                 video_frame));

  // Invalidate the frame to prevent the plugin from modifying it.
  it->second->Invalidate();
  video_frames_.erase(it);

  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoEncoderResource::GetBitstreamBuffer(
    PP_BitstreamBuffer* bitstream_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  if (encoder_last_error_)
    return encoder_last_error_;
  if (TrackedCallback::IsPending(get_bitstream_buffer_callback_))
    return PP_ERROR_INPROGRESS;

  get_bitstream_buffer_callback_ = callback;
  get_bitstream_buffer_data_ = bitstream_buffer;

  if (!available_bitstream_buffers_.empty()) {
    BitstreamBuffer buffer(available_bitstream_buffers_.front());
    available_bitstream_buffers_.pop_front();
    WriteBitstreamBuffer(buffer);
  }

  return PP_OK_COMPLETIONPENDING;
}

void VideoEncoderResource::RecycleBitstreamBuffer(
    const PP_BitstreamBuffer* bitstream_buffer) {
  if (encoder_last_error_)
    return;
  BitstreamBufferMap::const_iterator iter =
      bitstream_buffer_map_.find(bitstream_buffer->buffer);
  if (iter != bitstream_buffer_map_.end()) {
    Post(RENDERER,
         PpapiHostMsg_VideoEncoder_RecycleBitstreamBuffer(iter->second));
  }
}

void VideoEncoderResource::RequestEncodingParametersChange(uint32_t bitrate,
                                                           uint32_t framerate) {
  if (encoder_last_error_)
    return;
  Post(RENDERER, PpapiHostMsg_VideoEncoder_RequestEncodingParametersChange(
                     bitrate, framerate));
}

void VideoEncoderResource::Close() {
  if (closed_)
    return;
  Post(RENDERER, PpapiHostMsg_VideoEncoder_Close());
  closed_ = true;
  if (!encoder_last_error_ || !initialized_)
    NotifyError(PP_ERROR_ABORTED);
  ReleaseFrames();
}

void VideoEncoderResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(VideoEncoderResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoEncoder_BitstreamBuffers,
        OnPluginMsgBitstreamBuffers)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoEncoder_BitstreamBufferReady,
        OnPluginMsgBitstreamBufferReady)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(PpapiPluginMsg_VideoEncoder_NotifyError,
                                        OnPluginMsgNotifyError)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(
        PluginResource::OnReplyReceived(params, msg))
  PPAPI_END_MESSAGE_MAP()
}

void VideoEncoderResource::OnPluginMsgGetSupportedProfilesReply(
    const PP_ArrayOutput& output,
    const ResourceMessageReplyParams& params,
    const std::vector<PP_VideoProfileDescription>& profiles) {
  int32_t error = params.result();
  if (error) {
    NotifyError(error);
    return;
  }

  ArrayWriter writer(output);
  if (!writer.is_valid()) {
    RunCallback(&get_supported_profiles_callback_, PP_ERROR_BADARGUMENT);
    return;
  }

  if (!writer.StoreVector(profiles)) {
    RunCallback(&get_supported_profiles_callback_, PP_ERROR_FAILED);
    return;
  }

  RunCallback(&get_supported_profiles_callback_,
              base::checked_cast<int32_t>(profiles.size()));
}

void VideoEncoderResource::OnPluginMsgInitializeReply(
    const ResourceMessageReplyParams& params,
    uint32_t input_frame_count,
    const PP_Size& input_coded_size) {
  DCHECK(!initialized_);

  encoder_last_error_ = params.result();
  if (!encoder_last_error_)
    initialized_ = true;

  input_frame_count_ = input_frame_count;
  input_coded_size_ = input_coded_size;

  RunCallback(&initialize_callback_, encoder_last_error_);
}

void VideoEncoderResource::OnPluginMsgGetVideoFramesReply(
    const ResourceMessageReplyParams& params,
    uint32_t frame_count,
    uint32_t frame_length,
    const PP_Size& frame_size) {
  int32_t error = params.result();
  if (error) {
    NotifyError(error);
    return;
  }

  base::SharedMemoryHandle buffer_handle;
  params.TakeSharedMemoryHandleAtIndex(0, &buffer_handle);

  if (!buffer_manager_.SetBuffers(
          frame_count, frame_length,
          make_scoped_ptr(new base::SharedMemory(buffer_handle, false)),
          true)) {
    NotifyError(PP_ERROR_FAILED);
    return;
  }

  if (TrackedCallback::IsPending(get_video_frame_callback_))
    TryWriteVideoFrame();
}

void VideoEncoderResource::OnPluginMsgEncodeReply(
    PP_Resource video_frame,
    const ResourceMessageReplyParams& params,
    uint32_t frame_id) {
  DCHECK_NE(encode_callbacks_.size(), 0U);
  encoder_last_error_ = params.result();

  EncodeMap::iterator it = encode_callbacks_.find(video_frame);
  DCHECK(encode_callbacks_.end() != it);

  scoped_refptr<TrackedCallback> callback = it->second;
  encode_callbacks_.erase(it);
  RunCallback(&callback, encoder_last_error_);

  buffer_manager_.EnqueueBuffer(frame_id);
  // If the plugin is waiting for a video frame, we can give the one
  // that just became available again.
  if (TrackedCallback::IsPending(get_video_frame_callback_))
    TryWriteVideoFrame();
}

void VideoEncoderResource::OnPluginMsgBitstreamBuffers(
    const ResourceMessageReplyParams& params,
    uint32_t buffer_length) {
  std::vector<base::SharedMemoryHandle> shm_handles;
  params.TakeAllSharedMemoryHandles(&shm_handles);
  if (shm_handles.size() == 0) {
    NotifyError(PP_ERROR_FAILED);
    return;
  }

  for (uint32_t i = 0; i < shm_handles.size(); ++i) {
    scoped_ptr<base::SharedMemory> shm(
        new base::SharedMemory(shm_handles[i], true));
    CHECK(shm->Map(buffer_length));

    ShmBuffer* buffer = new ShmBuffer(i, shm.Pass());
    shm_buffers_.push_back(buffer);
    bitstream_buffer_map_.insert(
        std::make_pair(buffer->shm->memory(), buffer->id));
  }
}

void VideoEncoderResource::OnPluginMsgBitstreamBufferReady(
    const ResourceMessageReplyParams& params,
    uint32_t buffer_id,
    uint32_t buffer_size,
    bool key_frame) {
  available_bitstream_buffers_.push_back(
      BitstreamBuffer(buffer_id, buffer_size, key_frame));

  if (TrackedCallback::IsPending(get_bitstream_buffer_callback_)) {
    BitstreamBuffer buffer(available_bitstream_buffers_.front());
    available_bitstream_buffers_.pop_front();
    WriteBitstreamBuffer(buffer);
  }
}

void VideoEncoderResource::OnPluginMsgNotifyError(
    const ResourceMessageReplyParams& params,
    int32_t error) {
  NotifyError(error);
}

void VideoEncoderResource::NotifyError(int32_t error) {
  encoder_last_error_ = error;
  RunCallback(&get_supported_profiles_callback_, error);
  RunCallback(&initialize_callback_, error);
  RunCallback(&get_video_frame_callback_, error);
  get_video_frame_data_ = nullptr;
  RunCallback(&get_bitstream_buffer_callback_, error);
  get_bitstream_buffer_data_ = nullptr;
  for (EncodeMap::iterator it = encode_callbacks_.begin();
       it != encode_callbacks_.end(); ++it) {
    scoped_refptr<TrackedCallback> callback = it->second;
    RunCallback(&callback, error);
  }
  encode_callbacks_.clear();
}

void VideoEncoderResource::TryWriteVideoFrame() {
  DCHECK(TrackedCallback::IsPending(get_video_frame_callback_));

  int32_t frame_id = buffer_manager_.DequeueBuffer();
  if (frame_id < 0)
    return;

  scoped_refptr<VideoFrameResource> resource = new VideoFrameResource(
      pp_instance(), frame_id, buffer_manager_.GetBufferPointer(frame_id));
  video_frames_.insert(
      VideoFrameMap::value_type(resource->pp_resource(), resource));

  *get_video_frame_data_ = resource->GetReference();
  get_video_frame_data_ = nullptr;
  RunCallback(&get_video_frame_callback_, PP_OK);
}

void VideoEncoderResource::WriteBitstreamBuffer(const BitstreamBuffer& buffer) {
  DCHECK_LT(buffer.id, shm_buffers_.size());

  get_bitstream_buffer_data_->size = buffer.size;
  get_bitstream_buffer_data_->buffer = shm_buffers_[buffer.id]->shm->memory();
  get_bitstream_buffer_data_->key_frame = PP_FromBool(buffer.key_frame);
  get_bitstream_buffer_data_ = nullptr;
  RunCallback(&get_bitstream_buffer_callback_, PP_OK);
}

void VideoEncoderResource::ReleaseFrames() {
  for (VideoFrameMap::iterator it = video_frames_.begin();
       it != video_frames_.end(); ++it) {
    it->second->Invalidate();
    it->second = nullptr;
  }
  video_frames_.clear();
}

}  // namespace proxy
}  // namespace ppapi
