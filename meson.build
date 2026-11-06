project('xwayland-bridge', ['c', 'cpp'],
  version : '0.1.0',
  default_options : ['warning_level=3', 'cpp_std=c++23', 'c_std=c11']
)

wayland_server_dep = dependency('wayland-server', required: true)
xkbcommon_dep = dependency('xkbcommon', required: true)
thread_dep = dependency('threads')
wayland_protocols_dep = dependency('wayland-protocols', version: '>=1.15', required: true)
wayland_scanner = find_program('wayland-scanner', required: true)

subdir('build_common')

inc_dir = include_directories('include', '../../include', '../xserver/include')

src_files = [
  'src/main.cc',
  'src/wayland_bridge.cc',
  'src/xwayland_manager.cc',
  '../xserver/src/ipc_socket.cc',
  '../xserver/src/xbus_message_pool.cc',
]

executable('xwayland-bridge',
  src_files,
  bridge_protocol_srcs,
  include_directories: inc_dir,
  dependencies: [wayland_server_dep, xkbcommon_dep, thread_dep],
  install : true
)
