#include "xwayland-bridge/wayland_bridge.h"
#include "xdg-shell-protocol.h"
#include "single-pixel-buffer-v1-protocol.h"
#include "wayland-drm-protocol.h"
#include "idle-inhibit-unstable-v1-protocol.h"
// wlr-layer-shell uses 'namespace' as a parameter name, which is a C++ keyword.
#define namespace wlr_namespace
#include "wlr-layer-shell-unstable-v1-protocol.h"
#undef namespace
#include "xdg-dialog-v1-protocol.h"
#include "fractional-scale-v1-protocol.h"
#include "cursor-shape-v1-protocol.h"
#include "text-input-unstable-v3-protocol.h"
#include "linux-dmabuf-unstable-v1-protocol.h"
#include "xdg-output-unstable-v1-protocol.h"
#include "relative-pointer-unstable-v1-protocol.h"
#include "pointer-constraints-unstable-v1-protocol.h"
#include "wlr-screencopy-unstable-v1-protocol.h"
#include "xdg-decoration-unstable-v1-protocol.h"
#include "presentation-time-protocol.h"
#include "viewporter-protocol.h"
#include "xdg-activation-v1-protocol.h"
#include "security-context-v1-protocol.h"
#include "ext-idle-notify-v1-protocol.h"
#include <xkbcommon/xkbcommon.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

namespace fbx::xwayland {

// ============================================================
// Interface implementation tables
// ============================================================

static const struct wl_compositor_interface compositor_implementation = {
    .create_surface = WaylandBridge::createSurface,
    .create_region  = nullptr
};

static const struct wl_shm_pool_interface shm_pool_implementation = {
    .create_buffer = WaylandBridge::shmPoolCreateBuffer,
    .destroy       = WaylandBridge::shmPoolDestroy,
    .resize        = WaylandBridge::shmPoolResize
};

static const struct wl_shm_interface shm_implementation = {
    .create_pool = WaylandBridge::shmCreatePool
};

static const struct wl_surface_interface surface_implementation = {
    .destroy              = WaylandBridge::surfaceDestroy,
    .attach               = WaylandBridge::surfaceAttach,
    .damage               = nullptr,
    .frame                = WaylandBridge::surfaceFrame,
    .set_opaque_region    = nullptr,
    .set_input_region     = nullptr,
    .commit               = WaylandBridge::surfaceCommit,
    .set_buffer_transform = nullptr,
    .set_buffer_scale     = nullptr,
    .damage_buffer        = nullptr
};

static const struct zwp_linux_buffer_params_v1_interface dmabuf_params_implementation = {
    .destroy       = WaylandBridge::dmabufParamsDestroy,
    .add           = WaylandBridge::dmabufParamsAdd,
    .create        = WaylandBridge::dmabufParamsCreate,
    .create_immed  = WaylandBridge::dmabufParamsCreateImmed
};

static const struct zwp_linux_dmabuf_v1_interface dmabuf_implementation = {
    .destroy              = WaylandBridge::dmabufDestroy,
    .create_params        = WaylandBridge::dmabufCreateParams,
    .get_default_feedback = WaylandBridge::dmabufGetDefaultFeedback,
    .get_surface_feedback = WaylandBridge::dmabufGetSurfaceFeedback
};

static const struct zwp_linux_dmabuf_feedback_v1_interface dmabuf_feedback_implementation = {
    .destroy = WaylandBridge::dmabufFeedbackDestroy
};

static const struct zwp_idle_inhibit_manager_v1_interface idle_inhibit_manager_implementation = {
    .destroy           = WaylandBridge::idleInhibitManagerDestroy,
    .create_inhibitor  = WaylandBridge::idleInhibitManagerCreateInhibitor
};

static const struct wp_security_context_manager_v1_interface security_context_manager_implementation = {
    .destroy         = WaylandBridge::securityContextManagerDestroy,
    .create_listener = WaylandBridge::securityContextManagerCreateListener,
};

static const struct wp_security_context_v1_interface security_context_implementation = {
    .destroy            = WaylandBridge::securityContextDestroy,
    .set_sandbox_engine = WaylandBridge::securityContextSetSandboxEngine,
    .set_app_id         = WaylandBridge::securityContextSetAppId,
    .set_instance_id    = WaylandBridge::securityContextSetInstanceId,
    .commit             = WaylandBridge::securityContextCommit,
};

static const struct wl_drm_interface wl_drm_implementation = {
    .authenticate        = WaylandBridge::wlDrmAuthenticate,
    .create_buffer       = WaylandBridge::wlDrmCreateBuffer,
    .create_planar_buffer = WaylandBridge::wlDrmCreatePlanarBuffer,
    .create_prime_buffer = WaylandBridge::wlDrmCreatePrimeBuffer
};

static const struct zwlr_layer_shell_v1_interface layer_shell_implementation = {
    .get_layer_surface = WaylandBridge::layerShellGetLayerSurface,
    .destroy           = WaylandBridge::layerShellDestroy
};

static const struct zwlr_layer_surface_v1_interface layer_surface_implementation = {
    .set_size                   = WaylandBridge::layerSurfaceSetSize,
    .set_anchor                 = WaylandBridge::layerSurfaceSetAnchor,
    .set_exclusive_zone         = WaylandBridge::layerSurfaceSetExclusiveZone,
    .set_margin                 = WaylandBridge::layerSurfaceSetMargin,
    .set_keyboard_interactivity = WaylandBridge::layerSurfaceSetKeyboardInteractivity,
    .get_popup                  = WaylandBridge::layerSurfaceGetPopup,
    .ack_configure              = WaylandBridge::layerSurfaceAckConfigure,
    .destroy                    = WaylandBridge::layerSurfaceDestroy
};

static const struct wp_cursor_shape_manager_v1_interface cursor_shape_manager_implementation = {
    .destroy           = WaylandBridge::cursorShapeManagerDestroy,
    .get_pointer       = WaylandBridge::cursorShapeManagerGetPointer,
    .get_tablet_tool_v2 = WaylandBridge::cursorShapeManagerGetTabletToolV2
};

static const struct wp_cursor_shape_device_v1_interface cursor_shape_device_implementation = {
    .destroy   = WaylandBridge::cursorShapeDeviceDestroy,
    .set_shape = WaylandBridge::cursorShapeDeviceSetShape
};

static const struct wp_fractional_scale_manager_v1_interface fractional_scale_manager_implementation = {
    .destroy              = WaylandBridge::fractionalScaleManagerDestroy,
    .get_fractional_scale = WaylandBridge::fractionalScaleManagerGetFractionalScale
};

static const struct wp_fractional_scale_v1_interface fractional_scale_implementation = {
    .destroy = WaylandBridge::fractionalScaleDestroy
};

static const struct xdg_wm_dialog_v1_interface xdg_wm_dialog_implementation = {
    .destroy         = WaylandBridge::wmDialogDestroy,
    .get_xdg_dialog  = WaylandBridge::wmDialogGetXdgDialog
};

static const struct xdg_dialog_v1_interface xdg_dialog_implementation = {
    .destroy     = WaylandBridge::xdgDialogDestroy,
    .set_modal   = WaylandBridge::xdgDialogSetModal,
    .unset_modal = WaylandBridge::xdgDialogUnsetModal
};

static const struct xdg_positioner_interface positioner_implementation = {
    .destroy                   = WaylandBridge::positonerDestroy,
    .set_size                  = WaylandBridge::positionerSetSize,
    .set_anchor_rect           = WaylandBridge::positionerSetAnchorRect,
    .set_anchor                = WaylandBridge::positionerSetAnchor,
    .set_gravity               = WaylandBridge::positionerSetGravity,
    .set_constraint_adjustment = WaylandBridge::positionerSetConstraintAdjustment,
    .set_offset                = WaylandBridge::positionerSetOffset,
    .set_reactive              = WaylandBridge::positionerSetReactive,
    .set_parent_size           = WaylandBridge::positionerSetParentSize,
    .set_parent_configure      = WaylandBridge::positionerSetParentConfigure
};

static const struct xdg_popup_interface xdg_popup_implementation = {
    .destroy    = WaylandBridge::xdgPopupDestroy,
    .grab       = WaylandBridge::xdgPopupGrab,
    .reposition = WaylandBridge::xdgPopupReposition
};

static const struct xdg_wm_base_interface xdg_wm_base_implementation = {
    .destroy           = [](wl_client*, wl_resource* r) { wl_resource_destroy(r); },
    .create_positioner = WaylandBridge::xdgWmBaseCreatePositioner,
    .get_xdg_surface   = WaylandBridge::xdgWmBaseGetXdgSurface,
    .pong              = WaylandBridge::xdgWmBasePong
};

static const struct xdg_surface_interface xdg_surface_implementation = {
    .destroy             = [](wl_client*, wl_resource* r) { wl_resource_destroy(r); },
    .get_toplevel        = WaylandBridge::xdgSurfaceGetToplevel,
    .get_popup           = WaylandBridge::xdgSurfaceGetPopup,
    .set_window_geometry = WaylandBridge::xdgSurfaceSetWindowGeometry,
    .ack_configure       = WaylandBridge::xdgSurfaceAckConfigure
};

static const struct xdg_toplevel_interface xdg_toplevel_implementation = {
    .destroy          = [](wl_client*, wl_resource* r) { wl_resource_destroy(r); },
    .set_parent       = WaylandBridge::xdgToplevelSetParent,
    .set_title        = WaylandBridge::xdgToplevelSetTitle,
    .set_app_id       = WaylandBridge::xdgToplevelSetAppId,
    .show_window_menu = nullptr,
    .move             = nullptr,
    .resize           = WaylandBridge::xdgToplevelResize,
    .set_max_size     = nullptr,
    .set_min_size     = nullptr,
    .set_maximized    = nullptr,
    .unset_maximized  = nullptr,
    .set_fullscreen   = nullptr,
    .unset_fullscreen = nullptr,
    .set_minimized    = nullptr
};

static const struct wl_seat_interface seat_implementation = {
    .get_pointer  = WaylandBridge::seatGetPointer,
    .get_keyboard = WaylandBridge::seatGetKeyboard,
    .get_touch    = WaylandBridge::seatGetTouch,
    .release      = WaylandBridge::seatRelease
};

static const struct wl_pointer_interface pointer_implementation = {
    .set_cursor = [](wl_client*, wl_resource*, uint32_t, wl_resource*,
                     int32_t, int32_t) noexcept {},
    .release    = [](wl_client*, wl_resource* r) noexcept { wl_resource_destroy(r); }
};

static const struct wl_keyboard_interface keyboard_implementation = {
    .release = [](wl_client*, wl_resource* r) noexcept { wl_resource_destroy(r); }
};

static const struct wl_touch_interface touch_implementation = {
    .release = [](wl_client*, wl_resource* r) noexcept { wl_resource_destroy(r); }
};

static const struct wl_output_interface output_implementation = {
    .release = [](wl_client*, wl_resource* r) noexcept { wl_resource_destroy(r); }
};

static const struct zxdg_output_v1_interface xdg_output_implementation = {
    .destroy = WaylandBridge::xdgOutputDestroy
};

static const struct zxdg_output_manager_v1_interface xdg_output_manager_implementation = {
    .destroy        = WaylandBridge::xdgOutputManagerDestroy,
    .get_xdg_output = WaylandBridge::xdgOutputManagerGetXdgOutput
};

static const struct zwp_relative_pointer_v1_interface relative_pointer_implementation = {
    .destroy = WaylandBridge::relativePointerDestroy
};

static const struct zwp_relative_pointer_manager_v1_interface relative_pointer_manager_implementation = {
    .destroy              = WaylandBridge::relativePointerManagerDestroy,
    .get_relative_pointer = WaylandBridge::relativePointerManagerGetRelativePointer
};

static const struct zwp_locked_pointer_v1_interface locked_pointer_implementation = {
    .destroy                   = WaylandBridge::lockedPointerDestroy,
    .set_cursor_position_hint  = WaylandBridge::lockedPointerSetCursorPositionHint,
    .set_region                = WaylandBridge::lockedPointerSetRegion
};

static const struct zwp_confined_pointer_v1_interface confined_pointer_implementation = {
    .destroy    = WaylandBridge::confinedPointerDestroy,
    .set_region = WaylandBridge::confinedPointerSetRegion
};

static const struct zwp_pointer_constraints_v1_interface pointer_constraints_implementation = {
    .destroy          = WaylandBridge::pointerConstraintsDestroy,
    .lock_pointer     = WaylandBridge::pointerConstraintsLockPointer,
    .confine_pointer  = WaylandBridge::pointerConstraintsConfinePointer
};

static const struct wp_viewport_interface viewport_implementation = {
    .destroy         = WaylandBridge::viewportDestroy,
    .set_source      = WaylandBridge::viewportSetSource,
    .set_destination = WaylandBridge::viewportSetDestination
};

static const struct wp_viewporter_interface viewporter_implementation = {
    .destroy      = WaylandBridge::viewporterDestroy,
    .get_viewport = WaylandBridge::viewporterGetViewport
};

static const struct xdg_activation_token_v1_interface activation_token_implementation = {
    .set_serial  = WaylandBridge::activationTokenSetSerial,
    .set_app_id  = WaylandBridge::activationTokenSetAppId,
    .set_surface = WaylandBridge::activationTokenSetSurface,
    .commit      = WaylandBridge::activationTokenCommit,
    .destroy     = WaylandBridge::activationTokenDestroy
};

static const struct xdg_activation_v1_interface activation_implementation = {
    .destroy           = WaylandBridge::activationDestroy,
    .get_activation_token = WaylandBridge::activationGetToken,
    .activate          = WaylandBridge::activationActivate
};

static const struct wl_subsurface_interface subsurface_implementation = {
    .destroy      = WaylandBridge::subsurfaceDestroy,
    .set_position = WaylandBridge::subsurfaceSetPosition,
    .place_above  = WaylandBridge::subsurfacePlaceAbove,
    .place_below  = WaylandBridge::subsurfacePlaceBelow,
    .set_sync     = WaylandBridge::subsurfaceSetSync,
    .set_desync   = WaylandBridge::subsurfaceSetDesync
};

static const struct wl_subcompositor_interface subcompositor_implementation = {
    .destroy        = WaylandBridge::subcompositorDestroy,
    .get_subsurface = WaylandBridge::subcompositorGetSubsurface
};

static const struct wp_single_pixel_buffer_manager_v1_interface spbm_implementation = {
    .destroy               = WaylandBridge::spbmDestroy,
    .create_u32_rgba_buffer = WaylandBridge::spbmCreateU32RgbaBuffer
};

static const struct zwp_text_input_v3_interface text_input_v3_implementation = {
    .destroy                 = WaylandBridge::textInputDestroy,
    .enable                  = WaylandBridge::textInputEnable,
    .disable                 = WaylandBridge::textInputDisable,
    .set_surrounding_text    = WaylandBridge::textInputSetSurroundingText,
    .set_text_change_cause   = WaylandBridge::textInputSetTextChangeCause,
    .set_content_type        = WaylandBridge::textInputSetContentType,
    .set_cursor_rectangle    = WaylandBridge::textInputSetCursorRectangle,
    .commit                  = WaylandBridge::textInputCommit
};

static const struct zwp_text_input_manager_v3_interface text_input_manager_v3_implementation = {
    .destroy         = WaylandBridge::textInputManagerDestroy,
    .get_text_input  = WaylandBridge::textInputManagerGetTextInput
};

static const struct wl_data_offer_interface data_offer_implementation = {
    .accept      = WaylandBridge::dataOfferAccept,
    .receive     = WaylandBridge::dataOfferReceive,
    .destroy     = WaylandBridge::dataOfferDestroy,
    .finish      = WaylandBridge::dataOfferFinish,
    .set_actions = WaylandBridge::dataOfferSetActions
};

static const struct wl_data_source_interface data_source_implementation = {
    .offer       = WaylandBridge::dataSourceOffer,
    .destroy     = WaylandBridge::dataSourceDestroy,
    .set_actions = WaylandBridge::dataSourceSetActions
};

static const struct wl_data_device_interface data_device_implementation = {
    .start_drag    = WaylandBridge::dataDeviceStartDrag,
    .set_selection = WaylandBridge::dataDeviceSetSelection,
    .release       = WaylandBridge::dataDeviceRelease
};

static const struct wl_data_device_manager_interface data_device_manager_implementation = {
    .create_data_source = WaylandBridge::dataDeviceManagerCreateDataSource,
    .get_data_device    = WaylandBridge::dataDeviceManagerGetDataDevice
};

static const struct wp_presentation_interface presentation_implementation = {
    .destroy  = WaylandBridge::presentationDestroy,
    .feedback = WaylandBridge::presentationFeedback
};

static const struct zxdg_toplevel_decoration_v1_interface toplevel_decoration_implementation = {
    .destroy    = WaylandBridge::toplevelDecorationDestroy,
    .set_mode   = WaylandBridge::toplevelDecorationSetMode,
    .unset_mode = WaylandBridge::toplevelDecorationUnsetMode
};

static const struct zxdg_decoration_manager_v1_interface decoration_manager_implementation = {
    .destroy                 = WaylandBridge::decorationManagerDestroy,
    .get_toplevel_decoration = WaylandBridge::decorationManagerGetToplevelDecoration
};

static const struct zwlr_screencopy_frame_v1_interface screencopy_frame_implementation = {
    .copy             = WaylandBridge::screencopyFrameCopy,
    .destroy          = WaylandBridge::screencopyFrameDestroy,
    .copy_with_damage = WaylandBridge::screencopyFrameCopyWithDamage
};

static const struct zwlr_screencopy_manager_v1_interface screencopy_manager_implementation = {
    .capture_output        = WaylandBridge::screencopyManagerCaptureOutput,
    .capture_output_region = WaylandBridge::screencopyManagerCaptureOutputRegion,
    .destroy               = WaylandBridge::screencopyManagerDestroy
};

// ============================================================
// WlDisplayDeleter / lifecycle
// ============================================================

void WlDisplayDeleter::operator()(wl_display* display) const noexcept {
    if (display) {
        wl_display_destroy(display);
        std::cout << "[xwayland-bridge] Wayland display destroyed.\n";
    }
}

WaylandBridge::~WaylandBridge() noexcept {
    if (keymap_fd_ >= 0) ::close(keymap_fd_);
    if (dmabuf_feedback_table_fd_ >= 0) ::close(dmabuf_feedback_table_fd_);
}

// ============================================================
// setupKeymap() — compile XKB keymap into anonymous shm
// ============================================================

std::expected<void, BridgeError> WaylandBridge::setupKeymap() noexcept {
    xkb_context* ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!ctx) return {}; // non-fatal — clients fall back to no-keymap

    const xkb_rule_names names{
        .rules   = "evdev",
        .model   = "pc105",
        .layout  = "us",
        .variant = nullptr,
        .options = nullptr
    };
    xkb_keymap* keymap = xkb_keymap_new_from_names(ctx, &names,
                                                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) { xkb_context_unref(ctx); return {}; }

    char* str = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
    if (str) {
        size_t len = std::strlen(str) + 1; // include NUL terminator
        int fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
        if (fd >= 0 && ftruncate(fd, static_cast<off_t>(len)) == 0) {
            void* m = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (m != MAP_FAILED) {
                std::memcpy(m, str, len);
                munmap(m, len);
                keymap_fd_   = fd;
                keymap_size_ = len;
            } else {
                close(fd);
            }
        }
        free(str);
    }

    // Cache XKB modifier indices for use in handleInputMessage
    xkb_mod_shift_ = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
    xkb_mod_ctrl_  = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CTRL);
    xkb_mod_alt_   = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_ALT);
    xkb_mod_logo_  = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_LOGO);

    xkb_keymap_unref(keymap);
    xkb_context_unref(ctx);
    std::cout << "[xwayland-bridge] XKB keymap loaded (fd=" << keymap_fd_
              << ", size=" << keymap_size_ << ")\n";
    return {};
}

// ============================================================
// Factory
// ============================================================

std::expected<std::unique_ptr<WaylandBridge>, BridgeError> WaylandBridge::create() noexcept {
    auto bridge = std::unique_ptr<WaylandBridge>(new WaylandBridge());
    if (auto res = bridge->init();     !res.has_value()) return std::unexpected(res.error());
    if (auto res = bridge->setupIpc(); !res.has_value()) return std::unexpected(res.error());
    return bridge;
}

// ============================================================
// init() — register Wayland globals
// ============================================================

std::expected<void, BridgeError> WaylandBridge::init() noexcept {
    display_.reset(wl_display_create());
    if (!display_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    (void)setupKeymap();

    event_loop_ = wl_display_get_event_loop(display_.get());

    compositor_global_ = wl_global_create(display_.get(), &wl_compositor_interface,
                                          4, this, WaylandBridge::bindCompositor);
    if (!compositor_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    if (!wl_global_create(display_.get(), &wl_shm_interface, 1, this, WaylandBridge::bindShm))
        return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    xdg_shell_global_ = wl_global_create(display_.get(), &xdg_wm_base_interface,
                                          1, this, WaylandBridge::bindXdgShell);
    if (!xdg_shell_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    initDmabufFeedback();

    dmabuf_global_ = wl_global_create(display_.get(), &zwp_linux_dmabuf_v1_interface,
                                       4, this, WaylandBridge::bindLinuxDmabuf);
    if (!dmabuf_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wl_seat v5 — keyboard + pointer (no touch)
    seat_global_ = wl_global_create(display_.get(), &wl_seat_interface,
                                     5, this, WaylandBridge::bindSeat);
    if (!seat_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zxdg_output_manager_v1 — allows clients to query logical output geometry
    xdg_output_manager_global_ = wl_global_create(display_.get(),
        &zxdg_output_manager_v1_interface, 3, this, WaylandBridge::bindXdgOutputManager);
    if (!xdg_output_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zwp_relative_pointer_manager_v1 — relative mouse motion for games / 3D viewports
    relative_pointer_manager_global_ = wl_global_create(display_.get(),
        &zwp_relative_pointer_manager_v1_interface, 1, this,
        WaylandBridge::bindRelativePointerManager);
    if (!relative_pointer_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zwp_pointer_constraints_v1 — pointer lock and confinement
    pointer_constraints_global_ = wl_global_create(display_.get(),
        &zwp_pointer_constraints_v1_interface, 1, this,
        WaylandBridge::bindPointerConstraints);
    if (!pointer_constraints_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zwlr_screencopy_manager_v1 — screen capture for recording/streaming
    screencopy_manager_global_ = wl_global_create(display_.get(),
        &zwlr_screencopy_manager_v1_interface, 3, this,
        WaylandBridge::bindScreencopyManager);
    if (!screencopy_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zxdg_decoration_manager_v1 — server-side window decorations
    decoration_manager_global_ = wl_global_create(display_.get(),
        &zxdg_decoration_manager_v1_interface, 1, this,
        WaylandBridge::bindDecorationManager);
    if (!decoration_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wp_presentation — presentation-time feedback (vsync timing for clients)
    presentation_global_ = wl_global_create(display_.get(),
        &wp_presentation_interface, 1, this,
        WaylandBridge::bindPresentation);
    if (!presentation_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wl_data_device_manager — clipboard and drag-and-drop between Wayland clients
    data_device_manager_global_ = wl_global_create(display_.get(),
        &wl_data_device_manager_interface, 3, this,
        WaylandBridge::bindDataDeviceManager);
    if (!data_device_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wp_viewporter — buffer scaling and cropping
    viewporter_global_ = wl_global_create(display_.get(),
        &wp_viewporter_interface, 1, this, WaylandBridge::bindViewporter);
    if (!viewporter_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // xdg_activation_v1 — cross-client window focus requests
    activation_global_ = wl_global_create(display_.get(),
        &xdg_activation_v1_interface, 1, this, WaylandBridge::bindActivation);
    if (!activation_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wl_subcompositor — subsurface (layered child surface) support
    subcompositor_global_ = wl_global_create(display_.get(),
        &wl_subcompositor_interface, 1, this, WaylandBridge::bindSubcompositor);
    if (!subcompositor_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zwp_idle_inhibit_manager_v1 — prevent display idle/screensaver while active
    idle_inhibit_manager_global_ = wl_global_create(display_.get(),
        &zwp_idle_inhibit_manager_v1_interface, 1, this,
        WaylandBridge::bindIdleInhibitManager);
    if (!idle_inhibit_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wl_drm — Mesa legacy EGL DRM buffer sharing
    wl_drm_global_ = wl_global_create(display_.get(),
        &wl_drm_interface, 2, this, WaylandBridge::bindWlDrm);
    if (!wl_drm_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wp_single_pixel_buffer_manager_v1 — CPU-side 1×1 solid-color buffers
    single_pixel_buffer_manager_global_ = wl_global_create(display_.get(),
        &wp_single_pixel_buffer_manager_v1_interface, 1, this,
        WaylandBridge::bindSinglePixelBufferManager);
    if (!single_pixel_buffer_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zwp_text_input_manager_v3 — text input / IME protocol
    text_input_manager_global_ = wl_global_create(display_.get(),
        &zwp_text_input_manager_v3_interface, 1, this,
        WaylandBridge::bindTextInputManager);
    if (!text_input_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // zwlr_layer_shell_v1 — desktop component surfaces (panels, wallpapers, notifications, overlays)
    layer_shell_global_ = wl_global_create(display_.get(),
        &zwlr_layer_shell_v1_interface, 4, this,
        WaylandBridge::bindLayerShell);
    if (!layer_shell_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // xdg_wm_dialog_v1 — modal dialog hint protocol
    xdg_dialog_global_ = wl_global_create(display_.get(),
        &xdg_wm_dialog_v1_interface, 1, this,
        WaylandBridge::bindXdgWmDialog);
    if (!xdg_dialog_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wp_cursor_shape_manager_v1 — named cursor shapes
    cursor_shape_manager_global_ = wl_global_create(display_.get(),
        &wp_cursor_shape_manager_v1_interface, 2, this,
        WaylandBridge::bindCursorShapeManager);
    if (!cursor_shape_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wp_fractional_scale_manager_v1 — preferred fractional scale hints
    fractional_scale_manager_global_ = wl_global_create(display_.get(),
        &wp_fractional_scale_manager_v1_interface, 1, this,
        WaylandBridge::bindFractionalScaleManager);
    if (!fractional_scale_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    security_context_manager_global_ = wl_global_create(
        display_.get(), &wp_security_context_manager_v1_interface, 1,
        this, WaylandBridge::bindSecurityContextManager);
    if (!security_context_manager_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // ext_idle_notifier_v1 — idle detection for screen savers and idle-aware apps
    ext_idle_notifier_global_ = wl_global_create(display_.get(),
        &ext_idle_notifier_v1_interface, 1, this,
        WaylandBridge::bindExtIdleNotifier);
    if (!ext_idle_notifier_global_) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    // wl_output — one global per physical output; start with a single 1920×1080 default.
    shared::DisplayOutputInfo default_out{};
    default_out.index_      = 0;
    default_out.width_      = screen_width_;
    default_out.height_     = screen_height_;
    default_out.refresh_hz_ = 60;
    default_out.x_offset_   = 0.0f;
    std::strncpy(default_out.name_, "Output-0", sizeof(default_out.name_) - 1);
    setOutputLayout({default_out});
    if (output_globals_.empty()) return std::unexpected(BridgeError::DISPLAY_CREATION_ERROR);

    const char* socket = wl_display_add_socket_auto(display_.get());
    if (!socket) return std::unexpected(BridgeError::SOCKET_CREATION_ERROR);
    socket_name_ = socket;

    setenv("WAYLAND_DISPLAY", socket_name_.c_str(), 1);
    std::cout << "[xwayland-bridge] Listening on: " << socket_name_ << "\n";
    return {};
}

// ============================================================
// setupIpc() — connect to freebsd-x compositor
// ============================================================

std::expected<void, BridgeError> WaylandBridge::setupIpc() noexcept {
    xserver_client_ = std::make_unique<xserver::IpcSocket>("/tmp/freebsd-x.sock");
    if (xserver_client_->connect()) {
        ipc_connected_ = true;
        std::cout << "[xwayland-bridge] Connected to FreeBSD-X compositor.\n";
    } else {
        std::cerr << "[xwayland-bridge] Warning: compositor unavailable — offline mode.\n";
    }
    return {};
}

// ============================================================
// IPC receive — called from main loop when data arrives
// ============================================================

void WaylandBridge::handleIpcMessage(const std::vector<uint8_t>& data, int fd) noexcept {
    if (data.size() < 4) return;

    // Distinguish XBusMessage (magic = "XBUS") from RenderMessage by the first 4 bytes.
    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), sizeof(magic));

    if (magic == shared::XBUS_MAGIC) {
        if (data.size() < sizeof(shared::XBusMessage)) return;
        const auto* xmsg = reinterpret_cast<const shared::XBusMessage*>(data.data());

        if (std::strcmp(xmsg->service_name_, "com.freebsdx.Display") == 0 &&
            xmsg->opcode_ == static_cast<uint32_t>(shared::DisplayOpcode::OutputChanged)) {
            // Payload carries the full DisplayOutputList — sync globals to the new layout.
            shared::DisplayOutputList list{};
            std::memcpy(&list, xmsg->payload_, sizeof(list));
            std::vector<shared::DisplayOutputInfo> infos(
                list.outputs_, list.outputs_ + list.count_);
            setOutputLayout(infos);
        } else if (std::strcmp(xmsg->service_name_, "com.freebsdx.Screencopy") == 0) {
            // Response to a CaptureOutput request — fd is the shm with pixel data.
            if (!pending_screencopy_.has_value()) { if (fd >= 0) ::close(fd); return; }
            auto& sc = pending_screencopy_.value();

            shared::ScreencopyFrameInfo info{};
            std::memcpy(&info, xmsg->payload_, sizeof(info));

            if (info.width_ == 0 || fd < 0) {
                // Capture failed
                zwlr_screencopy_frame_v1_send_failed(sc.frame_resource);
            } else {
                size_t src_size = static_cast<size_t>(info.stride_) * info.height_;
                size_t dst_size = static_cast<size_t>(sc.copy_stride) * sc.copy_height;
                void* src = mmap(nullptr, src_size, PROT_READ, MAP_SHARED, fd, 0);
                if (src != MAP_FAILED) {
                    void* dst = mmap(nullptr, dst_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                     sc.client_shm_fd, sc.client_shm_offset);
                    if (dst != MAP_FAILED) {
                        std::memcpy(dst, src, std::min(src_size, dst_size));
                        munmap(dst, dst_size);
                    }
                    munmap(src, src_size);
                }

                struct timespec ts{};
                clock_gettime(CLOCK_MONOTONIC, &ts);
                zwlr_screencopy_frame_v1_send_ready(sc.frame_resource,
                    static_cast<uint32_t>(static_cast<uint64_t>(ts.tv_sec) >> 32),
                    static_cast<uint32_t>(ts.tv_sec),
                    static_cast<uint32_t>(ts.tv_nsec));
                wl_display_flush_clients(display_.get());
            }

            if (fd >= 0) ::close(fd);
            if (sc.client_shm_fd >= 0) ::close(sc.client_shm_fd);
            pending_screencopy_.reset();
        }
        return;
    }

    // InputMessages from the xserver (20 bytes, first 4 bytes = InputEventType ≠ XBUS_MAGIC)
    if (data.size() == sizeof(shared::InputMessage)) {
        handleInputMessage(*reinterpret_cast<const shared::InputMessage*>(data.data()));
        return;
    }

    // DRM page-flip timestamps for wp_presentation (28 bytes — distinct from InputMessage/RenderMessage)
    if (data.size() == sizeof(shared::PresentFeedback)) {
        const auto* fb = reinterpret_cast<const shared::PresentFeedback*>(data.data());
        for (auto& [sid, feedbacks] : pending_feedbacks_) {
            for (wl_resource* res : feedbacks) {
                if (!per_output_resources_.empty() && !per_output_resources_[0].empty())
                    wp_presentation_feedback_send_sync_output(res, per_output_resources_[0][0]);
                wp_presentation_feedback_send_presented(res,
                    fb->tv_sec_hi_, fb->tv_sec_lo_, fb->tv_nsec_, fb->refresh_ns_,
                    fb->seq_hi_, fb->seq_lo_,
                    WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
                    WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK |
                    WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION);
                wl_resource_destroy(res);
            }
        }
        pending_feedbacks_.clear();

        // Fire wl_surface.frame() callbacks with the flip timestamp (milliseconds).
        // tv_sec_lo is seconds, tv_nsec_ is nanoseconds — combine into ms for done().
        uint32_t ts_ms = fb->tv_sec_lo_ * 1000u + fb->tv_nsec_ / 1000000u;
        for (wl_resource* cb : pending_frame_callbacks_) {
            wl_callback_send_done(cb, ts_ms);
            wl_resource_destroy(cb);
        }
        pending_frame_callbacks_.clear();

        wl_display_flush_clients(display_.get());
        return;
    }

    if (data.size() < sizeof(shared::RenderMessage)) return;
    const auto* msg = reinterpret_cast<const shared::RenderMessage*>(data.data());

    if (msg->status_ == shared::RenderMessageStatus::SurfaceReady) {
        // Compositor reports the real screen size for this surface
        screen_width_  = msg->width_;
        screen_height_ = msg->height_;

        auto it = surface_resources_.find(msg->surface_id_);
        if (it != surface_resources_.end()) {
            sendConfigure(it->second,
                static_cast<int32_t>(screen_width_),
                static_cast<int32_t>(screen_height_));
        }
        std::cout << "[xwayland-bridge] SurfaceReady: "
                  << screen_width_ << "x" << screen_height_ << "\n";
    } else if (msg->status_ == shared::RenderMessageStatus::ConfigureRequest) {
        auto it = surface_resources_.find(msg->surface_id_);
        if (it != surface_resources_.end())
            sendConfigure(it->second,
                          static_cast<int32_t>(msg->width_),
                          static_cast<int32_t>(msg->height_));
    }
}


int WaylandBridge::ipcFd() const noexcept {
    return ipc_connected_ ? xserver_client_->fd() : -1;
}

// ============================================================
// sendConfigure — send toplevel+surface configure to unblock a client
// ============================================================

void WaylandBridge::sendConfigure(wl_resource* surface_resource,
                                   int32_t w, int32_t h) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(surface_resource));
    if (!state || !state->xdg_toplevel || !state->xdg_surface) return;

    struct wl_array states_arr;
    wl_array_init(&states_arr);
    auto* activated = static_cast<uint32_t*>(wl_array_add(&states_arr, sizeof(uint32_t)));
    *activated = XDG_TOPLEVEL_STATE_ACTIVATED;

    xdg_toplevel_send_configure(state->xdg_toplevel, w, h, &states_arr);
    wl_array_release(&states_arr);
    xdg_surface_send_configure(state->xdg_surface, configure_serial_++);
}

// ============================================================
// wl_compositor
// ============================================================

void WaylandBridge::bindCompositor(wl_client* client, void* data,
                                    uint32_t version, uint32_t id) noexcept {
    wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, version, id);
    wl_resource_set_implementation(resource, &compositor_implementation, data, nullptr);
    std::cout << "[xwayland-bridge] Client bound wl_compositor.\n";
}

void WaylandBridge::createSurface(wl_client* client, wl_resource* resource,
                                   uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(resource));
    wl_resource* surface_resource = wl_resource_create(
        client, &wl_surface_interface, wl_resource_get_version(resource), id);

    auto* state = new SurfaceState{ .id = self->next_surface_id_++, .bridge = self };

    // Destructor: remove from map + free state
    wl_resource_set_implementation(surface_resource, &surface_implementation, state,
        [](wl_resource* res) {
            auto* st = static_cast<SurfaceState*>(wl_resource_get_user_data(res));
            if (st->bridge) {
                st->bridge->surface_resources_.erase(st->id);
                if (st->bridge->keyboard_focus_surface_ == res)
                    st->bridge->keyboard_focus_surface_ = nullptr;
                // Discard any pending frame callbacks so we don't fire them after destroy
                for (wl_resource* cb : st->frame_callbacks)
                    wl_resource_destroy(cb);
                auto& pfc = st->bridge->pending_frame_callbacks_;
                // pending_frame_callbacks_ holds bare pointers — they become dangling on
                // client disconnect so purge them.  In practice this only happens when the
                // entire client disconnects (at which point wl_display cleans up all
                // resources anyway), but be defensive.
                pfc.clear();
            }
            delete st;
        });

    self->surface_resources_[state->id] = surface_resource;
    std::cout << "[xwayland-bridge] Created surface (ID: " << state->id << ")\n";

    // Notify compositor
    if (self->ipc_connected_) {
        shared::RenderMessage msg{
            .surface_id_ = state->id,
            .status_     = shared::RenderMessageStatus::SurfaceCreated,
            .width_      = 0,
            .height_     = 0,
            .stride_     = 0
        };
        (void)self->xserver_client_->sendMessage(
            {reinterpret_cast<const uint8_t*>(&msg), sizeof(msg)});
    }

    // If this client has a wp_security_context identity, relay it to xserver.
    if (self->ipc_connected_) {
        auto it = self->client_security_contexts_.find(client);
        if (it != self->client_security_contexts_.end()) {
            shared::XBusMessage xmsg{};
            xmsg.magic_ = shared::XBUS_MAGIC;
            std::strncpy(xmsg.service_name_, "com.freebsdx.SecurityContext",
                         sizeof(xmsg.service_name_) - 1);
            xmsg.opcode_ = static_cast<uint32_t>(shared::SecurityContextOpcode::SetContext);
            shared::SecurityContextInfo info{};
            info.surface_id_ = state->id;
            std::strncpy(info.app_id_, it->second.app_id.c_str(),
                         sizeof(info.app_id_) - 1);
            std::strncpy(info.sandbox_engine_, it->second.sandbox_engine.c_str(),
                         sizeof(info.sandbox_engine_) - 1);
            std::strncpy(info.instance_id_, it->second.instance_id.c_str(),
                         sizeof(info.instance_id_) - 1);
            std::memcpy(xmsg.payload_, &info, sizeof(info));
            (void)self->xserver_client_->sendMessage(
                {reinterpret_cast<const uint8_t*>(&xmsg), sizeof(xmsg)});
        }
    }
}

// ============================================================
// wl_shm
// ============================================================

void WaylandBridge::bindShm(wl_client* client, void* data,
                             uint32_t version, uint32_t id) noexcept {
    wl_resource* resource = wl_resource_create(client, &wl_shm_interface, version, id);
    wl_resource_set_implementation(resource, &shm_implementation, data, nullptr);
    wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
    wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
    std::cout << "[xwayland-bridge] Client bound wl_shm.\n";
}

void WaylandBridge::shmCreatePool(wl_client* client, [[maybe_unused]] wl_resource* resource,
                                   uint32_t id, int32_t fd, int32_t size) noexcept {
    wl_resource* pool_resource = wl_resource_create(client, &wl_shm_pool_interface, 1, id);
    auto* pool_state = new PoolState{ .fd = fd, .size = size };
    wl_resource_set_implementation(pool_resource, &shm_pool_implementation, pool_state,
        [](wl_resource* res) {
            auto* s = static_cast<PoolState*>(wl_resource_get_user_data(res));
            if (s->fd >= 0) close(s->fd);
            delete s;
        });
    std::cout << "[xwayland-bridge] SHM pool (FD: " << fd << ", size: " << size << ")\n";
}

void WaylandBridge::shmPoolCreateBuffer(wl_client* client, wl_resource* resource,
                                         uint32_t id, int32_t offset,
                                         int32_t width, int32_t height,
                                         int32_t stride, uint32_t format) noexcept {
    auto* pool = static_cast<PoolState*>(wl_resource_get_user_data(resource));
    wl_resource* buf = wl_resource_create(client, &wl_buffer_interface, 1, id);
    int buf_fd = fcntl(pool->fd, F_DUPFD_CLOEXEC, 0);
    auto* buf_state = new BufferState{ .fd = buf_fd,
                                       .width = width, .height = height,
                                       .stride = stride, .offset = offset,
                                       .format = format };
    wl_resource_set_implementation(buf, nullptr, buf_state, [](wl_resource* res) {
        auto* s = static_cast<BufferState*>(wl_resource_get_user_data(res));
        if (s->fd >= 0) close(s->fd);
        delete s;
    });
    std::cout << "[xwayland-bridge] SHM buffer: " << width << "x" << height << "\n";
}

void WaylandBridge::shmPoolResize(wl_client* /*client*/, wl_resource* resource,
                                   int32_t size) noexcept {
    static_cast<PoolState*>(wl_resource_get_user_data(resource))->size = size;
}

void WaylandBridge::shmPoolDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

// ============================================================
// zwp_linux_dmabuf_v1
// ============================================================

void WaylandBridge::bindLinuxDmabuf(wl_client* client, void* data,
                                     uint32_t version, uint32_t id) noexcept {
    wl_resource* resource = wl_resource_create(
        client, &zwp_linux_dmabuf_v1_interface, version, id);
    wl_resource_set_implementation(resource, &dmabuf_implementation, data, nullptr);
    // Deprecated format/modifier events: must not be sent for v4+ clients
    if (version < 4) {
        zwp_linux_dmabuf_v1_send_format(resource, 0x34325241u); // DRM_FORMAT_ARGB8888
        zwp_linux_dmabuf_v1_send_format(resource, 0x34325258u); // DRM_FORMAT_XRGB8888
    }
    std::cout << "[xwayland-bridge] Client bound zwp_linux_dmabuf_v1 (v" << version << ").\n";
}

// ---- dmabuf feedback format table (one-time setup in init) ----

struct DmabufFormatEntry {
    uint32_t format;
    uint32_t padding; // must be 0
    uint64_t modifier;
};

void WaylandBridge::initDmabufFeedback() noexcept {
    static const DmabufFormatEntry entries[] = {
        {0x34325258u, 0u, 0ULL}, // DRM_FORMAT_XRGB8888 + DRM_FORMAT_MOD_LINEAR
        {0x34325241u, 0u, 0ULL}, // DRM_FORMAT_ARGB8888 + DRM_FORMAT_MOD_LINEAR
    };
    const uint32_t size = sizeof(entries);

    int fd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        std::cerr << "[xwayland-bridge] dmabuf feedback: shm_open failed: "
                  << strerror(errno) << "\n";
        return;
    }
    if (ftruncate(fd, static_cast<off_t>(size)) < 0) { close(fd); return; }
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { close(fd); return; }
    std::memcpy(ptr, entries, size);
    munmap(ptr, size);

    dmabuf_feedback_table_fd_   = fd;
    dmabuf_feedback_table_size_ = size;

    // Discover DRM render node dev_t so Mesa can stat it and find a match
    struct ::stat st{};
    if (::stat("/dev/dri/renderD128", &st) == 0 ||
        ::stat("/dev/drm/128",         &st) == 0) {
        dmabuf_main_device_ = static_cast<uint64_t>(st.st_rdev);
    }
    std::cout << "[xwayland-bridge] dmabuf feedback ready ("
              << (size / sizeof(DmabufFormatEntry))
              << " entries, dev=" << dmabuf_main_device_ << ").\n";
}

void WaylandBridge::sendDmabufFeedback(wl_resource* res) noexcept {
    if (dmabuf_feedback_table_fd_ < 0) {
        zwp_linux_dmabuf_feedback_v1_send_done(res);
        return;
    }
    zwp_linux_dmabuf_feedback_v1_send_format_table(
        res, dmabuf_feedback_table_fd_, dmabuf_feedback_table_size_);

    wl_array dev_arr; wl_array_init(&dev_arr);
    auto* devp = static_cast<uint64_t*>(wl_array_add(&dev_arr, sizeof(uint64_t)));
    *devp = dmabuf_main_device_;
    zwp_linux_dmabuf_feedback_v1_send_main_device(res, &dev_arr);
    zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(res, &dev_arr);
    wl_array_release(&dev_arr);

    uint32_t n = dmabuf_feedback_table_size_ / sizeof(DmabufFormatEntry);
    wl_array idx_arr; wl_array_init(&idx_arr);
    for (uint32_t i = 0; i < n; ++i) {
        auto* idx = static_cast<uint16_t*>(wl_array_add(&idx_arr, sizeof(uint16_t)));
        *idx = static_cast<uint16_t>(i);
    }
    zwp_linux_dmabuf_feedback_v1_send_tranche_formats(res, &idx_arr);
    wl_array_release(&idx_arr);

    zwp_linux_dmabuf_feedback_v1_send_tranche_flags(res, 0); // render tranche
    zwp_linux_dmabuf_feedback_v1_send_tranche_done(res);
    zwp_linux_dmabuf_feedback_v1_send_done(res);
}

void WaylandBridge::dmabufFeedbackDestroy(wl_client* /*client*/,
                                           wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::dmabufGetDefaultFeedback(wl_client* client, wl_resource* res,
                                              uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(res));
    wl_resource* fb = wl_resource_create(
        client, &zwp_linux_dmabuf_feedback_v1_interface, 1, id);
    wl_resource_set_implementation(fb, &dmabuf_feedback_implementation, self, nullptr);
    self->sendDmabufFeedback(fb);
}

void WaylandBridge::dmabufGetSurfaceFeedback(wl_client* client, wl_resource* res,
                                              uint32_t id,
                                              wl_resource* /*surface*/) noexcept {
    // Per-surface feedback: for now identical to default (single GPU, single output format).
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(res));
    wl_resource* fb = wl_resource_create(
        client, &zwp_linux_dmabuf_feedback_v1_interface, 1, id);
    wl_resource_set_implementation(fb, &dmabuf_feedback_implementation, self, nullptr);
    self->sendDmabufFeedback(fb);
}

void WaylandBridge::dmabufDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::dmabufCreateParams(wl_client* client, wl_resource* /*resource*/,
                                        uint32_t params_id) noexcept {
    wl_resource* params = wl_resource_create(
        client, &zwp_linux_buffer_params_v1_interface, 1, params_id);
    auto* buf_state = new BufferState{};
    wl_resource_set_implementation(params, &dmabuf_params_implementation, buf_state,
        [](wl_resource* res) {
            auto* s = static_cast<BufferState*>(wl_resource_get_user_data(res));
            // fd ownership was transferred to the wl_buffer on create; close only if not
            if (s->fd >= 0) close(s->fd);
            delete s;
        });
}

void WaylandBridge::dmabufParamsDestroy(wl_client* /*client*/,
                                         wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::dmabufParamsAdd(wl_client* /*client*/, wl_resource* resource,
                                     int32_t fd, uint32_t /*plane_idx*/,
                                     uint32_t /*offset*/, uint32_t /*stride*/,
                                     uint32_t /*modifier_hi*/, uint32_t /*modifier_lo*/) noexcept {
    auto* s = static_cast<BufferState*>(wl_resource_get_user_data(resource));
    if (s->fd >= 0) close(s->fd); // replace any prior plane fd
    s->fd = fd;
}

void WaylandBridge::dmabufParamsCreate(wl_client* client, wl_resource* resource,
                                        int32_t width, int32_t height,
                                        uint32_t format, uint32_t /*flags*/) noexcept {
    auto* src = static_cast<BufferState*>(wl_resource_get_user_data(resource));
    src->width  = width;
    src->height = height;
    src->format = format;

    // Create the wl_buffer to hand back via the 'created' event
    wl_resource* buf = wl_resource_create(client, &wl_buffer_interface, 1, 0);
    int dup_fd = (src->fd >= 0) ? fcntl(src->fd, F_DUPFD_CLOEXEC, 0) : -1;
    auto* buf_state = new BufferState{
        .fd = dup_fd, .width = width, .height = height, .format = format
    };
    wl_resource_set_implementation(buf, nullptr, buf_state, [](wl_resource* res) {
        auto* s = static_cast<BufferState*>(wl_resource_get_user_data(res));
        if (s->fd >= 0) close(s->fd);
        delete s;
    });

    zwp_linux_buffer_params_v1_send_created(resource, buf);
    std::cout << "[xwayland-bridge] DMABUF async: " << width << "x" << height << "\n";
}

void WaylandBridge::dmabufParamsCreateImmed(wl_client* client, wl_resource* resource,
                                             uint32_t buffer_id,
                                             int32_t width, int32_t height,
                                             uint32_t format, uint32_t /*flags*/) noexcept {
    auto* src = static_cast<BufferState*>(wl_resource_get_user_data(resource));

    wl_resource* buf = wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);
    int dup_fd = (src->fd >= 0) ? fcntl(src->fd, F_DUPFD_CLOEXEC, 0) : -1;
    auto* buf_state = new BufferState{
        .fd = dup_fd, .width = width, .height = height, .format = format
    };
    wl_resource_set_implementation(buf, nullptr, buf_state, [](wl_resource* res) {
        auto* s = static_cast<BufferState*>(wl_resource_get_user_data(res));
        if (s->fd >= 0) close(s->fd);
        delete s;
    });
    std::cout << "[xwayland-bridge] DMABUF immed: " << width << "x" << height << "\n";
}

// ============================================================
// wl_surface
// ============================================================

void WaylandBridge::surfaceDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    wl_resource_destroy(resource); // triggers destructor lambda → removes from map
}

void WaylandBridge::surfaceFrame(wl_client* client, wl_resource* resource,
                                  uint32_t callback_id) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    wl_resource* cb = wl_resource_create(client, &wl_callback_interface, 1, callback_id);
    if (!cb) { wl_client_post_no_memory(client); return; }
    // No implementation needed — wl_callback only sends 'done', never receives requests.
    wl_resource_set_implementation(cb, nullptr, nullptr, nullptr);
    if (state) state->frame_callbacks.push_back(cb);
}

void WaylandBridge::surfaceAttach(wl_client* /*client*/, wl_resource* resource,
                                   wl_resource* buffer, int32_t /*x*/,
                                   int32_t /*y*/) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    if (buffer) {
        state->pending_buffer           = static_cast<BufferState*>(
                                              wl_resource_get_user_data(buffer));
        state->attached_buffer_resource = buffer;
    } else {
        state->pending_buffer           = nullptr;
        state->attached_buffer_resource = nullptr;
    }
}

// Core commit logic: send pending buffer to xserver, release back to client.
// sub_x/sub_y are the committed subsurface offset (piggybacked on damage_x_/y_).
void WaylandBridge::applyCommit(SurfaceState* state, float sub_x, float sub_y) noexcept {
    if (!state->pending_buffer || !state->bridge) return;
    BufferState*   buf = state->pending_buffer;
    WaylandBridge* br  = state->bridge;

    if (br->ipc_connected_) {
        uint32_t effective_w = (state->viewport_dst_w > 0)
                               ? static_cast<uint32_t>(state->viewport_dst_w)
                               : static_cast<uint32_t>(buf->width);
        uint32_t effective_h = (state->viewport_dst_h > 0)
                               ? static_cast<uint32_t>(state->viewport_dst_h)
                               : static_cast<uint32_t>(buf->height);

        if (buf->solid_color) {
            // Solid-color buffer: send SolidColor IPC — stride_ = ARGB, no fd
            shared::RenderMessage msg{
                .surface_id_    = state->id,
                .status_        = shared::RenderMessageStatus::SolidColor,
                .width_         = effective_w,
                .height_        = effective_h,
                .stride_        = buf->solid_argb,
                .damage_x_      = sub_x,
                .damage_y_      = sub_y,
                .damage_width_  = static_cast<float>(effective_w),
                .damage_height_ = static_cast<float>(effective_h)
            };
            std::span<const uint8_t> payload(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
            (void)br->xserver_client_->sendMessage(payload);
        } else {
            shared::RenderMessage msg{
                .surface_id_    = state->id,
                .status_        = shared::RenderMessageStatus::FrameReady,
                .width_         = effective_w,
                .height_        = effective_h,
                .stride_        = 0,
                // Subsurfaces: damage_x_/y_ carry the committed position offset.
                // Normal surfaces: 0,0 (xserver uses full-surface damage).
                .damage_x_      = sub_x,
                .damage_y_      = sub_y,
                .damage_width_  = static_cast<float>(effective_w),
                .damage_height_ = static_cast<float>(effective_h),
                // xdg_surface window geometry (geom_w_ == 0 → not set)
                .geom_x_        = state->geom_x,
                .geom_y_        = state->geom_y,
                .geom_w_        = state->geom_w,
                .geom_h_        = state->geom_h,
            };
            std::span<const uint8_t> payload(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
            (void)br->xserver_client_->sendMessage(payload, buf->fd);
        }
    }

    if (state->attached_buffer_resource)
        wl_buffer_send_release(state->attached_buffer_resource);

    // Move frame callbacks into the bridge-level pending list so they fire with the
    // real DRM flip timestamp (PresentFeedback IPC, 28 bytes).  In headless/virtual
    // mode PresentFeedback never arrives, so fire immediately with a monotonic clock.
    if (!state->frame_callbacks.empty()) {
        if (br->ipc_connected_) {
            auto& dst = br->pending_frame_callbacks_;
            dst.insert(dst.end(), state->frame_callbacks.begin(), state->frame_callbacks.end());
        } else {
            struct timespec ts{};
            clock_gettime(CLOCK_MONOTONIC, &ts);
            uint32_t ts_ms = static_cast<uint32_t>(ts.tv_sec) * 1000u
                           + static_cast<uint32_t>(ts.tv_nsec) / 1000000u;
            for (wl_resource* cb : state->frame_callbacks) {
                wl_callback_send_done(cb, ts_ms);
                wl_resource_destroy(cb);
            }
        }
        state->frame_callbacks.clear();
    }

    // wp_presentation_feedback resolved by DRM flip IPC (handleIpcMessage, 28 bytes).

    state->pending_buffer           = nullptr;
    state->attached_buffer_resource = nullptr;
}

void WaylandBridge::surfaceCommit(wl_client* /*client*/, wl_resource* resource) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    if (!state->bridge) return;
    WaylandBridge* br = state->bridge;

    // Sync subsurface: cache commit; apply only when parent commits.
    if (state->subsurface_resource) {
        auto* ss = static_cast<SubsurfaceState*>(
            wl_resource_get_user_data(state->subsurface_resource));
        if (ss && ss->sync) {
            if (state->pending_buffer) ss->has_pending_commit = true;
            return; // don't send to xserver yet; don't release buffer
        }
        // Desync subsurface: apply immediately with current position.
        if (ss && state->pending_buffer) {
            ss->cur_x = ss->pending_x;
            ss->cur_y = ss->pending_y;
            applyCommit(state, static_cast<float>(ss->cur_x), static_cast<float>(ss->cur_y));
        }
        return;
    }

    // Layer-shell surface: handle configure flow then submit buffer.
    if (state->layer_surface) {
        auto* lss = static_cast<LayerSurfaceState*>(
            wl_resource_get_user_data(state->layer_surface));
        constexpr uint32_t ANCHOR_L = 4, ANCHOR_R = 8, ANCHOR_T = 1, ANCHOR_B = 2;

        if (!lss->configure_serial) {
            // First commit (no buffer expected): resolve size and send configure.
            uint32_t out_w = br->screen_width_, out_h = br->screen_height_;
            uint32_t cfg_w = lss->pending_w, cfg_h = lss->pending_h;
            if (cfg_w == 0)
                cfg_w = ((lss->anchor & ANCHOR_L) && (lss->anchor & ANCHOR_R))
                    ? out_w - static_cast<uint32_t>(lss->margin_left + lss->margin_right)
                    : out_w;
            if (cfg_h == 0)
                cfg_h = ((lss->anchor & ANCHOR_T) && (lss->anchor & ANCHOR_B))
                    ? out_h - static_cast<uint32_t>(lss->margin_top + lss->margin_bottom)
                    : out_h;
            lss->cfg_w = cfg_w;
            lss->cfg_h = cfg_h;
            lss->configure_serial = ++br->configure_serial_;
            zwlr_layer_surface_v1_send_configure(
                state->layer_surface, lss->configure_serial, cfg_w, cfg_h);
            return; // wait for ack_configure + buffer commit
        }

        if (!lss->acked || !state->pending_buffer) return;

        // Resolve anchor-based position in output-local coordinates.
        float out_w = static_cast<float>(br->screen_width_);
        float out_h = static_cast<float>(br->screen_height_);
        float cfg_w = static_cast<float>(lss->cfg_w);
        float cfg_h = static_cast<float>(lss->cfg_h);
        float x, y;

        if ((lss->anchor & ANCHOR_L) && (lss->anchor & ANCHOR_R))
            x = static_cast<float>(lss->margin_left);
        else if (lss->anchor & ANCHOR_L)
            x = static_cast<float>(lss->margin_left);
        else if (lss->anchor & ANCHOR_R)
            x = out_w - cfg_w - static_cast<float>(lss->margin_right);
        else
            x = (out_w - cfg_w) * 0.5f;

        if ((lss->anchor & ANCHOR_T) && (lss->anchor & ANCHOR_B))
            y = static_cast<float>(lss->margin_top);
        else if (lss->anchor & ANCHOR_T)
            y = static_cast<float>(lss->margin_top);
        else if (lss->anchor & ANCHOR_B)
            y = out_h - cfg_h - static_cast<float>(lss->margin_bottom);
        else
            y = (out_h - cfg_h) * 0.5f;

        if (!lss->registered && br->ipc_connected_) {
            // Notify xserver to register this layer surface at its resolved position.
            shared::RenderMessage msg{};
            msg.surface_id_ = state->id;
            msg.status_     = shared::RenderMessageStatus::LayerSurfaceCreated;
            msg.stride_     = lss->layer;  // layer index 0-3
            msg.damage_x_   = x;
            msg.damage_y_   = y;
            msg.width_      = lss->cfg_w;
            msg.height_     = lss->cfg_h;
            std::span<const uint8_t> payload(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
            (void)br->xserver_client_->sendMessage(payload);
            lss->registered = true;
        }
        applyCommit(state, x, y);
        return;
    }

    // Normal surface (or desync-applied above): send to xserver.
    if (state->pending_buffer)
        applyCommit(state);

    // Drain pending sync-child commits in z-order.
    for (wl_resource* sub_res : state->subsurface_children) {
        auto* ss = static_cast<SubsurfaceState*>(wl_resource_get_user_data(sub_res));
        if (!ss || !ss->has_pending_commit) continue;
        auto* cs = static_cast<SurfaceState*>(wl_resource_get_user_data(ss->surface_resource));
        if (!cs || !cs->pending_buffer) { ss->has_pending_commit = false; continue; }
        ss->cur_x = ss->pending_x;
        ss->cur_y = ss->pending_y;
        applyCommit(cs, static_cast<float>(ss->cur_x), static_cast<float>(ss->cur_y));
        ss->has_pending_commit = false;
    }
}

// ============================================================
// xdg_wm_base
// ============================================================

void WaylandBridge::bindXdgShell(wl_client* client, void* data,
                                   uint32_t version, uint32_t id) noexcept {
    wl_resource* resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
    wl_resource_set_implementation(resource, &xdg_wm_base_implementation, data, nullptr);
    std::cout << "[xwayland-bridge] Client bound xdg_wm_base.\n";
}

void WaylandBridge::xdgWmBaseGetXdgSurface(wl_client* client, wl_resource* /*resource*/,
                                             uint32_t id, wl_resource* surface) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(surface));
    wl_resource* xdg_surface = wl_resource_create(client, &xdg_surface_interface, 1, id);
    wl_resource_set_implementation(xdg_surface, &xdg_surface_implementation, state, nullptr);
    state->xdg_surface = xdg_surface;
    std::cout << "[xwayland-bridge] xdg_surface for surface " << state->id << "\n";
}

void WaylandBridge::xdgWmBasePong(wl_client* /*client*/, wl_resource* /*resource*/,
                                   uint32_t /*serial*/) noexcept {}

// ============================================================
// xdg_surface
// ============================================================

void WaylandBridge::xdgSurfaceGetToplevel(wl_client* client, wl_resource* resource,
                                           uint32_t id) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    wl_resource* toplevel = wl_resource_create(client, &xdg_toplevel_interface, 1, id);
    wl_resource_set_implementation(toplevel, &xdg_toplevel_implementation, state, nullptr);
    state->xdg_toplevel = toplevel;
    std::cout << "[xwayland-bridge] xdg_toplevel for surface " << state->id << "\n";

    // Unblock the client immediately with (0, 0) — "you pick the size".
    // A second configure with real dimensions will follow when SurfaceReady
    // arrives from the compositor via handleIpcMessage().
    if (state->bridge) {
        auto it = state->bridge->surface_resources_.find(state->id);
        if (it != state->bridge->surface_resources_.end()) {
            state->bridge->sendConfigure(it->second, 0, 0);

            // Give this surface keyboard focus and notify any already-bound keyboards
            wl_resource* surf = it->second;
            // Leave old focus first (text input)
            if (state->bridge->keyboard_focus_surface_ &&
                state->bridge->keyboard_focus_surface_ != surf)
                state->bridge->sendTextInputLeave(state->bridge->keyboard_focus_surface_);
            state->bridge->keyboard_focus_surface_ = surf;
            struct wl_array keys; wl_array_init(&keys);
            for (wl_resource* kbd : state->bridge->keyboard_resources_) {
                if (wl_resource_get_client(kbd) == wl_resource_get_client(surf)) {
                    wl_keyboard_send_enter(kbd, ++state->bridge->keyboard_serial_,
                                          surf, &keys);
                }
            }
            wl_array_release(&keys);
            state->bridge->sendTextInputEnter(surf);
        }
    }
}

void WaylandBridge::xdgSurfaceSetWindowGeometry(wl_client* /*client*/, wl_resource* resource,
                                                 int32_t x, int32_t y,
                                                 int32_t w, int32_t h) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    if (!state) return;
    // Double-buffered: stored now, sent to compositor on the next wl_surface.commit.
    state->geom_x = x; state->geom_y = y; state->geom_w = w; state->geom_h = h;
}

void WaylandBridge::xdgSurfaceAckConfigure(wl_client* /*client*/, wl_resource* resource,
                                            uint32_t /*serial*/) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    state->configured = true;
}

// ============================================================
// xdg_toplevel
// ============================================================

void WaylandBridge::xdgToplevelSetTitle(wl_client* /*client*/, wl_resource* resource,
                                         const char* title) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    std::cout << "[xwayland-bridge] Surface " << state->id << " title: " << title << "\n";
    state->title = title;

    if (state->bridge && state->bridge->ipc_connected_ && state->id != 0) {
        shared::BridgeTitleMessage msg{};
        msg.surface_id_ = state->id;
        std::strncpy(msg.title_, title, sizeof(msg.title_) - 1);
        std::span<const uint8_t> payload(
            reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
        (void)state->bridge->xserver_client_->sendMessage(payload);
    }
}

void WaylandBridge::xdgToplevelSetAppId(wl_client* client, wl_resource* resource,
                                         const char* app_id) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    if (!state || !state->bridge || !state->bridge->ipc_connected_ || !app_id) return;

    std::cout << "[xwayland-bridge] Surface " << state->id << " app_id: " << app_id << "\n";

    // If this client already has a wp_security_context tag, the identity is already set;
    // otherwise, accept the xdg-toplevel app_id as a best-effort identity hint.
    if (state->bridge->client_security_contexts_.find(client) ==
        state->bridge->client_security_contexts_.end()) {
        state->bridge->onSecurityContextClientConnect(
            client, app_id, "org.freebsdx.XdgToplevel", "");
    }

    // Send SetContext to xserver for this surface.
    shared::XBusMessage xmsg{};
    xmsg.magic_ = shared::XBUS_MAGIC;
    std::strncpy(xmsg.service_name_, "com.freebsdx.SecurityContext",
                 sizeof(xmsg.service_name_) - 1);
    xmsg.opcode_ = static_cast<uint32_t>(shared::SecurityContextOpcode::SetContext);
    shared::SecurityContextInfo info{};
    info.surface_id_ = state->id;
    std::strncpy(info.app_id_, app_id, sizeof(info.app_id_) - 1);
    std::strncpy(info.sandbox_engine_, "org.freebsdx.XdgToplevel",
                 sizeof(info.sandbox_engine_) - 1);
    std::memcpy(xmsg.payload_, &info, sizeof(info));
    (void)state->bridge->xserver_client_->sendMessage(
        {reinterpret_cast<const uint8_t*>(&xmsg), sizeof(xmsg)});
}

void WaylandBridge::xdgToplevelSetParent(wl_client* /*client*/, wl_resource* toplevel,
                                          wl_resource* parent_toplevel) noexcept {
    auto* ss = static_cast<SurfaceState*>(wl_resource_get_user_data(toplevel));
    if (!ss || !ss->bridge || !ss->bridge->ipc_connected_) return;

    uint32_t parent_id = 0;
    if (parent_toplevel) {
        auto* parent_ss = static_cast<SurfaceState*>(wl_resource_get_user_data(parent_toplevel));
        if (parent_ss) parent_id = parent_ss->id;
    }

    shared::RenderMessage msg{
        .surface_id_ = ss->id,
        .status_     = shared::RenderMessageStatus::ToplevelParentSet,
        .width_      = 0,
        .height_     = 0,
        .stride_     = parent_id
    };
    (void)ss->bridge->xserver_client_->sendMessage(
        {reinterpret_cast<const uint8_t*>(&msg), sizeof(msg)});
}

void WaylandBridge::xdgToplevelResize(wl_client* /*client*/, wl_resource* toplevel,
                                       wl_resource* /*seat*/, uint32_t /*serial*/,
                                       uint32_t edges) noexcept {
    auto* ss = static_cast<SurfaceState*>(wl_resource_get_user_data(toplevel));
    if (!ss || !ss->bridge || !ss->bridge->ipc_connected_) return;

    shared::RenderMessage msg{
        .surface_id_ = ss->id,
        .status_     = shared::RenderMessageStatus::ResizeBegin,
        .width_      = 0,
        .height_     = 0,
        .stride_     = edges
    };
    (void)ss->bridge->xserver_client_->sendMessage(
        {reinterpret_cast<const uint8_t*>(&msg), sizeof(msg)});
}

// ============================================================
// xdg_positioner
// ============================================================

void WaylandBridge::xdgWmBaseCreatePositioner(wl_client* client, wl_resource* /*resource*/,
                                               uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &xdg_positioner_interface, 1, id);
    auto* ps = new PositionerState{};
    wl_resource_set_implementation(res, &positioner_implementation, ps,
        [](wl_resource* r) { delete static_cast<PositionerState*>(wl_resource_get_user_data(r)); });
}

void WaylandBridge::positonerDestroy(wl_client* /*client*/,
                                      wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::positionerSetSize(wl_client* /*client*/, wl_resource* resource,
                                       int32_t w, int32_t h) noexcept {
    auto* ps = static_cast<PositionerState*>(wl_resource_get_user_data(resource));
    if (ps) { ps->width = w; ps->height = h; }
}

void WaylandBridge::positionerSetAnchorRect(wl_client* /*client*/, wl_resource* resource,
                                             int32_t x, int32_t y, int32_t w, int32_t h) noexcept {
    auto* ps = static_cast<PositionerState*>(wl_resource_get_user_data(resource));
    if (ps) { ps->anchor_x = x; ps->anchor_y = y; ps->anchor_w = w; ps->anchor_h = h; }
}

void WaylandBridge::positionerSetAnchor(wl_client* /*client*/, wl_resource* resource,
                                         uint32_t anchor) noexcept {
    auto* ps = static_cast<PositionerState*>(wl_resource_get_user_data(resource));
    if (ps) ps->anchor = anchor;
}

void WaylandBridge::positionerSetGravity(wl_client* /*client*/, wl_resource* resource,
                                          uint32_t gravity) noexcept {
    auto* ps = static_cast<PositionerState*>(wl_resource_get_user_data(resource));
    if (ps) ps->gravity = gravity;
}

void WaylandBridge::positionerSetConstraintAdjustment(wl_client* /*client*/,
                                                       wl_resource* resource,
                                                       uint32_t adj) noexcept {
    auto* ps = static_cast<PositionerState*>(wl_resource_get_user_data(resource));
    if (ps) ps->constraint_adjustment = adj;
}

void WaylandBridge::positionerSetOffset(wl_client* /*client*/, wl_resource* resource,
                                         int32_t x, int32_t y) noexcept {
    auto* ps = static_cast<PositionerState*>(wl_resource_get_user_data(resource));
    if (ps) { ps->offset_x = x; ps->offset_y = y; }
}

void WaylandBridge::positionerSetReactive(wl_client* /*client*/,
                                           wl_resource* /*resource*/) noexcept {}
void WaylandBridge::positionerSetParentSize(wl_client* /*client*/,
                                             wl_resource* /*resource*/,
                                             int32_t /*pw*/, int32_t /*ph*/) noexcept {}
void WaylandBridge::positionerSetParentConfigure(wl_client* /*client*/,
                                                  wl_resource* /*resource*/,
                                                  uint32_t /*serial*/) noexcept {}

// Compute popup position (relative to parent surface origin) from a positioner.
static std::pair<int32_t, int32_t> resolvePopupPosition(const PositionerState& ps) noexcept {
    // Anchor point within the anchor rectangle
    int32_t ax = ps.anchor_x;
    int32_t ay = ps.anchor_y;
    switch (ps.anchor) {
        case XDG_POSITIONER_ANCHOR_TOP:          ax += ps.anchor_w / 2; break;
        case XDG_POSITIONER_ANCHOR_BOTTOM:       ax += ps.anchor_w / 2; ay += ps.anchor_h; break;
        case XDG_POSITIONER_ANCHOR_LEFT:         ay += ps.anchor_h / 2; break;
        case XDG_POSITIONER_ANCHOR_RIGHT:        ax += ps.anchor_w; ay += ps.anchor_h / 2; break;
        case XDG_POSITIONER_ANCHOR_TOP_LEFT:     break;
        case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:  ay += ps.anchor_h; break;
        case XDG_POSITIONER_ANCHOR_TOP_RIGHT:    ax += ps.anchor_w; break;
        case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT: ax += ps.anchor_w; ay += ps.anchor_h; break;
        default: ax += ps.anchor_w / 2; ay += ps.anchor_h / 2; break; // NONE → centre
    }
    // Gravity: which direction the popup expands from the anchor point
    int32_t px = ax + ps.offset_x;
    int32_t py = ay + ps.offset_y;
    switch (ps.gravity) {
        case XDG_POSITIONER_GRAVITY_TOP:          px -= ps.width / 2; py -= ps.height; break;
        case XDG_POSITIONER_GRAVITY_BOTTOM:       px -= ps.width / 2; break;
        case XDG_POSITIONER_GRAVITY_LEFT:         px -= ps.width; py -= ps.height / 2; break;
        case XDG_POSITIONER_GRAVITY_RIGHT:        py -= ps.height / 2; break;
        case XDG_POSITIONER_GRAVITY_TOP_LEFT:     px -= ps.width; py -= ps.height; break;
        case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:  px -= ps.width; break;
        case XDG_POSITIONER_GRAVITY_TOP_RIGHT:    py -= ps.height; break;
        case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT: break; // expands right+down from anchor
        default: px -= ps.width / 2; py -= ps.height / 2; break; // NONE → centred
    }
    return {px, py};
}

// ============================================================
// xdg_popup
// ============================================================

void WaylandBridge::xdgSurfaceGetPopup(wl_client* client, wl_resource* resource,
                                        uint32_t id,
                                        wl_resource* parent_xdg_surface,
                                        wl_resource* positioner_res) noexcept {
    auto* state  = static_cast<SurfaceState*>(wl_resource_get_user_data(resource));
    auto* ps     = static_cast<PositionerState*>(wl_resource_get_user_data(positioner_res));
    if (!state || !ps || !state->bridge) return;

    WaylandBridge* br = state->bridge;

    // Resolve the popup's position relative to the parent surface.
    auto [px, py] = resolvePopupPosition(*ps);
    int32_t pw = ps->width;
    int32_t ph = ps->height;

    // Find the parent's SurfaceState (needed for parent_id and position tracking).
    SurfaceState* parent_state = nullptr;
    if (parent_xdg_surface) {
        parent_state = static_cast<SurfaceState*>(
            wl_resource_get_user_data(parent_xdg_surface));
    }

    // Create xdg_popup resource.
    wl_resource* popup_res = wl_resource_create(client, &xdg_popup_interface, 1, id);
    auto* pop = new PopupState{
        .bridge        = br,
        .surface_state = state,
        .parent_state  = parent_state,
        .x             = px,
        .y             = py,
        .has_grab      = false
    };
    wl_resource_set_implementation(popup_res, &xdg_popup_implementation, pop,
        [](wl_resource* r) {
            auto* p = static_cast<PopupState*>(wl_resource_get_user_data(r));
            if (p) {
                // Remove from grab stack if present
                if (p->bridge && p->has_grab) {
                    auto& grabs = p->bridge->active_popup_grabs_;
                    grabs.erase(std::remove(grabs.begin(), grabs.end(), r), grabs.end());
                }
                if (p->surface_state) p->surface_state->xdg_popup = nullptr;
            }
            delete p;
        });

    state->xdg_popup = popup_res;
    state->configured = false; // requires ack_configure before first commit

    // Send xdg_popup.configure (position + size relative to parent).
    xdg_popup_send_configure(popup_res, px, py, pw, ph);

    // Send xdg_surface.configure (serial) to trigger ack_configure.
    uint32_t serial = ++br->configure_serial_;
    xdg_surface_send_configure(resource, serial);

    // Register with xserver as a subsurface of the parent surface.
    // This reuses the SubsurfaceCreated path so popups inherit the subsurface
    // rendering (positioned above parent, no decorations).
    if (br->ipc_connected_ && parent_state) {
        shared::RenderMessage msg{};
        msg.surface_id_ = state->id;
        msg.status_     = shared::RenderMessageStatus::SubsurfaceCreated;
        msg.stride_     = parent_state->id;   // parent_id
        msg.damage_x_   = static_cast<float>(px);
        msg.damage_y_   = static_cast<float>(py);
        std::span<const uint8_t> payload(
            reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
        (void)br->xserver_client_->sendMessage(payload);
    }

    std::cout << "[xwayland-bridge] xdg_popup: surface=" << state->id
              << " parent=" << (parent_state ? parent_state->id : 0u)
              << " at (" << px << "," << py << ") " << pw << "x" << ph << "\n";
}

void WaylandBridge::xdgPopupGrab(wl_client* /*client*/, wl_resource* resource,
                                   wl_resource* /*seat*/, uint32_t /*serial*/) noexcept {
    auto* pop = static_cast<PopupState*>(wl_resource_get_user_data(resource));
    if (!pop || !pop->bridge) return;
    pop->has_grab = true;
    // Push onto grab stack (outermost last — innermost popup is at back, dismissed first)
    pop->bridge->active_popup_grabs_.push_back(resource);
}

void WaylandBridge::xdgPopupReposition(wl_client* /*client*/, wl_resource* resource,
                                         wl_resource* positioner_res,
                                         uint32_t token) noexcept {
    auto* pop = static_cast<PopupState*>(wl_resource_get_user_data(resource));
    auto* ps  = static_cast<PositionerState*>(wl_resource_get_user_data(positioner_res));
    if (!pop || !ps || !pop->bridge || !pop->surface_state) return;

    auto [px, py] = resolvePopupPosition(*ps);
    pop->x = px;
    pop->y = py;

    // Notify xserver of new position
    if (pop->bridge->ipc_connected_ && pop->parent_state) {
        shared::RenderMessage msg{};
        msg.surface_id_ = pop->surface_state->id;
        msg.status_     = shared::RenderMessageStatus::SubsurfaceCreated;
        msg.stride_     = pop->parent_state->id;
        msg.damage_x_   = static_cast<float>(px);
        msg.damage_y_   = static_cast<float>(py);
        std::span<const uint8_t> payload(
            reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
        (void)pop->bridge->xserver_client_->sendMessage(payload);
    }

    // Send xdg_popup.repositioned + new configure
    xdg_popup_send_repositioned(resource, token);
    xdg_popup_send_configure(resource, px, py, ps->width, ps->height);
    xdg_surface_send_configure(
        pop->surface_state->xdg_surface, ++pop->bridge->configure_serial_);
}

void WaylandBridge::xdgPopupDestroy(wl_client* /*client*/,
                                     wl_resource* resource) noexcept {
    wl_resource_destroy(resource); // triggers destructor → clears xdg_popup from SurfaceState
}

// ============================================================
// wl_seat
// ============================================================

void WaylandBridge::bindSeat(wl_client* client, void* data,
                              uint32_t version, uint32_t id) noexcept {
    wl_resource* resource = wl_resource_create(client, &wl_seat_interface, version, id);
    wl_resource_set_implementation(resource, &seat_implementation, data, nullptr);

    wl_seat_send_capabilities(resource,
        WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER);

    if (version >= 2)
        wl_seat_send_name(resource, "seat0");

    std::cout << "[xwayland-bridge] Client bound wl_seat.\n";
}

void WaylandBridge::seatGetPointer(wl_client* client, wl_resource* seat_res,
                                    uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(seat_res));
    uint32_t ver = wl_resource_get_version(seat_res);
    wl_resource* ptr = wl_resource_create(client, &wl_pointer_interface, ver, id);
    wl_resource_set_implementation(ptr, &pointer_implementation, self,
        [](wl_resource* r) {
            auto* bridge = static_cast<WaylandBridge*>(wl_resource_get_user_data(r));
            if (bridge) {
                auto& v = bridge->pointer_resources_;
                v.erase(std::remove(v.begin(), v.end(), r), v.end());
            }
        });
    self->pointer_resources_.push_back(ptr);

    // Send wl_pointer.enter for this client's surface (if one exists already)
    for (auto& [sid, sres] : self->surface_resources_) {
        if (wl_resource_get_client(sres) == client) {
            wl_pointer_send_enter(ptr, ++self->pointer_serial_, sres,
                wl_fixed_from_double(self->cursor_x_),
                wl_fixed_from_double(self->cursor_y_));
            break;
        }
    }
}

void WaylandBridge::seatGetKeyboard(wl_client* client, wl_resource* seat_res,
                                     uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(seat_res));
    uint32_t ver = wl_resource_get_version(seat_res);
    wl_resource* kbd = wl_resource_create(client, &wl_keyboard_interface, ver, id);
    wl_resource_set_implementation(kbd, &keyboard_implementation, self,
        [](wl_resource* r) {
            auto* bridge = static_cast<WaylandBridge*>(wl_resource_get_user_data(r));
            if (bridge) {
                auto& v = bridge->keyboard_resources_;
                v.erase(std::remove(v.begin(), v.end(), r), v.end());
            }
        });
    self->keyboard_resources_.push_back(kbd);

    // Send the XKB keymap (or a null keymap if setup failed)
    if (self->keymap_fd_ >= 0) {
        wl_keyboard_send_keymap(kbd, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                                self->keymap_fd_,
                                static_cast<uint32_t>(self->keymap_size_));
    } else {
        int devnull = open("/dev/null", O_RDONLY | O_CLOEXEC);
        wl_keyboard_send_keymap(kbd, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP, devnull, 0);
        if (devnull >= 0) close(devnull);
    }

    // key-repeat rate and delay (v4+): 25 keys/sec, 400 ms initial delay
    if (ver >= 4)
        wl_keyboard_send_repeat_info(kbd, 25, 400);

    // Send enter for the currently focused surface (if it belongs to this client)
    if (self->keyboard_focus_surface_ &&
        wl_resource_get_client(self->keyboard_focus_surface_) == client) {
        struct wl_array keys; wl_array_init(&keys);
        wl_keyboard_send_enter(kbd, ++self->keyboard_serial_,
                               self->keyboard_focus_surface_, &keys);
        wl_array_release(&keys);
    }

    std::cout << "[xwayland-bridge] Client bound wl_keyboard (v" << ver << ").\n";
}

void WaylandBridge::seatGetTouch(wl_client* client, wl_resource* /*resource*/,
                                  uint32_t id) noexcept {
    // Touch was not advertised in capabilities; create a graceful stub anyway
    wl_resource* touch = wl_resource_create(client, &wl_touch_interface, 1, id);
    wl_resource_set_implementation(touch, &touch_implementation, nullptr, nullptr);
}

void WaylandBridge::seatRelease(wl_client* /*client*/, wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

// ============================================================
// wl_output
// ============================================================

void WaylandBridge::bindOutput(wl_client* client, void* data,
                                uint32_t version, uint32_t id) noexcept {
    auto* gd   = static_cast<OutputGlobalData*>(data);
    auto* self = gd->bridge;
    uint32_t idx = gd->output_idx;

    wl_resource* resource = wl_resource_create(client, &wl_output_interface, version, id);
    wl_resource_set_implementation(resource, &output_implementation, data,
        [](wl_resource* res) {
            auto* gd = static_cast<OutputGlobalData*>(wl_resource_get_user_data(res));
            if (!gd || !gd->bridge) return;
            auto& v = gd->bridge->per_output_resources_[gd->output_idx];
            v.erase(std::remove(v.begin(), v.end(), res), v.end());
        });

    if (idx < self->per_output_resources_.size())
        self->per_output_resources_[idx].push_back(resource);

    // Determine geometry from stored output info
    int32_t w = static_cast<int32_t>(self->screen_width_);
    int32_t h = static_cast<int32_t>(self->screen_height_);
    int32_t x = 0;
    if (idx < self->output_infos_.size()) {
        w = static_cast<int32_t>(self->output_infos_[idx].width_);
        h = static_cast<int32_t>(self->output_infos_[idx].height_);
        x = static_cast<int32_t>(self->output_infos_[idx].x_offset_);
    }
    uint32_t refresh_hz = (idx < self->output_infos_.size())
        ? self->output_infos_[idx].refresh_hz_ : 60;

    wl_output_send_geometry(resource,
        x, 0,       // global canvas position (x_offset, 0)
        527, 296,   // physical size in mm (~24" display)
        WL_OUTPUT_SUBPIXEL_UNKNOWN,
        "freebsd-x", "Virtual Output",
        WL_OUTPUT_TRANSFORM_NORMAL);

    wl_output_send_mode(resource,
        WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
        w, h,
        static_cast<int32_t>(refresh_hz * 1000));

    if (version >= 2) {
        wl_output_send_scale(resource, 1);
        wl_output_send_done(resource);
    }

    std::cout << "[xwayland-bridge] Client bound wl_output[" << idx << "] ("
              << w << "x" << h << " @" << x << ").\n";
}

void WaylandBridge::setOutputLayout(const std::vector<shared::DisplayOutputInfo>& outputs) noexcept {
    size_t new_count = outputs.size();
    size_t old_count = output_globals_.size();

    // 1. Remove excess globals (outputs removed on unplug)
    for (size_t i = new_count; i < old_count; ++i) {
        if (output_globals_[i]) {
            wl_global_destroy(output_globals_[i]);
            output_globals_[i] = nullptr;
        }
        // Resources that were bound to this global remain valid until the client releases them;
        // their destructor will try to erase from per_output_resources_[i] which is fine.
    }
    output_globals_.resize(new_count, nullptr);
    output_global_data_.resize(new_count);
    per_output_resources_.resize(new_count);
    per_output_xdg_resources_.resize(new_count);
    output_infos_.assign(outputs.begin(), outputs.end());

    // 2. Create globals for new outputs
    for (size_t i = old_count; i < new_count; ++i) {
        auto gd = std::make_unique<OutputGlobalData>(OutputGlobalData{this, static_cast<uint32_t>(i)});
        output_globals_[i] = wl_global_create(display_.get(), &wl_output_interface,
                                               3, gd.get(), WaylandBridge::bindOutput);
        output_global_data_[i] = std::move(gd);
    }

    // 3. Update existing bound resources with new dimensions
    for (size_t i = 0; i < std::min(old_count, new_count); ++i) {
        int32_t w = static_cast<int32_t>(outputs[i].width_);
        int32_t h = static_cast<int32_t>(outputs[i].height_);
        int32_t x = static_cast<int32_t>(outputs[i].x_offset_);
        uint32_t mhz = outputs[i].refresh_hz_ * 1000;

        for (wl_resource* res : per_output_resources_[i]) {
            wl_output_send_geometry(res, x, 0, 527, 296,
                WL_OUTPUT_SUBPIXEL_UNKNOWN, "freebsd-x", "Virtual Output",
                WL_OUTPUT_TRANSFORM_NORMAL);
            wl_output_send_mode(res,
                WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                w, h, static_cast<int32_t>(mhz));
            if (wl_resource_get_version(res) >= 2)
                wl_output_send_done(res);
        }

        // Also update any zxdg_output_v1 resources for this output
        for (wl_resource* xres : per_output_xdg_resources_[i]) {
            zxdg_output_v1_send_logical_position(xres, x, 0);
            zxdg_output_v1_send_logical_size(xres, w, h);
            uint32_t xver = wl_resource_get_version(xres);
            if (xver >= 2) {
                char name_buf[24];
                std::snprintf(name_buf, sizeof(name_buf), "Output-%zu", i);
                zxdg_output_v1_send_name(xres, name_buf);
                zxdg_output_v1_send_description(xres, "freebsd-x Virtual Output");
            }
            // In protocol v3+ wl_output.done replaces zxdg_output_v1.done;
            // for v1/v2 we must send it ourselves.
            if (xver < 3)
                zxdg_output_v1_send_done(xres);
        }
    }

    // Update primary dimensions (used by SurfaceReady path)
    if (!outputs.empty()) {
        screen_width_  = outputs[0].width_;
        screen_height_ = outputs[0].height_;
    }

    wl_display_flush_clients(display_.get());

    // Notify all fractional-scale subscribers. Scale is always 1.0x (120/120) for now;
    // per-output HiDPI scaling can update this value when DPI metadata is available.
    for (auto& [sid, sres] : surface_resources_) {
        auto* ss = static_cast<SurfaceState*>(wl_resource_get_user_data(sres));
        if (ss && ss->fractional_scale_res)
            wp_fractional_scale_v1_send_preferred_scale(ss->fractional_scale_res, 120);
    }

    std::cout << "[xwayland-bridge] setOutputLayout: " << new_count << " output(s)\n";
    for (size_t i = 0; i < new_count; ++i) {
        std::cout << "  [" << i << "] " << outputs[i].width_ << "x" << outputs[i].height_
                  << " @x=" << outputs[i].x_offset_ << "\n";
    }
}

// ============================================================
// InputMessage translation → Wayland pointer events
// ============================================================

void WaylandBridge::handleInputMessage(const shared::InputMessage& msg) noexcept {
    using namespace shared;

    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t time_ms = static_cast<uint32_t>(
        static_cast<uint64_t>(ts.tv_sec) * 1000u +
        static_cast<uint64_t>(ts.tv_nsec) / 1000000u);

    if (msg.type_ == InputEventType::MouseMove) {
        float dx = static_cast<float>(msg.value_a_);
        float dy = static_cast<float>(msg.value_b_);

        // Always send relative motion to registered relative-pointer resources.
        if (!relative_pointer_resources_.empty()) {
            uint64_t us = static_cast<uint64_t>(ts.tv_sec) * 1000000u +
                          static_cast<uint64_t>(ts.tv_nsec) / 1000u;
            uint32_t utime_hi = static_cast<uint32_t>(us >> 32);
            uint32_t utime_lo = static_cast<uint32_t>(us);
            wl_fixed_t fx = wl_fixed_from_double(dx);
            wl_fixed_t fy = wl_fixed_from_double(dy);
            for (wl_resource* rp : relative_pointer_resources_)
                zwp_relative_pointer_v1_send_relative_motion(
                    rp, utime_hi, utime_lo, fx, fy, fx, fy);
        }

        // Send absolute wl_pointer.motion only when the pointer is not locked.
        if (!pointer_locked_ && !pointer_resources_.empty()) {
            cursor_x_ = std::max(0.0f, std::min(cursor_x_ + dx, (float)screen_width_));
            cursor_y_ = std::max(0.0f, std::min(cursor_y_ + dy, (float)screen_height_));
            wl_fixed_t ax = wl_fixed_from_double(cursor_x_);
            wl_fixed_t ay = wl_fixed_from_double(cursor_y_);
            for (wl_resource* ptr : pointer_resources_) {
                wl_pointer_send_motion(ptr, time_ms, ax, ay);
                if (wl_resource_get_version(ptr) >= WL_POINTER_FRAME_SINCE_VERSION)
                    wl_pointer_send_frame(ptr);
            }
        }
        notifyActivity();
    } else if (msg.type_ == InputEventType::MouseButton) {
        uint32_t button = static_cast<uint32_t>(msg.value_a_); // raw Linux BTN_* code
        uint32_t state  = static_cast<uint32_t>(msg.value_b_); // 1=pressed, 0=released

        // On button press, dismiss grab-owning popups (innermost first).
        // The grab stack holds popups outermost-first; dismiss from the back.
        if (state == 1 && !active_popup_grabs_.empty()) {
            wl_resource* top_popup = active_popup_grabs_.back();
            auto* pop = static_cast<PopupState*>(wl_resource_get_user_data(top_popup));
            if (pop) {
                xdg_popup_send_popup_done(top_popup);
                active_popup_grabs_.pop_back();
                pop->has_grab = false;
            }
        }

        for (wl_resource* ptr : pointer_resources_) {
            wl_pointer_send_button(ptr, ++pointer_serial_, time_ms, button, state);
            if (wl_resource_get_version(ptr) >= WL_POINTER_FRAME_SINCE_VERSION)
                wl_pointer_send_frame(ptr);
        }
    } else if (msg.type_ == InputEventType::MouseWheel && !pointer_resources_.empty()) {
        int32_t lines = msg.value_b_; // positive = scroll down
        if (lines != 0) {
            for (wl_resource* ptr : pointer_resources_) {
                wl_pointer_send_axis(ptr, time_ms,
                    WL_POINTER_AXIS_VERTICAL_SCROLL,
                    wl_fixed_from_int(lines * 10));
                if (wl_resource_get_version(ptr) >= WL_POINTER_FRAME_SINCE_VERSION)
                    wl_pointer_send_frame(ptr);
            }
        }
    } else if (msg.type_ == InputEventType::KeyPress && !keyboard_resources_.empty()) {
        uint32_t keycode   = static_cast<uint32_t>(msg.value_a_);
        uint32_t key_state = static_cast<uint32_t>(msg.value_b_); // 1=pressed, 0=released

        // Map our modifier bitmask to XKB modifier indices
        uint32_t mods_depressed = 0;
        if (msg.modifiers_ & 1) mods_depressed |= (1u << xkb_mod_shift_);
        if (msg.modifiers_ & 2) mods_depressed |= (1u << xkb_mod_ctrl_);
        if (msg.modifiers_ & 4) mods_depressed |= (1u << xkb_mod_alt_);
        if (msg.modifiers_ & 8) mods_depressed |= (1u << xkb_mod_logo_);

        for (wl_resource* kbd : keyboard_resources_) {
            wl_keyboard_send_key(kbd, ++keyboard_serial_, time_ms, keycode,
                key_state ? WL_KEYBOARD_KEY_STATE_PRESSED
                          : WL_KEYBOARD_KEY_STATE_RELEASED);
            wl_keyboard_send_modifiers(kbd, ++keyboard_serial_,
                mods_depressed, 0, 0, 0);
        }
        notifyActivity();

        // Forward text via zwp_text_input_v3 for enabled resources on key-press only.
        // No modifier keys — clients expect text input to skip control sequences.
        if (key_state == 1 && msg.modifiers_ == 0 && !text_input_resources_.empty()) {
            wl_client* focus_client = keyboard_focus_surface_
                ? wl_resource_get_client(keyboard_focus_surface_) : nullptr;

            for (wl_resource* ti : text_input_resources_) {
                auto* tis = static_cast<TextInputState*>(wl_resource_get_user_data(ti));
                if (!tis || !tis->enabled) continue;
                if (wl_resource_get_client(ti) != focus_client) continue;

                if (msg.char_value_ > 0) {
                    // Printable character: encode codepoint as UTF-8, deliver as commit_string
                    char utf8[5] = {};
                    int32_t cp = msg.char_value_;
                    if (cp <= 0x7F) {
                        utf8[0] = static_cast<char>(cp);
                    } else if (cp <= 0x7FF) {
                        utf8[0] = static_cast<char>(0xC0 | (cp >> 6));
                        utf8[1] = static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp <= 0xFFFF) {
                        utf8[0] = static_cast<char>(0xE0 | (cp >> 12));
                        utf8[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        utf8[2] = static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        utf8[0] = static_cast<char>(0xF0 | (cp >> 18));
                        utf8[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        utf8[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        utf8[3] = static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    zwp_text_input_v3_send_preedit_string(ti, nullptr, 0, 0);
                    zwp_text_input_v3_send_commit_string(ti, utf8);
                    zwp_text_input_v3_send_delete_surrounding_text(ti, 0, 0);
                    zwp_text_input_v3_send_done(ti, tis->serial);
                } else if (keycode == 14) {
                    // KEY_BACKSPACE (Linux evdev code 14): delete one byte before cursor
                    zwp_text_input_v3_send_preedit_string(ti, nullptr, 0, 0);
                    zwp_text_input_v3_send_commit_string(ti, nullptr);
                    zwp_text_input_v3_send_delete_surrounding_text(ti, 1, 0);
                    zwp_text_input_v3_send_done(ti, tis->serial);
                }
            }
        }
    }

    wl_display_flush_clients(display_.get());
}

void WaylandBridge::signalCursorLock(bool locked) noexcept {
    if (!ipc_connected_) return;
    shared::XBusMessage msg{};
    std::strncpy(msg.service_name_, "com.freebsdx.Input", sizeof(msg.service_name_) - 1);
    msg.opcode_ = locked ? 0x01u : 0x02u;
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
    (void)xserver_client_->sendMessage(span);
}

// ============================================================
// zwp_relative_pointer_manager_v1 + zwp_relative_pointer_v1
// ============================================================

void WaylandBridge::bindRelativePointerManager(wl_client* client, void* data,
                                                uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &zwp_relative_pointer_manager_v1_interface, version, id);
    wl_resource_set_implementation(res, &relative_pointer_manager_implementation, data, nullptr);
}

void WaylandBridge::relativePointerManagerDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res);
}

void WaylandBridge::relativePointerManagerGetRelativePointer(
        wl_client* client, wl_resource* manager_res, uint32_t id,
        wl_resource* /*pointer_res*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager_res));
    wl_resource* rp = wl_resource_create(
        client, &zwp_relative_pointer_v1_interface,
        wl_resource_get_version(manager_res), id);

    auto* rd = new RelativePointerData{self};
    wl_resource_set_implementation(rp, &relative_pointer_implementation, rd,
        [](wl_resource* r) {
            auto* d = static_cast<RelativePointerData*>(wl_resource_get_user_data(r));
            if (d && d->bridge) {
                auto& v = d->bridge->relative_pointer_resources_;
                v.erase(std::remove(v.begin(), v.end(), r), v.end());
            }
            delete d;
        });
    self->relative_pointer_resources_.push_back(rp);
    std::cout << "[xwayland-bridge] Client bound zwp_relative_pointer_v1.\n";
}

void WaylandBridge::relativePointerDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res);
}

// ============================================================
// zwp_pointer_constraints_v1 + zwp_locked_pointer_v1 + zwp_confined_pointer_v1
// ============================================================

void WaylandBridge::bindPointerConstraints(wl_client* client, void* data,
                                            uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &zwp_pointer_constraints_v1_interface, version, id);
    wl_resource_set_implementation(res, &pointer_constraints_implementation, data, nullptr);
}

void WaylandBridge::pointerConstraintsDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res);
}

void WaylandBridge::pointerConstraintsLockPointer(
        wl_client* client, wl_resource* constraints_res, uint32_t id,
        wl_resource* surface_res, wl_resource* /*pointer_res*/,
        wl_resource* /*region_res*/, uint32_t /*lifetime*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(constraints_res));

    wl_resource* lp = wl_resource_create(
        client, &zwp_locked_pointer_v1_interface,
        wl_resource_get_version(constraints_res), id);

    auto* ld = new LockedPointerData{self, surface_res};
    wl_resource_set_implementation(lp, &locked_pointer_implementation, ld,
        [](wl_resource* r) {
            auto* d = static_cast<LockedPointerData*>(wl_resource_get_user_data(r));
            if (d && d->bridge && d->bridge->locked_pointer_resource_ == r) {
                d->bridge->pointer_locked_ = false;
                d->bridge->locked_pointer_resource_ = nullptr;
                d->bridge->signalCursorLock(false);
                zwp_locked_pointer_v1_send_unlocked(r);
                std::cout << "[xwayland-bridge] Pointer unlocked.\n";
            }
            delete d;
        });

    self->locked_pointer_resource_ = lp;
    self->pointer_locked_ = true;
    self->signalCursorLock(true);
    zwp_locked_pointer_v1_send_locked(lp);
    std::cout << "[xwayland-bridge] Pointer locked by client.\n";
}

void WaylandBridge::pointerConstraintsConfinePointer(
        wl_client* client, wl_resource* constraints_res, uint32_t id,
        wl_resource* surface_res, wl_resource* /*pointer_res*/,
        wl_resource* /*region_res*/, uint32_t /*lifetime*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(constraints_res));

    wl_resource* cp = wl_resource_create(
        client, &zwp_confined_pointer_v1_interface,
        wl_resource_get_version(constraints_res), id);

    auto* cd = new ConfinedPointerData{self, surface_res};
    wl_resource_set_implementation(cp, &confined_pointer_implementation, cd,
        [](wl_resource* r) {
            auto* d = static_cast<ConfinedPointerData*>(wl_resource_get_user_data(r));
            if (d && d->bridge) {
                zwp_confined_pointer_v1_send_unconfined(r);
            }
            delete d;
        });

    zwp_confined_pointer_v1_send_confined(cp);
    std::cout << "[xwayland-bridge] Pointer confined by client.\n";
}

void WaylandBridge::lockedPointerDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res); // destructor lambda above handles unlock
}

void WaylandBridge::lockedPointerSetCursorPositionHint(wl_client*, wl_resource* /*res*/,
                                                        wl_fixed_t /*x*/, wl_fixed_t /*y*/) noexcept {
    // Hint where to warp the cursor on unlock; ignored (DRM cursor stays put)
}

void WaylandBridge::lockedPointerSetRegion(wl_client*, wl_resource*, wl_resource*) noexcept {}

void WaylandBridge::confinedPointerDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res);
}

void WaylandBridge::confinedPointerSetRegion(wl_client*, wl_resource*, wl_resource*) noexcept {}

// ============================================================
// zxdg_output_manager_v1 + zxdg_output_v1
// ============================================================

void WaylandBridge::bindXdgOutputManager(wl_client* client, void* data,
                                          uint32_t version, uint32_t id) noexcept {
    wl_resource* resource = wl_resource_create(
        client, &zxdg_output_manager_v1_interface, version, id);
    wl_resource_set_implementation(resource, &xdg_output_manager_implementation, data, nullptr);
    std::cout << "[xwayland-bridge] Client bound zxdg_output_manager_v1 (v" << version << ").\n";
}

void WaylandBridge::xdgOutputManagerDestroy(wl_client* /*client*/,
                                             wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::xdgOutputManagerGetXdgOutput(wl_client* client, wl_resource* manager_res,
                                                  uint32_t id, wl_resource* output_res) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager_res));

    // Determine which physical output this wl_output resource belongs to
    uint32_t output_idx = 0;
    auto* gd = static_cast<OutputGlobalData*>(wl_resource_get_user_data(output_res));
    if (gd) output_idx = gd->output_idx;

    uint32_t mgr_version = wl_resource_get_version(manager_res);
    wl_resource* xdg_out = wl_resource_create(
        client, &zxdg_output_v1_interface, mgr_version, id);

    auto* xd = new XdgOutputData{self, output_idx};
    wl_resource_set_implementation(xdg_out, &xdg_output_implementation, xd,
        [](wl_resource* res) {
            auto* d = static_cast<XdgOutputData*>(wl_resource_get_user_data(res));
            if (d && d->bridge && d->output_idx < d->bridge->per_output_xdg_resources_.size()) {
                auto& v = d->bridge->per_output_xdg_resources_[d->output_idx];
                v.erase(std::remove(v.begin(), v.end(), res), v.end());
            }
            delete d;
        });

    if (output_idx < self->per_output_xdg_resources_.size())
        self->per_output_xdg_resources_[output_idx].push_back(xdg_out);

    // Send initial geometry
    int32_t x = 0, w = static_cast<int32_t>(self->screen_width_),
                    h = static_cast<int32_t>(self->screen_height_);
    if (output_idx < self->output_infos_.size()) {
        x = static_cast<int32_t>(self->output_infos_[output_idx].x_offset_);
        w = static_cast<int32_t>(self->output_infos_[output_idx].width_);
        h = static_cast<int32_t>(self->output_infos_[output_idx].height_);
    }

    zxdg_output_v1_send_logical_position(xdg_out, x, 0);
    zxdg_output_v1_send_logical_size(xdg_out, w, h);
    if (mgr_version >= 2) {
        char name_buf[24];
        std::snprintf(name_buf, sizeof(name_buf), "Output-%u", output_idx);
        zxdg_output_v1_send_name(xdg_out, name_buf);
        zxdg_output_v1_send_description(xdg_out, "freebsd-x Virtual Output");
    }
    if (mgr_version < 3)
        zxdg_output_v1_send_done(xdg_out);
    // v3+: wl_output.done (already sent by bindOutput) acts as the synchronisation event

    std::cout << "[xwayland-bridge] zxdg_output_v1 created for output[" << output_idx
              << "] (" << w << "x" << h << " @x=" << x << ")\n";
}

void WaylandBridge::xdgOutputDestroy(wl_client* /*client*/,
                                      wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

// ============================================================
// zwlr_screencopy_manager_v1 + zwlr_screencopy_frame_v1
// ============================================================

void WaylandBridge::bindScreencopyManager(wl_client* client, void* data,
                                           uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &zwlr_screencopy_manager_v1_interface, version, id);
    wl_resource_set_implementation(res, &screencopy_manager_implementation, data, nullptr);
    std::cout << "[xwayland-bridge] Client bound zwlr_screencopy_manager_v1.\n";
}

void WaylandBridge::screencopyManagerDestroy(wl_client* /*client*/,
                                              wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::screencopyManagerCaptureOutput(wl_client* client, wl_resource* manager_res,
                                                    uint32_t id, int32_t /*overlay_cursor*/,
                                                    wl_resource* output_res) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager_res));

    uint32_t output_idx = 0;
    if (auto* gd = static_cast<OutputGlobalData*>(wl_resource_get_user_data(output_res)))
        output_idx = gd->output_idx;

    uint32_t w = self->screen_width_, h = self->screen_height_;
    if (output_idx < self->output_infos_.size()) {
        w = self->output_infos_[output_idx].width_;
        h = self->output_infos_[output_idx].height_;
    }
    uint32_t stride = w * 4; // XRGB8888 — 4 bytes per pixel

    uint32_t ver = wl_resource_get_version(manager_res);
    wl_resource* frame_res = wl_resource_create(
        client, &zwlr_screencopy_frame_v1_interface, ver, id);

    auto* fd = new ScreencopyFrameData{self, output_idx, w, h, stride};
    wl_resource_set_implementation(frame_res, &screencopy_frame_implementation, fd,
        [](wl_resource* res) {
            delete static_cast<ScreencopyFrameData*>(wl_resource_get_user_data(res));
        });

    zwlr_screencopy_frame_v1_send_buffer(frame_res, WL_SHM_FORMAT_XRGB8888, w, h, stride);
    if (ver >= 3)
        zwlr_screencopy_frame_v1_send_buffer_done(frame_res);
    wl_display_flush_clients(self->display_.get());

    std::cout << "[xwayland-bridge] screencopy frame for output[" << output_idx
              << "] " << w << "x" << h << "\n";
}

void WaylandBridge::screencopyManagerCaptureOutputRegion(wl_client* client, wl_resource* manager_res,
                                                          uint32_t id, int32_t overlay_cursor,
                                                          wl_resource* output_res,
                                                          int32_t /*x*/, int32_t /*y*/,
                                                          int32_t /*width*/, int32_t /*height*/) noexcept {
    // Capture the full output — region clipping not needed for our use-case.
    screencopyManagerCaptureOutput(client, manager_res, id, overlay_cursor, output_res);
}

void WaylandBridge::screencopyFrameDestroy(wl_client* /*client*/,
                                            wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::screencopyFrameCopy(wl_client* /*client*/, wl_resource* frame_res,
                                         wl_resource* buffer_res) noexcept {
    auto* fd_data = static_cast<ScreencopyFrameData*>(wl_resource_get_user_data(frame_res));
    if (!fd_data || !fd_data->bridge) return;
    auto* self = fd_data->bridge;

    auto* buf = static_cast<BufferState*>(wl_resource_get_user_data(buffer_res));
    if (!buf || buf->fd < 0) {
        zwlr_screencopy_frame_v1_send_failed(frame_res);
        return;
    }

    int dup_fd = fcntl(buf->fd, F_DUPFD_CLOEXEC, 0);
    if (dup_fd < 0) {
        zwlr_screencopy_frame_v1_send_failed(frame_res);
        return;
    }

    uint32_t stride = (buf->stride > 0) ? static_cast<uint32_t>(buf->stride)
                                        : fd_data->stride;
    self->pending_screencopy_ = PendingScreencopy{
        .frame_resource    = frame_res,
        .client_shm_fd     = dup_fd,
        .client_shm_offset = buf->offset,
        .copy_stride       = stride,
        .copy_height       = static_cast<uint32_t>(buf->height)
    };

    if (self->ipc_connected_) {
        shared::XBusMessage msg{};
        std::strncpy(msg.service_name_, "com.freebsdx.Screencopy",
                     sizeof(msg.service_name_) - 1);
        msg.opcode_ = 0x01;
        shared::ScreencopyCaptureRequest req{ .output_idx_ = fd_data->output_idx };
        std::memcpy(msg.payload_, &req, sizeof(req));
        std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
        (void)self->xserver_client_->sendMessage(span);
    } else {
        // Offline: send a failure immediately
        zwlr_screencopy_frame_v1_send_failed(frame_res);
        ::close(dup_fd);
        self->pending_screencopy_.reset();
    }
}

void WaylandBridge::screencopyFrameCopyWithDamage(wl_client* client, wl_resource* frame_res,
                                                   wl_resource* buffer_res) noexcept {
    // Treat the same as a full copy — damage information not tracked separately.
    screencopyFrameCopy(client, frame_res, buffer_res);
}

// ============================================================
// wp_viewporter + wp_viewport
// ============================================================

void WaylandBridge::bindViewporter(wl_client* client, void* data,
                                    uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &wp_viewporter_interface, version, id);
    wl_resource_set_implementation(res, &viewporter_implementation, data, nullptr);
}

void WaylandBridge::viewporterDestroy(wl_client* /*client*/,
                                       wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::viewporterGetViewport(wl_client* client, wl_resource* /*vp_res*/,
                                           uint32_t id, wl_resource* surface_res) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(surface_res));
    wl_resource* vp = wl_resource_create(client, &wp_viewport_interface, 1, id);
    // Store SurfaceState* so set_destination can update it directly
    wl_resource_set_implementation(vp, &viewport_implementation, state,
        [](wl_resource* r) {
            // On viewport destroy, clear destination so surface reverts to buffer size
            auto* s = static_cast<SurfaceState*>(wl_resource_get_user_data(r));
            if (s) { s->viewport_dst_w = -1; s->viewport_dst_h = -1; }
        });
}

void WaylandBridge::viewportDestroy(wl_client* /*client*/,
                                     wl_resource* resource) noexcept {
    wl_resource_destroy(resource); // destructor lambda above clears the viewport state
}

void WaylandBridge::viewportSetSource(wl_client* /*client*/, wl_resource* /*vp*/,
                                       wl_fixed_t /*x*/, wl_fixed_t /*y*/,
                                       wl_fixed_t /*w*/, wl_fixed_t /*h*/) noexcept {
    // Source crop region — accepted but not GPU-applied in this bridge implementation
}

void WaylandBridge::viewportSetDestination(wl_client* /*client*/, wl_resource* vp,
                                            int32_t width, int32_t height) noexcept {
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(vp));
    if (state) {
        state->viewport_dst_w = width;  // -1 means "unset" per protocol
        state->viewport_dst_h = height;
    }
}

// ============================================================
// xdg_activation_v1 + xdg_activation_token_v1
// ============================================================

void WaylandBridge::bindActivation(wl_client* client, void* data,
                                    uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &xdg_activation_v1_interface, version, id);
    wl_resource_set_implementation(res, &activation_implementation, data, nullptr);
}

void WaylandBridge::activationDestroy(wl_client* /*client*/,
                                       wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::activationGetToken(wl_client* client, wl_resource* act_res,
                                        uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(act_res));
    wl_resource* tok = wl_resource_create(
        client, &xdg_activation_token_v1_interface,
        wl_resource_get_version(act_res), id);
    wl_resource_set_implementation(tok, &activation_token_implementation, self, nullptr);
}

void WaylandBridge::activationActivate(wl_client* /*client*/, wl_resource* act_res,
                                        const char* /*token*/,
                                        wl_resource* surface_res) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(act_res));
    if (!surface_res) return;

    // Transfer keyboard focus to the requested surface
    self->keyboard_focus_surface_ = surface_res;
    struct wl_array keys; wl_array_init(&keys);
    for (wl_resource* kbd : self->keyboard_resources_) {
        if (wl_resource_get_client(kbd) == wl_resource_get_client(surface_res))
            wl_keyboard_send_enter(kbd, ++self->keyboard_serial_, surface_res, &keys);
    }
    wl_array_release(&keys);
    wl_display_flush_clients(self->display_.get());
    std::cout << "[xwayland-bridge] xdg_activation: focus transferred.\n";
}

void WaylandBridge::activationTokenSetSerial(wl_client* /*client*/, wl_resource* /*tok*/,
                                              uint32_t /*serial*/,
                                              wl_resource* /*seat*/) noexcept {}

void WaylandBridge::activationTokenSetAppId(wl_client* /*client*/, wl_resource* /*tok*/,
                                             const char* /*app_id*/) noexcept {}

void WaylandBridge::activationTokenSetSurface(wl_client* /*client*/, wl_resource* /*tok*/,
                                               wl_resource* /*surface*/) noexcept {}

void WaylandBridge::activationTokenCommit(wl_client* /*client*/,
                                           wl_resource* tok_res) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(tok_res));
    // Generate a unique token string and return it to the client
    char token_str[32];
    std::snprintf(token_str, sizeof(token_str), "freebsdx-%u",
                  self->next_activation_token_++);
    xdg_activation_token_v1_send_done(tok_res, token_str);
}

void WaylandBridge::activationTokenDestroy(wl_client* /*client*/,
                                            wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

// ============================================================
// wl_data_device_manager + wl_data_source + wl_data_device + wl_data_offer
// ============================================================

void WaylandBridge::bindDataDeviceManager(wl_client* client, void* data,
                                           uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &wl_data_device_manager_interface, version, id);
    wl_resource_set_implementation(res, &data_device_manager_implementation, data, nullptr);
}

void WaylandBridge::dataDeviceManagerCreateDataSource(wl_client* client,
                                                       wl_resource* manager_res,
                                                       uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager_res));
    uint32_t ver = wl_resource_get_version(manager_res);
    wl_resource* src = wl_resource_create(client, &wl_data_source_interface, ver, id);

    auto* state = new DataSourceState{self};
    wl_resource_set_implementation(src, &data_source_implementation, state,
        [](wl_resource* r) {
            auto* s = static_cast<DataSourceState*>(wl_resource_get_user_data(r));
            // If this was the active selection, clear it
            if (s && s->bridge && s->bridge->current_selection_source_ == r) {
                s->bridge->current_selection_source_ = nullptr;
                for (wl_resource* dev : s->bridge->data_device_resources_)
                    wl_data_device_send_selection(dev, nullptr);
            }
            delete s;
        });
}

void WaylandBridge::dataDeviceManagerGetDataDevice(wl_client* client,
                                                    wl_resource* manager_res,
                                                    uint32_t id,
                                                    wl_resource* /*seat_res*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager_res));
    uint32_t ver = wl_resource_get_version(manager_res);
    wl_resource* dev = wl_resource_create(client, &wl_data_device_interface, ver, id);

    wl_resource_set_implementation(dev, &data_device_implementation, self,
        [](wl_resource* r) {
            auto* bridge = static_cast<WaylandBridge*>(wl_resource_get_user_data(r));
            if (bridge) {
                auto& v = bridge->data_device_resources_;
                v.erase(std::remove(v.begin(), v.end(), r), v.end());
            }
        });
    self->data_device_resources_.push_back(dev);

    // Advertise current selection to the new device immediately
    self->sendSelectionTo(dev);
    wl_display_flush_clients(self->display_.get());
}

// --- wl_data_source ---

void WaylandBridge::dataSourceOffer(wl_client* /*client*/, wl_resource* src_res,
                                     const char* mime_type) noexcept {
    auto* state = static_cast<DataSourceState*>(wl_resource_get_user_data(src_res));
    if (state) state->mime_types.emplace_back(mime_type);
}

void WaylandBridge::dataSourceDestroy(wl_client* /*client*/,
                                       wl_resource* resource) noexcept {
    wl_resource_destroy(resource); // destructor lambda above handles selection cleanup
}

void WaylandBridge::dataSourceSetActions(wl_client* /*client*/, wl_resource* /*res*/,
                                          uint32_t /*dnd_actions*/) noexcept {
    // DnD actions — not needed for clipboard-only implementation
}

// --- wl_data_device ---

void WaylandBridge::dataDeviceStartDrag(wl_client* /*client*/, wl_resource* /*dev*/,
                                         wl_resource* /*source*/, wl_resource* /*origin*/,
                                         wl_resource* /*icon*/, uint32_t /*serial*/) noexcept {
    // DnD not yet implemented
}

void WaylandBridge::dataDeviceSetSelection(wl_client* /*client*/, wl_resource* dev_res,
                                            wl_resource* source_res,
                                            uint32_t /*serial*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(dev_res));

    // Cancel the previous selection source
    if (self->current_selection_source_ &&
        self->current_selection_source_ != source_res)
        wl_data_source_send_cancelled(self->current_selection_source_);

    self->current_selection_source_ = source_res;

    // Notify every data device of the new selection
    for (wl_resource* dev : self->data_device_resources_)
        self->sendSelectionTo(dev);
    wl_display_flush_clients(self->display_.get());
}

void WaylandBridge::dataDeviceRelease(wl_client* /*client*/,
                                       wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

// --- wl_data_offer ---

void WaylandBridge::dataOfferAccept(wl_client* /*client*/, wl_resource* /*offer*/,
                                     uint32_t /*serial*/, const char* /*mime_type*/) noexcept {
    // DnD accept hint — not needed for clipboard
}

void WaylandBridge::dataOfferReceive(wl_client* /*client*/, wl_resource* offer_res,
                                      const char* mime_type, int32_t fd) noexcept {
    auto* state = static_cast<DataOfferState*>(wl_resource_get_user_data(offer_res));
    if (state && state->source_resource) {
        // Ask the source client to write the requested MIME type to fd.
        // libwayland sends fd via SCM_RIGHTS on the source client's socket.
        wl_data_source_send_send(state->source_resource, mime_type, fd);
    }
    ::close(fd); // bridge's copy; kernel duplicated it for the source client above
}

void WaylandBridge::dataOfferDestroy(wl_client* /*client*/,
                                      wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::dataOfferFinish(wl_client* /*client*/,
                                     wl_resource* /*resource*/) noexcept {
    // DnD finish — not needed for clipboard
}

void WaylandBridge::dataOfferSetActions(wl_client* /*client*/, wl_resource* /*offer*/,
                                         uint32_t /*dnd_actions*/,
                                         uint32_t /*preferred_action*/) noexcept {
    // DnD action negotiation — not needed for clipboard
}

// --- sendSelectionTo helper ---

void WaylandBridge::sendSelectionTo(wl_resource* device_res) noexcept {
    wl_client* client = wl_resource_get_client(device_res);

    if (!current_selection_source_) {
        wl_data_device_send_selection(device_res, nullptr);
        return;
    }

    auto* src_state = static_cast<DataSourceState*>(
        wl_resource_get_user_data(current_selection_source_));
    if (!src_state) return;

    uint32_t ver = wl_resource_get_version(device_res);
    wl_resource* offer = wl_resource_create(client, &wl_data_offer_interface, ver, 0);

    auto* offer_state = new DataOfferState{current_selection_source_};
    wl_resource_set_implementation(offer, &data_offer_implementation, offer_state,
        [](wl_resource* r) {
            delete static_cast<DataOfferState*>(wl_resource_get_user_data(r));
        });

    // data_offer event must precede selection so the client knows the object
    wl_data_device_send_data_offer(device_res, offer);

    for (const auto& mime : src_state->mime_types)
        wl_data_offer_send_offer(offer, mime.c_str());

    wl_data_device_send_selection(device_res, offer);
}

// ============================================================
// wp_presentation + wp_presentation_feedback
// ============================================================

void WaylandBridge::bindPresentation(wl_client* client, void* data,
                                      uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &wp_presentation_interface, version, id);
    wl_resource_set_implementation(res, &presentation_implementation, data, nullptr);
    // Advertise CLOCK_MONOTONIC (1) so clients know which clock our timestamps use
    wp_presentation_send_clock_id(res, CLOCK_MONOTONIC);
    std::cout << "[xwayland-bridge] Client bound wp_presentation.\n";
}

void WaylandBridge::presentationDestroy(wl_client* /*client*/,
                                         wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::presentationFeedback(wl_client* client, wl_resource* pres_res,
                                          wl_resource* surface_res, uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(pres_res));

    // Resolve surface_id from the SurfaceState attached to this wl_surface resource
    auto* state = static_cast<SurfaceState*>(wl_resource_get_user_data(surface_res));
    if (!state) return; // surface already destroyed

    // wp_presentation_feedback has no requests — null implementation
    wl_resource* fb = wl_resource_create(
        client, &wp_presentation_feedback_interface,
        wl_resource_get_version(pres_res), id);

    auto* fbd = new PresentationFeedbackData{self, state->id};
    wl_resource_set_implementation(fb, nullptr, fbd,
        [](wl_resource* r) {
            auto* d = static_cast<PresentationFeedbackData*>(wl_resource_get_user_data(r));
            // Remove from pending map if not yet resolved
            if (d && d->bridge) {
                auto it = d->bridge->pending_feedbacks_.find(d->surface_id);
                if (it != d->bridge->pending_feedbacks_.end()) {
                    auto& v = it->second;
                    v.erase(std::remove(v.begin(), v.end(), r), v.end());
                    if (v.empty()) d->bridge->pending_feedbacks_.erase(it);
                }
            }
            delete d;
        });

    self->pending_feedbacks_[state->id].push_back(fb);
}

// ============================================================
// zxdg_decoration_manager_v1 + zxdg_toplevel_decoration_v1
// ============================================================

void WaylandBridge::bindDecorationManager(wl_client* client, void* data,
                                           uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &zxdg_decoration_manager_v1_interface, version, id);
    wl_resource_set_implementation(res, &decoration_manager_implementation, data, nullptr);
    std::cout << "[xwayland-bridge] Client bound zxdg_decoration_manager_v1.\n";
}

void WaylandBridge::decorationManagerDestroy(wl_client* /*client*/,
                                              wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::decorationManagerGetToplevelDecoration(wl_client* client,
                                                            wl_resource* manager_res,
                                                            uint32_t id,
                                                            wl_resource* /*toplevel*/) noexcept {
    uint32_t ver = wl_resource_get_version(manager_res);
    wl_resource* deco = wl_resource_create(
        client, &zxdg_toplevel_decoration_v1_interface, ver, id);
    wl_resource_set_implementation(deco, &toplevel_decoration_implementation,
                                   wl_resource_get_user_data(manager_res), nullptr);

    // Immediately advertise server-side decorations — the compositor owns all chrome
    zxdg_toplevel_decoration_v1_send_configure(
        deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void WaylandBridge::toplevelDecorationDestroy(wl_client* /*client*/,
                                               wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::toplevelDecorationSetMode(wl_client* /*client*/,
                                               wl_resource* resource,
                                               uint32_t /*mode*/) noexcept {
    // Client preference is ignored — always enforce server-side
    zxdg_toplevel_decoration_v1_send_configure(
        resource, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void WaylandBridge::toplevelDecorationUnsetMode(wl_client* /*client*/,
                                                 wl_resource* resource) noexcept {
    // No preference stated — still server-side
    zxdg_toplevel_decoration_v1_send_configure(
        resource, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

// ============================================================
// Event loop helpers
// ============================================================

int WaylandBridge::getEventFd() const noexcept {
    return wl_event_loop_get_fd(event_loop_);
}

void WaylandBridge::dispatch() noexcept {
    wl_event_loop_dispatch(event_loop_, 0);
}

void WaylandBridge::flush() noexcept {
    wl_display_flush_clients(display_.get());
}

// ============================================================
// wl_subcompositor + wl_subsurface
// ============================================================

void WaylandBridge::bindSubcompositor(wl_client* client, void* data,
                                       uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &wl_subcompositor_interface, version, id);
    wl_resource_set_implementation(res, &subcompositor_implementation, data, nullptr);
}

void WaylandBridge::subcompositorDestroy(wl_client* /*client*/,
                                          wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::subcompositorGetSubsurface(wl_client* client, wl_resource* /*resource*/,
                                                uint32_t id,
                                                wl_resource* surface,
                                                wl_resource* parent) noexcept {
    auto* parent_state = static_cast<SurfaceState*>(wl_resource_get_user_data(parent));
    auto* surf_state   = static_cast<SurfaceState*>(wl_resource_get_user_data(surface));
    if (!parent_state || !surf_state || !surf_state->bridge) return;

    WaylandBridge* br = surf_state->bridge;

    // Create the wl_subsurface resource
    wl_resource* sub_res = wl_resource_create(client, &wl_subsurface_interface, 1, id);

    auto* ss = new SubsurfaceState{
        .bridge           = br,
        .surface_resource = surface,
        .parent_resource  = parent,
        .pending_x        = 0,
        .pending_y        = 0,
        .cur_x            = 0,
        .cur_y            = 0,
        .sync             = true,
        .has_pending_commit = false
    };

    wl_resource_set_implementation(sub_res, &subsurface_implementation, ss,
        [](wl_resource* r) {
            auto* s = static_cast<SubsurfaceState*>(wl_resource_get_user_data(r));
            if (!s) return;
            // Remove from parent's child list
            if (s->parent_resource) {
                auto* ps = static_cast<SurfaceState*>(
                    wl_resource_get_user_data(s->parent_resource));
                if (ps) {
                    auto& v = ps->subsurface_children;
                    v.erase(std::remove(v.begin(), v.end(), r), v.end());
                }
            }
            // Clear subsurface role from the surface
            if (s->surface_resource) {
                auto* cs = static_cast<SurfaceState*>(
                    wl_resource_get_user_data(s->surface_resource));
                if (cs) cs->subsurface_resource = nullptr;
            }
            delete s;
        });

    // Assign subsurface role to the surface
    surf_state->subsurface_resource = sub_res;
    // Register as a child of the parent (appended = topmost z-order)
    parent_state->subsurface_children.push_back(sub_res);

    // Notify xserver: SubsurfaceCreated — stride_ = parent_id, damage_x_/y_ = initial offset
    if (br->ipc_connected_) {
        shared::RenderMessage msg{};
        msg.surface_id_ = surf_state->id;
        msg.status_     = shared::RenderMessageStatus::SubsurfaceCreated;
        msg.stride_     = parent_state->id; // parent_id
        msg.damage_x_   = 0.0f;
        msg.damage_y_   = 0.0f;
        std::span<const uint8_t> payload(
            reinterpret_cast<const uint8_t*>(&msg), sizeof(msg));
        (void)br->xserver_client_->sendMessage(payload);
    }

    std::cout << "[xwayland-bridge] Subsurface created (surface=" << surf_state->id
              << " parent=" << parent_state->id << ")\n";
}

void WaylandBridge::subsurfaceDestroy(wl_client* /*client*/,
                                       wl_resource* resource) noexcept {
    wl_resource_destroy(resource); // triggers destructor above
}

void WaylandBridge::subsurfaceSetPosition(wl_client* /*client*/, wl_resource* resource,
                                           int32_t x, int32_t y) noexcept {
    auto* ss = static_cast<SubsurfaceState*>(wl_resource_get_user_data(resource));
    if (!ss) return;
    // Position becomes effective on the next commit (sync: parent commit; desync: self commit)
    ss->pending_x = x;
    ss->pending_y = y;
}

void WaylandBridge::subsurfacePlaceAbove(wl_client* /*client*/, wl_resource* resource,
                                          wl_resource* sibling) noexcept {
    auto* ss = static_cast<SubsurfaceState*>(wl_resource_get_user_data(resource));
    if (!ss || !ss->parent_resource) return;
    auto* ps = static_cast<SurfaceState*>(wl_resource_get_user_data(ss->parent_resource));
    if (!ps) return;
    auto& v = ps->subsurface_children;
    // Move this subsurface just above the sibling in the z-order list
    auto self_it    = std::find(v.begin(), v.end(), resource);
    auto sibling_it = std::find(v.begin(), v.end(), sibling);
    if (self_it == v.end() || sibling_it == v.end()) return;
    wl_resource* tmp = *self_it;
    v.erase(self_it);
    sibling_it = std::find(v.begin(), v.end(), sibling);
    v.insert(sibling_it + 1, tmp);
}

void WaylandBridge::subsurfacePlaceBelow(wl_client* /*client*/, wl_resource* resource,
                                          wl_resource* sibling) noexcept {
    auto* ss = static_cast<SubsurfaceState*>(wl_resource_get_user_data(resource));
    if (!ss || !ss->parent_resource) return;
    auto* ps = static_cast<SurfaceState*>(wl_resource_get_user_data(ss->parent_resource));
    if (!ps) return;
    auto& v = ps->subsurface_children;
    auto self_it    = std::find(v.begin(), v.end(), resource);
    auto sibling_it = std::find(v.begin(), v.end(), sibling);
    if (self_it == v.end() || sibling_it == v.end()) return;
    wl_resource* tmp = *self_it;
    v.erase(self_it);
    sibling_it = std::find(v.begin(), v.end(), sibling);
    v.insert(sibling_it, tmp);
}

void WaylandBridge::subsurfaceSetSync(wl_client* /*client*/,
                                       wl_resource* resource) noexcept {
    auto* ss = static_cast<SubsurfaceState*>(wl_resource_get_user_data(resource));
    if (ss) ss->sync = true;
}

void WaylandBridge::subsurfaceSetDesync(wl_client* /*client*/,
                                         wl_resource* resource) noexcept {
    auto* ss = static_cast<SubsurfaceState*>(wl_resource_get_user_data(resource));
    if (!ss) return;
    ss->sync = false;
    // If there's a cached commit from when we were in sync mode, apply it now
    if (ss->has_pending_commit && ss->surface_resource) {
        auto* cs = static_cast<SurfaceState*>(
            wl_resource_get_user_data(ss->surface_resource));
        if (cs && cs->pending_buffer) {
            ss->cur_x = ss->pending_x;
            ss->cur_y = ss->pending_y;
            applyCommit(cs, static_cast<float>(ss->cur_x), static_cast<float>(ss->cur_y));
        }
        ss->has_pending_commit = false;
    }
}

// ============================================================
// zwp_text_input_manager_v3 + zwp_text_input_v3
// ============================================================

void WaylandBridge::bindTextInputManager(wl_client* client, void* data,
                                          uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client,
        &zwp_text_input_manager_v3_interface, version, id);
    wl_resource_set_implementation(res, &text_input_manager_v3_implementation, data, nullptr);
}

void WaylandBridge::textInputManagerDestroy(wl_client* /*client*/,
                                             wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::textInputManagerGetTextInput(wl_client* client, wl_resource* mgr,
                                                  uint32_t id, wl_resource* /*seat*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(mgr));
    auto* tis  = new TextInputState{ .bridge = self };

    wl_resource* ti = wl_resource_create(client, &zwp_text_input_v3_interface, 1, id);
    wl_resource_set_implementation(ti, &text_input_v3_implementation, tis,
        [](wl_resource* res) {
            auto* s = static_cast<TextInputState*>(wl_resource_get_user_data(res));
            if (s && s->bridge) {
                auto& v = s->bridge->text_input_resources_;
                v.erase(std::remove(v.begin(), v.end(), res), v.end());
            }
            delete s;
        });

    self->text_input_resources_.push_back(ti);

    // If there is already a keyboard-focused surface belonging to this client, send enter now
    if (self->keyboard_focus_surface_ &&
        wl_resource_get_client(self->keyboard_focus_surface_) == client) {
        tis->focused_surface = self->keyboard_focus_surface_;
        zwp_text_input_v3_send_enter(ti, self->keyboard_focus_surface_);
    }
}

void WaylandBridge::textInputDestroy(wl_client* /*client*/,
                                      wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::textInputEnable(wl_client* /*client*/,
                                     wl_resource* resource) noexcept {
    auto* tis = static_cast<TextInputState*>(wl_resource_get_user_data(resource));
    if (tis) tis->pending_enabled = true;
}

void WaylandBridge::textInputDisable(wl_client* /*client*/,
                                      wl_resource* resource) noexcept {
    auto* tis = static_cast<TextInputState*>(wl_resource_get_user_data(resource));
    if (tis) tis->pending_enabled = false;
}

void WaylandBridge::textInputSetSurroundingText(wl_client* /*client*/,
                                                 wl_resource* /*resource*/,
                                                 const char* /*text*/,
                                                 int32_t /*cursor*/,
                                                 int32_t /*anchor*/) noexcept {
    // Stored for IME context — ignored without an IME daemon
}

void WaylandBridge::textInputSetTextChangeCause(wl_client* /*client*/,
                                                 wl_resource* /*resource*/,
                                                 uint32_t /*cause*/) noexcept {}

void WaylandBridge::textInputSetContentType(wl_client* /*client*/,
                                             wl_resource* /*resource*/,
                                             uint32_t /*hint*/,
                                             uint32_t /*purpose*/) noexcept {}

void WaylandBridge::textInputSetCursorRectangle(wl_client* /*client*/,
                                                 wl_resource* /*resource*/,
                                                 int32_t /*x*/, int32_t /*y*/,
                                                 int32_t /*width*/, int32_t /*height*/) noexcept {}

void WaylandBridge::textInputCommit(wl_client* /*client*/,
                                     wl_resource* resource) noexcept {
    auto* tis = static_cast<TextInputState*>(wl_resource_get_user_data(resource));
    if (!tis) return;
    ++tis->serial;
    tis->enabled = tis->pending_enabled;
}

void WaylandBridge::sendTextInputEnter(wl_resource* surface) noexcept {
    if (!surface) return;
    wl_client* client = wl_resource_get_client(surface);
    for (wl_resource* ti : text_input_resources_) {
        if (wl_resource_get_client(ti) != client) continue;
        auto* tis = static_cast<TextInputState*>(wl_resource_get_user_data(ti));
        if (tis) tis->focused_surface = surface;
        zwp_text_input_v3_send_enter(ti, surface);
    }
}

void WaylandBridge::sendTextInputLeave(wl_resource* surface) noexcept {
    if (!surface) return;
    wl_client* client = wl_resource_get_client(surface);
    for (wl_resource* ti : text_input_resources_) {
        if (wl_resource_get_client(ti) != client) continue;
        auto* tis = static_cast<TextInputState*>(wl_resource_get_user_data(ti));
        if (tis) {
            tis->focused_surface = nullptr;
            tis->enabled         = false;
            tis->pending_enabled = false;
        }
        zwp_text_input_v3_send_leave(ti, surface);
    }
}

// ============================================================
// zwp_idle_inhibit_manager_v1 + zwp_idle_inhibitor_v1
// ============================================================

void WaylandBridge::bindIdleInhibitManager(wl_client* client, void* data,
                                            uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(
        client, &zwp_idle_inhibit_manager_v1_interface, version, id);
    wl_resource_set_implementation(res, &idle_inhibit_manager_implementation, data, nullptr);
}

void WaylandBridge::idleInhibitManagerDestroy(wl_client* /*client*/,
                                               wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::idleInhibitManagerCreateInhibitor(wl_client* client,
                                                       wl_resource* mgr,
                                                       uint32_t id,
                                                       wl_resource* /*surface*/) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(mgr));

    wl_resource* inhibitor = wl_resource_create(
        client, &zwp_idle_inhibitor_v1_interface, 1, id);
    wl_resource_set_implementation(inhibitor, nullptr, self,
        [](wl_resource* res) {
            // Inhibitor destroyed: decrement count, re-enable idle if now zero
            auto* br = static_cast<WaylandBridge*>(wl_resource_get_user_data(res));
            if (br && br->idle_inhibitor_count_ > 0) {
                --br->idle_inhibitor_count_;
                if (br->idle_inhibitor_count_ == 0)
                    std::cout << "[xwayland-bridge] Idle inhibition lifted.\n";
            }
        });

    ++self->idle_inhibitor_count_;
    if (self->idle_inhibitor_count_ == 1)
        std::cout << "[xwayland-bridge] Idle inhibited by client.\n";
}

void WaylandBridge::idleInhibitorDestroy(wl_client* /*client*/,
                                          wl_resource* resource) noexcept {
    wl_resource_destroy(resource); // destructor lambda handles the count
}

// ============================================================
// wl_drm (Mesa legacy EGL DRM buffer sharing)
// ============================================================

void WaylandBridge::bindWlDrm(wl_client* client, void* data,
                                uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &wl_drm_interface, version, id);
    wl_resource_set_implementation(res, &wl_drm_implementation, data, nullptr);

    // Send the DRM device path — Mesa will open this for GBM buffer allocation
    const char* drm_path = nullptr;
    if (access("/dev/dri/renderD128", R_OK) == 0)
        drm_path = "/dev/dri/renderD128";
    else if (access("/dev/drm/128", R_OK) == 0)
        drm_path = "/dev/drm/128";
    else
        drm_path = "/dev/dri/card0";

    wl_drm_send_device(res, drm_path);
    wl_drm_send_format(res, 0x34325258u); // DRM_FORMAT_XRGB8888
    wl_drm_send_format(res, 0x34325241u); // DRM_FORMAT_ARGB8888
    // Advertise PRIME capability (fd-based buffer import, no flink required)
    wl_drm_send_capabilities(res, WL_DRM_CAPABILITY_PRIME);
}

void WaylandBridge::wlDrmAuthenticate(wl_client* /*client*/,
                                       wl_resource* resource,
                                       uint32_t /*magic*/) noexcept {
    // We trust all clients — reply authenticated unconditionally
    wl_drm_send_authenticated(resource);
}

void WaylandBridge::wlDrmCreateBuffer(wl_client* /*client*/, wl_resource* resource,
                                       uint32_t /*id*/, uint32_t /*name*/,
                                       int32_t /*w*/, int32_t /*h*/,
                                       uint32_t /*stride*/, uint32_t /*format*/) noexcept {
    // Legacy flink-name path not supported (requires GEM global names = insecure)
    wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_NAME,
                           "flink buffer names not supported; use create_prime_buffer");
}

void WaylandBridge::wlDrmCreatePlanarBuffer(wl_client* /*client*/, wl_resource* resource,
                                             uint32_t /*id*/, uint32_t /*name*/,
                                             int32_t /*w*/, int32_t /*h*/,
                                             uint32_t /*format*/,
                                             int32_t /*o0*/, int32_t /*s0*/,
                                             int32_t /*o1*/, int32_t /*s1*/,
                                             int32_t /*o2*/, int32_t /*s2*/) noexcept {
    wl_resource_post_error(resource, WL_DRM_ERROR_INVALID_NAME,
                           "flink buffer names not supported; use create_prime_buffer");
}

void WaylandBridge::wlDrmCreatePrimeBuffer(wl_client* client, wl_resource* /*drm_res*/,
                                            uint32_t id, int32_t fd,
                                            int32_t width, int32_t height,
                                            uint32_t format,
                                            int32_t offset0, int32_t stride0,
                                            int32_t /*offset1*/, int32_t /*stride1*/,
                                            int32_t /*offset2*/, int32_t /*stride2*/) noexcept {
    // Same treatment as dmabuf create_immed: wrap the prime fd in a BufferState wl_buffer
    auto* buf = new BufferState{
        .fd     = fd,
        .width  = width,
        .height = height,
        .stride = stride0,
        .offset = offset0,
        .format = format
    };
    wl_resource* buf_res = wl_resource_create(client, &wl_buffer_interface, 1, id);
    wl_resource_set_implementation(buf_res, nullptr, buf,
        [](wl_resource* res) {
            auto* s = static_cast<BufferState*>(wl_resource_get_user_data(res));
            if (s && s->fd >= 0) close(s->fd);
            delete s;
        });
}

// ============================================================
// wp_single_pixel_buffer_manager_v1
// ============================================================

void WaylandBridge::bindSinglePixelBufferManager(wl_client* client, void* data,
                                                   uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client,
        &wp_single_pixel_buffer_manager_v1_interface, version, id);
    wl_resource_set_implementation(res, &spbm_implementation, data, nullptr);
}

void WaylandBridge::spbmDestroy(wl_client* /*client*/,
                                 wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::spbmCreateU32RgbaBuffer(wl_client* client, wl_resource* /*mgr*/,
                                              uint32_t id,
                                              uint32_t r, uint32_t g,
                                              uint32_t b, uint32_t a) noexcept {
    // Convert [0, UINT32_MAX] → [0, 255] by taking the top 8 bits.
    uint8_t r8 = static_cast<uint8_t>(r >> 24);
    uint8_t g8 = static_cast<uint8_t>(g >> 24);
    uint8_t b8 = static_cast<uint8_t>(b >> 24);
    uint8_t a8 = static_cast<uint8_t>(a >> 24);
    uint32_t argb = (static_cast<uint32_t>(a8) << 24)
                  | (static_cast<uint32_t>(r8) << 16)
                  | (static_cast<uint32_t>(g8) <<  8)
                  |  static_cast<uint32_t>(b8);

    auto* buf = new BufferState{};
    buf->width       = 1;
    buf->height      = 1;
    buf->solid_color = true;
    buf->solid_argb  = argb;

    wl_resource* buffer_res = wl_resource_create(client, &wl_buffer_interface, 1, id);
    wl_resource_set_implementation(buffer_res, nullptr, buf,
        [](wl_resource* res) {
            delete static_cast<BufferState*>(wl_resource_get_user_data(res));
        });
}

// ============================================================
// zwlr_layer_shell_v1
// ============================================================

void WaylandBridge::bindLayerShell(wl_client* client, void* data,
                                    uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &zwlr_layer_shell_v1_interface, version, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &layer_shell_implementation, data, nullptr);
}

void WaylandBridge::layerShellGetLayerSurface(wl_client* client, wl_resource* shell_res,
    uint32_t id, wl_resource* surface_res, wl_resource* /*output*/,
    uint32_t layer, const char* /*ns*/) noexcept {
    auto* br = static_cast<WaylandBridge*>(wl_resource_get_user_data(shell_res));
    auto* ss = static_cast<SurfaceState*>(wl_resource_get_user_data(surface_res));

    wl_resource* layer_res = wl_resource_create(client, &zwlr_layer_surface_v1_interface,
                                                 wl_resource_get_version(shell_res), id);
    if (!layer_res) { wl_client_post_no_memory(client); return; }

    auto* lss = new LayerSurfaceState{};
    lss->bridge        = br;
    lss->surface_state = ss;
    lss->layer         = layer;

    wl_resource_set_implementation(layer_res, &layer_surface_implementation, lss,
        [](wl_resource* r) {
            delete static_cast<LayerSurfaceState*>(wl_resource_get_user_data(r));
        });

    ss->layer_surface = layer_res;
}

void WaylandBridge::layerShellDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

// ============================================================
// zwlr_layer_surface_v1
// ============================================================

void WaylandBridge::layerSurfaceSetSize(wl_client* /*client*/, wl_resource* resource,
                                         uint32_t width, uint32_t height) noexcept {
    auto* lss = static_cast<LayerSurfaceState*>(wl_resource_get_user_data(resource));
    lss->pending_w = width;
    lss->pending_h = height;
}

void WaylandBridge::layerSurfaceSetAnchor(wl_client* /*client*/, wl_resource* resource,
                                           uint32_t anchor) noexcept {
    auto* lss = static_cast<LayerSurfaceState*>(wl_resource_get_user_data(resource));
    lss->anchor = anchor;
}

void WaylandBridge::layerSurfaceSetExclusiveZone(wl_client* /*client*/, wl_resource* resource,
                                                   int32_t zone) noexcept {
    auto* lss = static_cast<LayerSurfaceState*>(wl_resource_get_user_data(resource));
    lss->exclusive_zone = zone;
}

void WaylandBridge::layerSurfaceSetMargin(wl_client* /*client*/, wl_resource* resource,
                                           int32_t top, int32_t right,
                                           int32_t bottom, int32_t left) noexcept {
    auto* lss = static_cast<LayerSurfaceState*>(wl_resource_get_user_data(resource));
    lss->margin_top    = top;
    lss->margin_right  = right;
    lss->margin_bottom = bottom;
    lss->margin_left   = left;
}

void WaylandBridge::layerSurfaceSetKeyboardInteractivity(wl_client* /*client*/,
    wl_resource* /*resource*/, uint32_t /*interactivity*/) noexcept {
    // Keyboard focus for layer surfaces is handled by the normal focus mechanism.
}

void WaylandBridge::layerSurfaceGetPopup(wl_client* /*client*/,
    wl_resource* /*resource*/, wl_resource* /*popup*/) noexcept {
    // Popups parented to layer surfaces — not yet implemented.
}

void WaylandBridge::layerSurfaceAckConfigure(wl_client* /*client*/, wl_resource* resource,
                                              uint32_t /*serial*/) noexcept {
    auto* lss = static_cast<LayerSurfaceState*>(wl_resource_get_user_data(resource));
    lss->acked = true;
}

void WaylandBridge::layerSurfaceDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    auto* lss = static_cast<LayerSurfaceState*>(wl_resource_get_user_data(resource));
    if (lss && lss->surface_state)
        lss->surface_state->layer_surface = nullptr;
    wl_resource_destroy(resource);
}

// ============================================================
// wp_cursor_shape_manager_v1 + wp_cursor_shape_device_v1
// ============================================================

void WaylandBridge::bindCursorShapeManager(wl_client* client, void* data,
                                            uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &wp_cursor_shape_manager_v1_interface,
                                           static_cast<int>(version), id);
    wl_resource_set_implementation(res, &cursor_shape_manager_implementation, data, nullptr);
}

void WaylandBridge::cursorShapeManagerDestroy(wl_client* /*client*/,
                                               wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::cursorShapeManagerGetPointer(wl_client* client, wl_resource* manager,
                                                  uint32_t id,
                                                  wl_resource* /*pointer*/) noexcept {
    auto* br = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager));
    auto* ds = new CursorShapeDeviceState{br};
    wl_resource* dev = wl_resource_create(client, &wp_cursor_shape_device_v1_interface, 1, id);
    wl_resource_set_implementation(dev, &cursor_shape_device_implementation, ds,
        [](wl_resource* r) {
            delete static_cast<CursorShapeDeviceState*>(wl_resource_get_user_data(r));
        });
}

void WaylandBridge::cursorShapeManagerGetTabletToolV2(wl_client* /*client*/,
                                                       wl_resource* /*manager*/,
                                                       uint32_t /*id*/,
                                                       wl_resource* /*tablet_tool*/) noexcept {
    // Tablet tools are not implemented.
}

void WaylandBridge::cursorShapeDeviceDestroy(wl_client* /*client*/,
                                              wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::cursorShapeDeviceSetShape(wl_client* /*client*/, wl_resource* resource,
                                               uint32_t /*serial*/, uint32_t shape) noexcept {
    auto* ds = static_cast<CursorShapeDeviceState*>(wl_resource_get_user_data(resource));
    if (!ds || !ds->bridge || !ds->bridge->ipc_connected_) return;

    // Validate: protocol defines shape values 1..36.
    if (shape < 1 || shape > 36) {
        wl_resource_post_error(resource, WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE,
                               "invalid cursor shape value %u", shape);
        return;
    }

    shared::XBusMessage msg{};
    std::strncpy(msg.service_name_, "com.freebsdx.Input", sizeof(msg.service_name_) - 1);
    msg.opcode_ = 0x03; // set cursor shape
    std::memcpy(msg.payload_, &shape, sizeof(shape));
    (void)ds->bridge->xserver_client_->sendMessage(
        {reinterpret_cast<const uint8_t*>(&msg), sizeof(msg)});
}

// ============================================================
// wp_fractional_scale_manager_v1 + wp_fractional_scale_v1
// ============================================================

void WaylandBridge::bindFractionalScaleManager(wl_client* client, void* data,
                                                uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &wp_fractional_scale_manager_v1_interface,
                                           static_cast<int>(version), id);
    wl_resource_set_implementation(res, &fractional_scale_manager_implementation, data, nullptr);
}

void WaylandBridge::fractionalScaleManagerDestroy(wl_client* /*client*/,
                                                   wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::fractionalScaleManagerGetFractionalScale(wl_client* client,
                                                              wl_resource* manager,
                                                              uint32_t id,
                                                              wl_resource* surface_res) noexcept {
    auto* br = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager));
    auto* ss = static_cast<SurfaceState*>(wl_resource_get_user_data(surface_res));
    if (!ss) return;

    if (ss->fractional_scale_res) {
        wl_resource_post_error(manager,
            WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
            "wp_fractional_scale_v1 already exists for this surface");
        return;
    }

    auto* fss = new FractionalScaleState{br, ss};
    wl_resource* fs_res = wl_resource_create(client, &wp_fractional_scale_v1_interface, 1, id);
    wl_resource_set_implementation(fs_res, &fractional_scale_implementation, fss,
        [](wl_resource* r) {
            delete static_cast<FractionalScaleState*>(wl_resource_get_user_data(r));
        });
    ss->fractional_scale_res = fs_res;

    // Send initial preferred scale: 120 = 1.0x (denominator is always 120 per protocol).
    wp_fractional_scale_v1_send_preferred_scale(fs_res, 120);
}

void WaylandBridge::fractionalScaleDestroy(wl_client* /*client*/,
                                            wl_resource* resource) noexcept {
    auto* fss = static_cast<FractionalScaleState*>(wl_resource_get_user_data(resource));
    if (fss && fss->surface_state)
        fss->surface_state->fractional_scale_res = nullptr;
    wl_resource_destroy(resource);
}

// ============================================================
// xdg_wm_dialog_v1
// ============================================================

void WaylandBridge::bindXdgWmDialog(wl_client* client, void* data,
                                     uint32_t version, uint32_t id) noexcept {
    wl_resource* res = wl_resource_create(client, &xdg_wm_dialog_v1_interface,
                                           static_cast<int>(version), id);
    wl_resource_set_implementation(res, &xdg_wm_dialog_implementation, data, nullptr);
}

void WaylandBridge::wmDialogDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    wl_resource_destroy(resource);
}

void WaylandBridge::wmDialogGetXdgDialog(wl_client* client, wl_resource* manager,
                                          uint32_t id, wl_resource* toplevel) noexcept {
    auto* br = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager));
    auto* ss = static_cast<SurfaceState*>(wl_resource_get_user_data(toplevel));
    if (!ss) return;

    // Error if a dialog already exists for this toplevel.
    if (ss->dialog_res) {
        wl_resource_post_error(manager, XDG_WM_DIALOG_V1_ERROR_ALREADY_USED,
            "xdg_dialog_v1 already exists for this toplevel");
        return;
    }

    auto* ds = new DialogState{br, ss, false};
    wl_resource* dialog = wl_resource_create(client, &xdg_dialog_v1_interface, 1, id);
    wl_resource_set_implementation(dialog, &xdg_dialog_implementation, ds,
        [](wl_resource* r) { delete static_cast<DialogState*>(wl_resource_get_user_data(r)); });
    ss->dialog_res = dialog;
}

void WaylandBridge::xdgDialogDestroy(wl_client* /*client*/, wl_resource* resource) noexcept {
    auto* ds = static_cast<DialogState*>(wl_resource_get_user_data(resource));
    if (ds && ds->surface_state)
        ds->surface_state->dialog_res = nullptr;
    wl_resource_destroy(resource);
}

void WaylandBridge::xdgDialogSetModal(wl_client* /*client*/, wl_resource* resource) noexcept {
    auto* ds = static_cast<DialogState*>(wl_resource_get_user_data(resource));
    if (!ds || ds->is_modal) return;
    ds->is_modal = true;
    if (ds->bridge && ds->bridge->ipc_connected_) {
        shared::RenderMessage msg{
            .surface_id_ = ds->surface_state ? ds->surface_state->id : 0,
            .status_     = shared::RenderMessageStatus::DialogModalSet,
            .width_      = 0,
            .height_     = 0,
            .stride_     = 0
        };
        (void)ds->bridge->xserver_client_->sendMessage(
            {reinterpret_cast<const uint8_t*>(&msg), sizeof(msg)});
    }
}

void WaylandBridge::xdgDialogUnsetModal(wl_client* /*client*/, wl_resource* resource) noexcept {
    auto* ds = static_cast<DialogState*>(wl_resource_get_user_data(resource));
    if (!ds || !ds->is_modal) return;
    ds->is_modal = false;
    if (ds->bridge && ds->bridge->ipc_connected_) {
        shared::RenderMessage msg{
            .surface_id_ = ds->surface_state ? ds->surface_state->id : 0,
            .status_     = shared::RenderMessageStatus::DialogModalUnset,
            .width_      = 0,
            .height_     = 0,
            .stride_     = 0
        };
        (void)ds->bridge->xserver_client_->sendMessage(
            {reinterpret_cast<const uint8_t*>(&msg), sizeof(msg)});
    }
}

// ============================================================
// wp_security_context_manager_v1
// ============================================================

void WaylandBridge::bindSecurityContextManager(wl_client* client, void* data,
                                                uint32_t version, uint32_t id) noexcept {
    auto* self = static_cast<WaylandBridge*>(data);
    // Prevent nesting: deny if this client already has a security context.
    if (self->client_security_contexts_.count(client)) {
        wl_client_post_implementation_error(client,
            "wp_security_context_manager_v1: nested security contexts are forbidden");
        return;
    }
    wl_resource* res = wl_resource_create(
        client, &wp_security_context_manager_v1_interface, (int)version, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &security_context_manager_implementation, self, nullptr);
}

void WaylandBridge::securityContextManagerDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res);
}

// Event-loop callback: accept a new Wayland client from the security-context listen socket.
static int securityContextAcceptClient(int listen_fd, uint32_t mask, void* data) noexcept {
    if (!(mask & WL_EVENT_READABLE)) return 0;
    auto* ctx = static_cast<SecurityContextState*>(data);
    if (!ctx->bridge) return 0;

    int client_fd = accept4(listen_fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (client_fd < 0) return 0;

    wl_client* new_client = wl_client_create(ctx->bridge->rawDisplay(), client_fd);
    if (!new_client) { ::close(client_fd); return 0; }

    ctx->bridge->onSecurityContextClientConnect(
        new_client, ctx->app_id, ctx->sandbox_engine, ctx->instance_id);
    return 0;
}

// Event-loop callback: close_fd hung up — stop accepting new connections.
static int securityContextCloseFd(int /*fd*/, uint32_t mask, void* data) noexcept {
    if (!(mask & WL_EVENT_HANGUP)) return 0;
    auto* ctx = static_cast<SecurityContextState*>(data);
    if (ctx->listen_source) {
        wl_event_source_remove(ctx->listen_source);
        ctx->listen_source = nullptr;
    }
    if (ctx->listen_fd >= 0) {
        ::close(ctx->listen_fd);
        ctx->listen_fd = -1;
    }
    return 0;
}

void WaylandBridge::securityContextManagerCreateListener(wl_client* client,
                                                          wl_resource* manager_res,
                                                          uint32_t id,
                                                          int listen_fd,
                                                          int close_fd) noexcept {
    auto* self = static_cast<WaylandBridge*>(wl_resource_get_user_data(manager_res));

    wl_resource* ctx_res = wl_resource_create(
        client, &wp_security_context_v1_interface, 1, id);
    if (!ctx_res) {
        wl_client_post_no_memory(client);
        ::close(listen_fd);
        ::close(close_fd);
        return;
    }

    auto* ctx = new SecurityContextState{};
    ctx->bridge    = self;
    ctx->listen_fd = listen_fd;
    ctx->close_fd  = close_fd;

    wl_resource_set_implementation(ctx_res, &security_context_implementation, ctx,
        [](wl_resource* res) {
            auto* c = static_cast<SecurityContextState*>(wl_resource_get_user_data(res));
            if (c->listen_source) { wl_event_source_remove(c->listen_source); }
            if (c->close_source)  { wl_event_source_remove(c->close_source);  }
            if (c->listen_fd >= 0) ::close(c->listen_fd);
            if (c->close_fd  >= 0) ::close(c->close_fd);
            delete c;
        });
}

void WaylandBridge::securityContextDestroy(wl_client*, wl_resource* res) noexcept {
    wl_resource_destroy(res);
}

void WaylandBridge::securityContextSetSandboxEngine(wl_client*, wl_resource* res,
                                                      const char* name) noexcept {
    auto* ctx = static_cast<SecurityContextState*>(wl_resource_get_user_data(res));
    if (ctx->committed) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED,
                               "security context already committed");
        return;
    }
    if (!ctx->sandbox_engine.empty()) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET,
                               "sandbox_engine already set");
        return;
    }
    ctx->sandbox_engine = name ? name : "";
}

void WaylandBridge::securityContextSetAppId(wl_client*, wl_resource* res,
                                             const char* app_id) noexcept {
    auto* ctx = static_cast<SecurityContextState*>(wl_resource_get_user_data(res));
    if (ctx->committed) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED,
                               "security context already committed");
        return;
    }
    if (!ctx->app_id.empty()) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET,
                               "app_id already set");
        return;
    }
    ctx->app_id = app_id ? app_id : "";
}

void WaylandBridge::securityContextSetInstanceId(wl_client*, wl_resource* res,
                                                   const char* instance_id) noexcept {
    auto* ctx = static_cast<SecurityContextState*>(wl_resource_get_user_data(res));
    if (ctx->committed) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED,
                               "security context already committed");
        return;
    }
    if (!ctx->instance_id.empty()) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_SET,
                               "instance_id already set");
        return;
    }
    ctx->instance_id = instance_id ? instance_id : "";
}

void WaylandBridge::securityContextCommit(wl_client*, wl_resource* res) noexcept {
    auto* ctx = static_cast<SecurityContextState*>(wl_resource_get_user_data(res));
    if (ctx->committed) {
        wl_resource_post_error(res, WP_SECURITY_CONTEXT_V1_ERROR_ALREADY_USED,
                               "security context already committed");
        return;
    }
    ctx->committed = true;

    // Start listening for new Wayland clients on listen_fd.
    wl_event_loop* loop = wl_display_get_event_loop(ctx->bridge->display_.get());
    ctx->listen_source = wl_event_loop_add_fd(loop, ctx->listen_fd,
                                               WL_EVENT_READABLE,
                                               securityContextAcceptClient, ctx);
    // Monitor close_fd for POLLHUP to know when to stop accepting.
    ctx->close_source = wl_event_loop_add_fd(loop, ctx->close_fd,
                                              WL_EVENT_HANGUP,
                                              securityContextCloseFd, ctx);

    std::cout << "[xwayland-bridge] SecurityContext committed: app_id=" << ctx->app_id
              << " engine=" << ctx->sandbox_engine << "\n";
}

void WaylandBridge::onSecurityContextClientConnect(
        wl_client* client,
        const std::string& app_id,
        const std::string& sandbox_engine,
        const std::string& instance_id) noexcept {
    client_security_contexts_[client] = {app_id, sandbox_engine, instance_id};

    // Register a destroy listener so we clean up the map when the client disconnects.
    auto* dl = new ClientSecCtxDestroyListener{};
    dl->bridge  = this;
    dl->client  = client;
    dl->listener.notify = [](wl_listener* l, void*) noexcept {
        auto* dl2 = reinterpret_cast<ClientSecCtxDestroyListener*>(
            reinterpret_cast<char*>(l) - offsetof(ClientSecCtxDestroyListener, listener));
        dl2->bridge->client_security_contexts_.erase(dl2->client);
        delete dl2;
    };
    wl_client_add_destroy_late_listener(client, &dl->listener);
}

// ============================================================
// ext-idle-notify-v1 implementation
// ============================================================

static const struct ext_idle_notifier_v1_interface ext_idle_notifier_implementation = {
    .destroy              = WaylandBridge::extIdleNotifierDestroy,
    .get_idle_notification = WaylandBridge::extIdleNotifierGetNotification,
};

static const struct ext_idle_notification_v1_interface ext_idle_notification_implementation = {
    .destroy = WaylandBridge::extIdleNotificationDestroy,
};

void WaylandBridge::bindExtIdleNotifier(wl_client* client, void* data,
                                         uint32_t version, uint32_t id) {
    auto* self = static_cast<WaylandBridge*>(data);
    wl_resource* res = wl_resource_create(client,
        &ext_idle_notifier_v1_interface, static_cast<int>(version), id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &ext_idle_notifier_implementation, self, nullptr);
}

void WaylandBridge::extIdleNotifierGetNotification(wl_client* client,
                                                     wl_resource* notifier_res,
                                                     uint32_t new_id,
                                                     uint32_t timeout_msec,
                                                     wl_resource* /*seat*/) {
    auto* self = static_cast<WaylandBridge*>(
        wl_resource_get_user_data(notifier_res));
    wl_resource* notif_res = wl_resource_create(client,
        &ext_idle_notification_v1_interface, 1, new_id);
    if (!notif_res) { wl_client_post_no_memory(client); return; }

    // Store pointer back to the bridge and a reference into the vector.
    // We set user_data to the bridge so we can find the notification on destroy.
    wl_resource_set_implementation(notif_res, &ext_idle_notification_implementation,
                                   self, nullptr);

    // Set destructor to remove notification from vector when resource is destroyed
    wl_resource_set_destructor(notif_res, [](wl_resource* r) {
        auto* bridge = static_cast<WaylandBridge*>(wl_resource_get_user_data(r));
        if (!bridge) return;
        auto& nots = bridge->idle_notifications_;
        nots.erase(std::remove_if(nots.begin(), nots.end(),
            [r](const IdleNotification& n) { return n.resource == r; }),
            nots.end());
    });

    self->idle_notifications_.push_back({notif_res, timeout_msec, false});
}

void WaylandBridge::extIdleNotifierDestroy(wl_client*, wl_resource* res) {
    wl_resource_destroy(res);
}

void WaylandBridge::extIdleNotificationDestroy(wl_client*, wl_resource* res) {
    wl_resource_destroy(res);
}

void WaylandBridge::notifyActivity() noexcept {
    // Get current time in ms
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t now_ms = static_cast<uint32_t>(
        (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    last_activity_ms_ = now_ms;

    for (auto& n : idle_notifications_) {
        if (n.idled) {
            ext_idle_notification_v1_send_resumed(n.resource);
            n.idled = false;
        }
    }
}

void WaylandBridge::pollIdleNotifications(uint32_t now_ms) noexcept {
    if (idle_notifications_.empty()) return;
    uint32_t elapsed = now_ms - last_activity_ms_;
    for (auto& n : idle_notifications_) {
        if (!n.idled && elapsed >= n.timeout_msec) {
            ext_idle_notification_v1_send_idled(n.resource);
            n.idled = true;
        }
    }
}

} // namespace fbx::xwayland
