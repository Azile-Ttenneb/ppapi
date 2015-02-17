// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_IMAGE_CAPTURE_RESOURCE_H_
#define PPAPI_PROXY_IMAGE_CAPTURE_RESOURCE_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_image_capture_api.h"

namespace ppapi {
namespace proxy {

class CameraCapabilitiesResource;
class ImageCaptureConfigResource;

class PPAPI_PROXY_EXPORT ImageCaptureResource
    : public PluginResource,
      public thunk::PPB_ImageCapture_API {
 public:
  ImageCaptureResource(Connection connection, PP_Instance instance);
  ~ImageCaptureResource() override;

  // Resource overrides:
  thunk::PPB_ImageCapture_API* AsPPB_ImageCapture_API() override {
    return this;
  }

  // PPB_ImageCapture_API implementation.
  int32_t Open(PP_Var device_id,
               const scoped_refptr<TrackedCallback>& callback) override;
  void Close() override;
  int32_t GetCameraCapabilities(
      PP_Resource* capabilities,
      const scoped_refptr<TrackedCallback>& callback) override;

 private:
  enum class OpenState { BEFORE_OPEN, OPENED, CLOSED };

  void OnPluginMsgGetPreviewSizesReply(
      PP_Resource* capabilities_output,
      const ResourceMessageReplyParams& params,
      const std::vector<PP_Size>& preview_sizes);
  void OnPluginMsgOpenReply(const ResourceMessageReplyParams& params);

  bool is_opened() const { return open_state_ == OpenState::OPENED; }

  // Holds a reference of the callback so that Close() can cancel it.
  scoped_refptr<TrackedCallback> open_callback_;
  OpenState open_state_;

  scoped_refptr<TrackedCallback> get_capabilities_callback_;
  scoped_refptr<CameraCapabilitiesResource> camera_capabilities_;

  DISALLOW_COPY_AND_ASSIGN(ImageCaptureResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_IMAGE_CAPTURE_RESOURCE_H_
