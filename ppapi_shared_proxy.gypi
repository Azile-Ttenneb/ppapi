# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'ppapi_shared_impl',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
        '../base/base.gyp:base',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'shared_impl/audio_impl.cc',
        'shared_impl/audio_impl.h',
        'shared_impl/image_data_impl.cc',
        'shared_impl/image_data_impl.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_guid': 'E7420D65-A885-41EB-B4BE-04DE0C97033B',
        }],
      ],
    },
    {
      'target_name': 'ppapi_proxy',
      'type': 'static_library',
      'dependencies': [
        '../ipc/ipc.gyp:ipc',
        'ppapi_c',
        'ppapi_shared_impl',
      ],
      'all_dependent_settings': {
        'include_dirs': [
           '..',
        ],
      },
      'include_dirs': [
        '..',
        '../..',  # For nacl includes to work.
      ],
      'sources': [
        'proxy/callback_tracker.cc',
        'proxy/callback_tracker.h',
        'proxy/dispatcher.cc',
        'proxy/dispatcher.h',
        'proxy/host_dispatcher.cc',
        'proxy/host_dispatcher.h',
        'proxy/host_resource.h',
        'proxy/host_var_serialization_rules.cc',
        'proxy/host_var_serialization_rules.h',
        'proxy/interface_proxy.cc',
        'proxy/interface_proxy.h',
        'proxy/plugin_dispatcher.cc',
        'proxy/plugin_dispatcher.h',
        'proxy/plugin_resource.cc',
        'proxy/plugin_resource.h',
        'proxy/plugin_resource_tracker.cc',
        'proxy/plugin_resource_tracker.h',
        'proxy/plugin_var_serialization_rules.cc',
        'proxy/plugin_var_serialization_rules.h',
        'proxy/plugin_var_tracker.cc',
        'proxy/plugin_var_tracker.h',
        'proxy/ppapi_messages.cc',
        'proxy/ppapi_messages.h',
        'proxy/ppapi_messages_internal.h',
        'proxy/ppapi_param_traits.cc',
        'proxy/ppapi_param_traits.h',
        'proxy/ppb_audio_config_proxy.cc',
        'proxy/ppb_audio_config_proxy.h',
        'proxy/ppb_audio_proxy.cc',
        'proxy/ppb_audio_proxy.h',
        'proxy/ppb_buffer_proxy.cc',
        'proxy/ppb_buffer_proxy.h',
        'proxy/ppb_char_set_proxy.cc',
        'proxy/ppb_char_set_proxy.h',
        'proxy/ppb_core_proxy.cc',
        'proxy/ppb_core_proxy.h',
        'proxy/ppb_cursor_control_proxy.cc',
        'proxy/ppb_cursor_control_proxy.h',
        'proxy/ppb_flash_proxy.cc',
        'proxy/ppb_flash_proxy.h',
        'proxy/ppb_font_proxy.cc',
        'proxy/ppb_font_proxy.h',
        'proxy/ppb_fullscreen_proxy.cc',
        'proxy/ppb_fullscreen_proxy.h',
        'proxy/ppb_graphics_2d_proxy.cc',
        'proxy/ppb_graphics_2d_proxy.h',
        'proxy/ppb_image_data_proxy.cc',
        'proxy/ppb_image_data_proxy.h',
        'proxy/ppb_instance_proxy.cc',
        'proxy/ppb_instance_proxy.h',
        'proxy/ppb_pdf_proxy.cc',
        'proxy/ppb_pdf_proxy.h',
        'proxy/ppb_testing_proxy.cc',
        'proxy/ppb_testing_proxy.h',
        'proxy/ppb_url_loader_proxy.cc',
        'proxy/ppb_url_loader_proxy.h',
        'proxy/ppb_url_request_info_proxy.cc',
        'proxy/ppb_url_request_info_proxy.h',
        'proxy/ppb_url_response_info_proxy.cc',
        'proxy/ppb_url_response_info_proxy.h',
        'proxy/ppb_var_deprecated_proxy.cc',
        'proxy/ppb_var_deprecated_proxy.h',
        'proxy/ppp_class_proxy.cc',
        'proxy/ppp_class_proxy.h',
        'proxy/ppp_instance_proxy.cc',
        'proxy/ppp_instance_proxy.h',
        'proxy/serialized_structs.cc',
        'proxy/serialized_structs.h',
        'proxy/serialized_var.cc',
        'proxy/serialized_var.h',
        'proxy/var_serialization_rules.h',
      ],
      'defines': [
      ],
      'conditions': [
        ['OS=="win"', {
        }],
        ['OS=="linux"', {
        }],
        ['OS=="mac"', {
        }]
      ],
    },
  ],
}
