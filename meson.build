project('xwayland-bridge', ['c', 'cpp'],
  version : '0.1.0',
  default_options : ['warning_level=3', 'cpp_std=c++23', 'c_std=c11']
)

wayland_server_dep = dependency('wayland-server', required: true)
xkbcommon_dep = dependency('xkbcommon', required: true)
thread_dep = dependency('threads')
wayland_protocols_dep = dependency('wayland-protocols', version: '>=1.15', required: true)
wayland_scanner = find_program('wayland-scanner', required: true)

# Define the path to xdg-shell.xml from wayland-protocols
protocols_dir = wayland_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')
xdg_shell_xml      = protocols_dir + '/stable/xdg-shell/xdg-shell.xml'
linux_dmabuf_xml   = protocols_dir + '/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml'
xdg_output_xml          = protocols_dir + '/unstable/xdg-output/xdg-output-unstable-v1.xml'
relative_pointer_xml    = protocols_dir + '/unstable/relative-pointer/relative-pointer-unstable-v1.xml'
pointer_constraints_xml = protocols_dir + '/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml'
xdg_decoration_xml      = protocols_dir + '/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'
presentation_time_xml   = protocols_dir + '/stable/presentation-time/presentation-time.xml'
viewporter_xml       = protocols_dir + '/stable/viewporter/viewporter.xml'
xdg_activation_xml   = protocols_dir + '/staging/xdg-activation/xdg-activation-v1.xml'
single_pixel_buffer_xml  = protocols_dir + '/staging/single-pixel-buffer/single-pixel-buffer-v1.xml'
xdg_dialog_xml           = protocols_dir + '/staging/xdg-dialog/xdg-dialog-v1.xml'
fractional_scale_xml     = protocols_dir + '/staging/fractional-scale/fractional-scale-v1.xml'
cursor_shape_xml         = protocols_dir + '/staging/cursor-shape/cursor-shape-v1.xml'
text_input_v3_xml        = protocols_dir + '/unstable/text-input/text-input-unstable-v3.xml'
idle_inhibit_xml         = protocols_dir + '/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'
security_context_xml     = protocols_dir + '/staging/security-context/security-context-v1.xml'
ext_idle_notify_xml      = protocols_dir + '/staging/ext-idle-notify/ext-idle-notify-v1.xml'

# Generator for wayland-scanner
scanner_header = custom_target(
  'xdg-shell-protocol-header',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

scanner_code = custom_target(
  'xdg-shell-protocol-code',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

dmabuf_header = custom_target(
  'linux-dmabuf-unstable-v1-header',
  input: linux_dmabuf_xml,
  output: 'linux-dmabuf-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

dmabuf_code = custom_target(
  'linux-dmabuf-unstable-v1-code',
  input: linux_dmabuf_xml,
  output: 'linux-dmabuf-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

xdg_output_header = custom_target(
  'xdg-output-unstable-v1-header',
  input: xdg_output_xml,
  output: 'xdg-output-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

xdg_output_code = custom_target(
  'xdg-output-unstable-v1-code',
  input: xdg_output_xml,
  output: 'xdg-output-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

relative_pointer_header = custom_target(
  'relative-pointer-unstable-v1-header',
  input: relative_pointer_xml,
  output: 'relative-pointer-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

relative_pointer_code = custom_target(
  'relative-pointer-unstable-v1-code',
  input: relative_pointer_xml,
  output: 'relative-pointer-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

pointer_constraints_header = custom_target(
  'pointer-constraints-unstable-v1-header',
  input: pointer_constraints_xml,
  output: 'pointer-constraints-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

pointer_constraints_code = custom_target(
  'pointer-constraints-unstable-v1-code',
  input: pointer_constraints_xml,
  output: 'pointer-constraints-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

presentation_time_header = custom_target(
  'presentation-time-header',
  input: presentation_time_xml,
  output: 'presentation-time-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

presentation_time_code = custom_target(
  'presentation-time-code',
  input: presentation_time_xml,
  output: 'presentation-time-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

viewporter_header = custom_target(
  'viewporter-header',
  input: viewporter_xml,
  output: 'viewporter-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

viewporter_code = custom_target(
  'viewporter-code',
  input: viewporter_xml,
  output: 'viewporter-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

xdg_activation_header = custom_target(
  'xdg-activation-v1-header',
  input: xdg_activation_xml,
  output: 'xdg-activation-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

xdg_activation_code = custom_target(
  'xdg-activation-v1-code',
  input: xdg_activation_xml,
  output: 'xdg-activation-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

xdg_decoration_header = custom_target(
  'xdg-decoration-unstable-v1-header',
  input: xdg_decoration_xml,
  output: 'xdg-decoration-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

xdg_decoration_code = custom_target(
  'xdg-decoration-unstable-v1-code',
  input: xdg_decoration_xml,
  output: 'xdg-decoration-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

single_pixel_buffer_header = custom_target(
  'single-pixel-buffer-v1-header',
  input: single_pixel_buffer_xml,
  output: 'single-pixel-buffer-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

single_pixel_buffer_code = custom_target(
  'single-pixel-buffer-v1-code',
  input: single_pixel_buffer_xml,
  output: 'single-pixel-buffer-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

text_input_v3_header = custom_target(
  'text-input-unstable-v3-header',
  input: text_input_v3_xml,
  output: 'text-input-unstable-v3-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

text_input_v3_code = custom_target(
  'text-input-unstable-v3-code',
  input: text_input_v3_xml,
  output: 'text-input-unstable-v3-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

xdg_dialog_header = custom_target(
  'xdg-dialog-v1-header',
  input: xdg_dialog_xml,
  output: 'xdg-dialog-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

xdg_dialog_code = custom_target(
  'xdg-dialog-v1-code',
  input: xdg_dialog_xml,
  output: 'xdg-dialog-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

cursor_shape_header = custom_target(
  'cursor-shape-v1-header',
  input: cursor_shape_xml,
  output: 'cursor-shape-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

cursor_shape_code = custom_target(
  'cursor-shape-v1-code',
  input: cursor_shape_xml,
  output: 'cursor-shape-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

fractional_scale_header = custom_target(
  'fractional-scale-v1-header',
  input: fractional_scale_xml,
  output: 'fractional-scale-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

fractional_scale_code = custom_target(
  'fractional-scale-v1-code',
  input: fractional_scale_xml,
  output: 'fractional-scale-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

idle_inhibit_header = custom_target(
  'idle-inhibit-unstable-v1-header',
  input: idle_inhibit_xml,
  output: 'idle-inhibit-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

idle_inhibit_code = custom_target(
  'idle-inhibit-unstable-v1-code',
  input: idle_inhibit_xml,
  output: 'idle-inhibit-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

security_context_header = custom_target(
  'security-context-v1-header',
  input: security_context_xml,
  output: 'security-context-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

security_context_code = custom_target(
  'security-context-v1-code',
  input: security_context_xml,
  output: 'security-context-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

ext_idle_notify_header = custom_target(
  'ext-idle-notify-v1-header',
  input: ext_idle_notify_xml,
  output: 'ext-idle-notify-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

ext_idle_notify_code = custom_target(
  'ext-idle-notify-v1-code',
  input: ext_idle_notify_xml,
  output: 'ext-idle-notify-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

# wl_drm is bundled with Mesa, not in upstream wayland-protocols
wl_drm_header = custom_target(
  'wayland-drm-header',
  input: files('protocols/wayland-drm.xml'),
  output: 'wayland-drm-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

wl_drm_code = custom_target(
  'wayland-drm-code',
  input: files('protocols/wayland-drm.xml'),
  output: 'wayland-drm-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

# wlr-screencopy is not in upstream wayland-protocols; use bundled XML
screencopy_xml = meson.current_source_dir() + '/protocols/wlr-screencopy-unstable-v1.xml'

screencopy_header = custom_target(
  'wlr-screencopy-unstable-v1-header',
  input: files('protocols/wlr-screencopy-unstable-v1.xml'),
  output: 'wlr-screencopy-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

screencopy_code = custom_target(
  'wlr-screencopy-unstable-v1-code',
  input: files('protocols/wlr-screencopy-unstable-v1.xml'),
  output: 'wlr-screencopy-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

# wlr-layer-shell is not in upstream wayland-protocols; use bundled XML
layer_shell_header = custom_target(
  'wlr-layer-shell-unstable-v1-header',
  input: files('protocols/wlr-layer-shell-unstable-v1.xml'),
  output: 'wlr-layer-shell-unstable-v1-protocol.h',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

layer_shell_code = custom_target(
  'wlr-layer-shell-unstable-v1-code',
  input: files('protocols/wlr-layer-shell-unstable-v1.xml'),
  output: 'wlr-layer-shell-unstable-v1-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

inc_dir = include_directories('include', '../../include', '../server/include')

src_files = [
  'src/main.cc',
  'src/wayland_bridge.cc',
  'src/xwayland_manager.cc',
  '../server/src/ipc_socket.cc',
]

executable('xwayland-bridge',
  src_files,
  scanner_header,
  scanner_code,
  dmabuf_header,
  dmabuf_code,
  xdg_output_header,
  xdg_output_code,
  relative_pointer_header,
  relative_pointer_code,
  pointer_constraints_header,
  pointer_constraints_code,
  screencopy_header,
  screencopy_code,
  xdg_decoration_header,
  xdg_decoration_code,
  presentation_time_header,
  presentation_time_code,
  viewporter_header,
  viewporter_code,
  xdg_activation_header,
  xdg_activation_code,
  single_pixel_buffer_header,
  single_pixel_buffer_code,
  text_input_v3_header,
  text_input_v3_code,
  wl_drm_header,
  wl_drm_code,
  idle_inhibit_header,
  idle_inhibit_code,
  layer_shell_header,
  layer_shell_code,
  xdg_dialog_header,
  xdg_dialog_code,
  fractional_scale_header,
  fractional_scale_code,
  cursor_shape_header,
  cursor_shape_code,
  security_context_header,
  security_context_code,
  ext_idle_notify_header,
  ext_idle_notify_code,
  include_directories: inc_dir,
  dependencies: [wayland_server_dep, xkbcommon_dep, thread_dep],
  install : true
)
