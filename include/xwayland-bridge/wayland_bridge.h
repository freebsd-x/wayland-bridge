#pragma once

#include <memory>
#include <string>
#include <expected>
#include <system_error>
#include <utility>
#include <unordered_map>
#include <vector>
#include <wayland-server.h>
#include "xserver/ipc_socket.h"
#include "shared/protocol.h"

namespace fbx::xwayland {

enum class BridgeError {
    DISPLAY_CREATION_ERROR,
    SOCKET_CREATION_ERROR,
    EVENT_LOOP_ERROR,
    IPC_CONNECTION_ERROR
};

struct WlDisplayDeleter {
    void operator()(wl_display* display) const noexcept;
};

using UniqueWlDisplay = std::unique_ptr<wl_display, WlDisplayDeleter>;

// Forward declarations — these structs are defined later in this header
// but are used as member types or parameter types inside WaylandBridge.
struct SurfaceState;
struct OutputGlobalData;
struct SubsurfaceState;
struct PositionerState;
struct PopupState;

class WaylandBridge {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<WaylandBridge>, BridgeError> create() noexcept;

    ~WaylandBridge() noexcept;

    WaylandBridge(const WaylandBridge&) = delete;
    WaylandBridge& operator=(const WaylandBridge&) = delete;

    WaylandBridge(WaylandBridge&&) noexcept = default;
    WaylandBridge& operator=(WaylandBridge&&) noexcept = default;

    [[nodiscard]] int getEventFd() const noexcept;
    [[nodiscard]] int ipcFd() const noexcept;
    [[nodiscard]] const std::string& getSocketName() const noexcept { return socket_name_; }

    void dispatch() noexcept;
    void flush() noexcept;

    // Called from the main loop when data arrives on the IPC fd
    void handleIpcMessage(const std::vector<uint8_t>& data, int fd) noexcept;

    // --- wl_compositor ---
    static void bindCompositor(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void createSurface(wl_client*, wl_resource*, uint32_t) noexcept;

    // --- wl_shm ---
    static void bindShm(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void shmCreatePool(wl_client*, wl_resource*, uint32_t, int32_t, int32_t) noexcept;
    static void shmPoolCreateBuffer(wl_client*, wl_resource*, uint32_t, int32_t,
                                    int32_t, int32_t, int32_t, uint32_t) noexcept;
    static void shmPoolResize(wl_client*, wl_resource*, int32_t) noexcept;
    static void shmPoolDestroy(wl_client*, wl_resource*) noexcept;

    // --- zwp_linux_dmabuf_v1 ---
    static void bindLinuxDmabuf(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void dmabufDestroy(wl_client*, wl_resource*) noexcept;
    static void dmabufCreateParams(wl_client*, wl_resource*, uint32_t) noexcept;
    static void dmabufGetDefaultFeedback(wl_client*, wl_resource*, uint32_t) noexcept;
    static void dmabufGetSurfaceFeedback(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void dmabufParamsDestroy(wl_client*, wl_resource*) noexcept;
    static void dmabufParamsAdd(wl_client*, wl_resource*, int32_t, uint32_t,
                                uint32_t, uint32_t, uint32_t, uint32_t) noexcept;
    static void dmabufParamsCreate(wl_client*, wl_resource*, int32_t, int32_t,
                                   uint32_t, uint32_t) noexcept;
    static void dmabufParamsCreateImmed(wl_client*, wl_resource*, uint32_t,
                                        int32_t, int32_t, uint32_t, uint32_t) noexcept;
    // --- zwp_linux_dmabuf_feedback_v1 ---
    static void dmabufFeedbackDestroy(wl_client*, wl_resource*) noexcept;
    void sendDmabufFeedback(wl_resource* feedback_res) noexcept;
    void initDmabufFeedback() noexcept;

    // --- wl_surface ---
    static void surfaceDestroy(wl_client*, wl_resource*) noexcept;
    static void surfaceAttach(wl_client*, wl_resource*, wl_resource*, int32_t, int32_t) noexcept;
    static void surfaceCommit(wl_client*, wl_resource*) noexcept;
    static void surfaceFrame(wl_client*, wl_resource*, uint32_t) noexcept;

    // --- xdg_wm_base ---
    static void bindXdgShell(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void xdgWmBaseGetXdgSurface(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void xdgWmBasePong(wl_client*, wl_resource*, uint32_t) noexcept;

    // --- xdg_surface ---
    static void xdgSurfaceGetToplevel(wl_client*, wl_resource*, uint32_t) noexcept;
    static void xdgSurfaceGetPopup(wl_client*, wl_resource*, uint32_t,
                                    wl_resource*, wl_resource*) noexcept;
    static void xdgSurfaceSetWindowGeometry(wl_client*, wl_resource*,
                                             int32_t x, int32_t y,
                                             int32_t w, int32_t h) noexcept;
    static void xdgSurfaceAckConfigure(wl_client*, wl_resource*, uint32_t) noexcept;

    // --- xdg_toplevel ---
    static void xdgToplevelSetTitle(wl_client*, wl_resource*, const char*) noexcept;
    static void xdgToplevelSetAppId(wl_client*, wl_resource*, const char*) noexcept;
    static void xdgToplevelSetParent(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void xdgToplevelResize(wl_client*, wl_resource*, wl_resource*, uint32_t, uint32_t) noexcept;

    // --- xdg_positioner ---
    static void xdgWmBaseCreatePositioner(wl_client*, wl_resource*, uint32_t) noexcept;
    static void positonerDestroy(wl_client*, wl_resource*) noexcept;
    static void positionerSetSize(wl_client*, wl_resource*, int32_t, int32_t) noexcept;
    static void positionerSetAnchorRect(wl_client*, wl_resource*,
                                         int32_t, int32_t, int32_t, int32_t) noexcept;
    static void positionerSetAnchor(wl_client*, wl_resource*, uint32_t) noexcept;
    static void positionerSetGravity(wl_client*, wl_resource*, uint32_t) noexcept;
    static void positionerSetConstraintAdjustment(wl_client*, wl_resource*, uint32_t) noexcept;
    static void positionerSetOffset(wl_client*, wl_resource*, int32_t, int32_t) noexcept;
    static void positionerSetReactive(wl_client*, wl_resource*) noexcept;
    static void positionerSetParentSize(wl_client*, wl_resource*, int32_t, int32_t) noexcept;
    static void positionerSetParentConfigure(wl_client*, wl_resource*, uint32_t) noexcept;

    // --- xdg_popup ---
    static void xdgPopupDestroy(wl_client*, wl_resource*) noexcept;
    static void xdgPopupGrab(wl_client*, wl_resource*, wl_resource*, uint32_t) noexcept;
    static void xdgPopupReposition(wl_client*, wl_resource*, wl_resource*, uint32_t) noexcept;

    // --- wl_seat ---
    static void bindSeat(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void seatGetPointer(wl_client*, wl_resource*, uint32_t) noexcept;
    static void seatGetKeyboard(wl_client*, wl_resource*, uint32_t) noexcept;
    static void seatGetTouch(wl_client*, wl_resource*, uint32_t) noexcept;
    static void seatRelease(wl_client*, wl_resource*) noexcept;

    // --- wl_output ---
    static void bindOutput(wl_client*, void*, uint32_t, uint32_t) noexcept;

    // Sync the set of wl_output globals to the new physical layout.
    // Adds globals for new outputs, removes globals for removed outputs,
    // and re-sends mode/done to all already-bound resources.
    void setOutputLayout(const std::vector<shared::DisplayOutputInfo>& outputs) noexcept;

    // --- zxdg_output_manager_v1 ---
    static void bindXdgOutputManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void xdgOutputManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void xdgOutputManagerGetXdgOutput(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void xdgOutputDestroy(wl_client*, wl_resource*) noexcept;

    // --- zwp_relative_pointer_manager_v1 ---
    static void bindRelativePointerManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void relativePointerManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void relativePointerManagerGetRelativePointer(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void relativePointerDestroy(wl_client*, wl_resource*) noexcept;

    // --- zwp_pointer_constraints_v1 ---
    static void bindPointerConstraints(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void pointerConstraintsDestroy(wl_client*, wl_resource*) noexcept;
    static void pointerConstraintsLockPointer(wl_client*, wl_resource*, uint32_t,
                                               wl_resource*, wl_resource*, wl_resource*, uint32_t) noexcept;
    static void pointerConstraintsConfinePointer(wl_client*, wl_resource*, uint32_t,
                                                  wl_resource*, wl_resource*, wl_resource*, uint32_t) noexcept;
    static void lockedPointerDestroy(wl_client*, wl_resource*) noexcept;
    static void lockedPointerSetCursorPositionHint(wl_client*, wl_resource*,
                                                    wl_fixed_t, wl_fixed_t) noexcept;
    static void lockedPointerSetRegion(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void confinedPointerDestroy(wl_client*, wl_resource*) noexcept;
    static void confinedPointerSetRegion(wl_client*, wl_resource*, wl_resource*) noexcept;

    // --- wp_presentation ---
    static void bindPresentation(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void presentationDestroy(wl_client*, wl_resource*) noexcept;
    static void presentationFeedback(wl_client*, wl_resource*, wl_resource*, uint32_t) noexcept;

    // --- zxdg_decoration_manager_v1 ---
    static void bindDecorationManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void decorationManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void decorationManagerGetToplevelDecoration(wl_client*, wl_resource*,
                                                       uint32_t, wl_resource*) noexcept;

    // --- zxdg_toplevel_decoration_v1 ---
    static void toplevelDecorationDestroy(wl_client*, wl_resource*) noexcept;
    static void toplevelDecorationSetMode(wl_client*, wl_resource*, uint32_t) noexcept;
    static void toplevelDecorationUnsetMode(wl_client*, wl_resource*) noexcept;

    // --- zwlr_screencopy_manager_v1 ---
    static void bindScreencopyManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void screencopyManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void screencopyManagerCaptureOutput(wl_client*, wl_resource*, uint32_t,
                                               int32_t, wl_resource*) noexcept;
    static void screencopyManagerCaptureOutputRegion(wl_client*, wl_resource*, uint32_t,
                                                     int32_t, wl_resource*,
                                                     int32_t, int32_t, int32_t, int32_t) noexcept;

    // --- zwlr_screencopy_frame_v1 ---
    static void screencopyFrameDestroy(wl_client*, wl_resource*) noexcept;
    static void screencopyFrameCopy(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void screencopyFrameCopyWithDamage(wl_client*, wl_resource*, wl_resource*) noexcept;

    // --- wp_viewporter ---
    static void bindViewporter(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void viewporterDestroy(wl_client*, wl_resource*) noexcept;
    static void viewporterGetViewport(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;

    // --- wp_viewport ---
    static void viewportDestroy(wl_client*, wl_resource*) noexcept;
    static void viewportSetSource(wl_client*, wl_resource*,
                                  wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t) noexcept;
    static void viewportSetDestination(wl_client*, wl_resource*, int32_t, int32_t) noexcept;

    // --- xdg_activation_v1 ---
    static void bindActivation(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void activationDestroy(wl_client*, wl_resource*) noexcept;
    static void activationGetToken(wl_client*, wl_resource*, uint32_t) noexcept;
    static void activationActivate(wl_client*, wl_resource*, const char*, wl_resource*) noexcept;

    // --- xdg_activation_token_v1 ---
    static void activationTokenSetSerial(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void activationTokenSetAppId(wl_client*, wl_resource*, const char*) noexcept;
    static void activationTokenSetSurface(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void activationTokenCommit(wl_client*, wl_resource*) noexcept;
    static void activationTokenDestroy(wl_client*, wl_resource*) noexcept;

    // --- wl_data_device_manager ---
    static void bindDataDeviceManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void dataDeviceManagerCreateDataSource(wl_client*, wl_resource*, uint32_t) noexcept;
    static void dataDeviceManagerGetDataDevice(wl_client*, wl_resource*, uint32_t,
                                               wl_resource*) noexcept;

    // --- wl_data_source ---
    static void dataSourceOffer(wl_client*, wl_resource*, const char*) noexcept;
    static void dataSourceDestroy(wl_client*, wl_resource*) noexcept;
    static void dataSourceSetActions(wl_client*, wl_resource*, uint32_t) noexcept;

    // --- wl_data_device ---
    static void dataDeviceStartDrag(wl_client*, wl_resource*, wl_resource*,
                                    wl_resource*, wl_resource*, uint32_t) noexcept;
    static void dataDeviceSetSelection(wl_client*, wl_resource*, wl_resource*,
                                       uint32_t) noexcept;
    static void dataDeviceRelease(wl_client*, wl_resource*) noexcept;

    // --- wl_data_offer ---
    static void dataOfferAccept(wl_client*, wl_resource*, uint32_t, const char*) noexcept;
    static void dataOfferReceive(wl_client*, wl_resource*, const char*, int32_t) noexcept;
    static void dataOfferDestroy(wl_client*, wl_resource*) noexcept;
    static void dataOfferFinish(wl_client*, wl_resource*) noexcept;
    static void dataOfferSetActions(wl_client*, wl_resource*, uint32_t, uint32_t) noexcept;

    // --- zwp_idle_inhibit_manager_v1 ---
    static void bindIdleInhibitManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void idleInhibitManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void idleInhibitManagerCreateInhibitor(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void idleInhibitorDestroy(wl_client*, wl_resource*) noexcept;

    // --- wl_drm (Mesa legacy DRM buffer sharing) ---
    static void bindWlDrm(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void wlDrmAuthenticate(wl_client*, wl_resource*, uint32_t) noexcept;
    static void wlDrmCreateBuffer(wl_client*, wl_resource*, uint32_t, uint32_t,
                                   int32_t, int32_t, uint32_t, uint32_t) noexcept;
    static void wlDrmCreatePlanarBuffer(wl_client*, wl_resource*, uint32_t, uint32_t,
                                         int32_t, int32_t, uint32_t,
                                         int32_t, int32_t, int32_t, int32_t,
                                         int32_t, int32_t) noexcept;
    static void wlDrmCreatePrimeBuffer(wl_client*, wl_resource*, uint32_t, int32_t,
                                        int32_t, int32_t, uint32_t,
                                        int32_t, int32_t, int32_t, int32_t,
                                        int32_t, int32_t) noexcept;

    // --- zwp_text_input_manager_v3 ---
    static void bindTextInputManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void textInputManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void textInputManagerGetTextInput(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;

    // --- zwp_text_input_v3 ---
    static void textInputDestroy(wl_client*, wl_resource*) noexcept;
    static void textInputEnable(wl_client*, wl_resource*) noexcept;
    static void textInputDisable(wl_client*, wl_resource*) noexcept;
    static void textInputSetSurroundingText(wl_client*, wl_resource*, const char*, int32_t, int32_t) noexcept;
    static void textInputSetTextChangeCause(wl_client*, wl_resource*, uint32_t) noexcept;
    static void textInputSetContentType(wl_client*, wl_resource*, uint32_t, uint32_t) noexcept;
    static void textInputSetCursorRectangle(wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) noexcept;
    static void textInputCommit(wl_client*, wl_resource*) noexcept;

    // Send text_input enter/leave to all matching resources when keyboard focus changes
    void sendTextInputEnter(wl_resource* surface) noexcept;
    void sendTextInputLeave(wl_resource* surface) noexcept;

    // --- wp_single_pixel_buffer_manager_v1 ---
    static void bindSinglePixelBufferManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void spbmDestroy(wl_client*, wl_resource*) noexcept;
    static void spbmCreateU32RgbaBuffer(wl_client*, wl_resource*, uint32_t,
                                         uint32_t, uint32_t, uint32_t, uint32_t) noexcept;

    // --- wp_cursor_shape_manager_v1 ---
    static void bindCursorShapeManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void cursorShapeManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void cursorShapeManagerGetPointer(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void cursorShapeManagerGetTabletToolV2(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void cursorShapeDeviceDestroy(wl_client*, wl_resource*) noexcept;
    static void cursorShapeDeviceSetShape(wl_client*, wl_resource*, uint32_t, uint32_t) noexcept;

    // --- wp_fractional_scale_manager_v1 ---
    static void bindFractionalScaleManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void fractionalScaleManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void fractionalScaleManagerGetFractionalScale(wl_client*, wl_resource*,
                                                          uint32_t, wl_resource*) noexcept;
    static void fractionalScaleDestroy(wl_client*, wl_resource*) noexcept;

    // --- xdg_wm_dialog_v1 ---
    static void bindXdgWmDialog(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void wmDialogDestroy(wl_client*, wl_resource*) noexcept;
    static void wmDialogGetXdgDialog(wl_client*, wl_resource*, uint32_t, wl_resource*) noexcept;
    static void xdgDialogDestroy(wl_client*, wl_resource*) noexcept;
    static void xdgDialogSetModal(wl_client*, wl_resource*) noexcept;
    static void xdgDialogUnsetModal(wl_client*, wl_resource*) noexcept;

    // --- zwlr_layer_shell_v1 ---
    static void bindLayerShell(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void layerShellGetLayerSurface(wl_client*, wl_resource*, uint32_t,
                                           wl_resource*, wl_resource*, uint32_t, const char*) noexcept;
    static void layerShellDestroy(wl_client*, wl_resource*) noexcept;

    // --- zwlr_layer_surface_v1 ---
    static void layerSurfaceSetSize(wl_client*, wl_resource*, uint32_t, uint32_t) noexcept;
    static void layerSurfaceSetAnchor(wl_client*, wl_resource*, uint32_t) noexcept;
    static void layerSurfaceSetExclusiveZone(wl_client*, wl_resource*, int32_t) noexcept;
    static void layerSurfaceSetMargin(wl_client*, wl_resource*, int32_t, int32_t, int32_t, int32_t) noexcept;
    static void layerSurfaceSetKeyboardInteractivity(wl_client*, wl_resource*, uint32_t) noexcept;
    static void layerSurfaceGetPopup(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void layerSurfaceAckConfigure(wl_client*, wl_resource*, uint32_t) noexcept;
    static void layerSurfaceDestroy(wl_client*, wl_resource*) noexcept;

    // --- wl_subcompositor ---
    static void bindSubcompositor(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void subcompositorDestroy(wl_client*, wl_resource*) noexcept;
    static void subcompositorGetSubsurface(wl_client*, wl_resource*, uint32_t,
                                            wl_resource*, wl_resource*) noexcept;

    // --- wl_subsurface ---
    static void subsurfaceDestroy(wl_client*, wl_resource*) noexcept;
    static void subsurfaceSetPosition(wl_client*, wl_resource*, int32_t, int32_t) noexcept;
    static void subsurfacePlaceAbove(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void subsurfacePlaceBelow(wl_client*, wl_resource*, wl_resource*) noexcept;
    static void subsurfaceSetSync(wl_client*, wl_resource*) noexcept;
    static void subsurfaceSetDesync(wl_client*, wl_resource*) noexcept;

    // --- wp_security_context_manager_v1 ---
    static void bindSecurityContextManager(wl_client*, void*, uint32_t, uint32_t) noexcept;
    static void securityContextManagerDestroy(wl_client*, wl_resource*) noexcept;
    static void securityContextManagerCreateListener(wl_client*, wl_resource*,
                                                      uint32_t, int, int) noexcept;

    // --- wp_security_context_v1 ---
    static void securityContextDestroy(wl_client*, wl_resource*) noexcept;
    static void securityContextSetSandboxEngine(wl_client*, wl_resource*, const char*) noexcept;
    static void securityContextSetAppId(wl_client*, wl_resource*, const char*) noexcept;
    static void securityContextSetInstanceId(wl_client*, wl_resource*, const char*) noexcept;
    static void securityContextCommit(wl_client*, wl_resource*) noexcept;

    // Called from the event-loop callback when a client connects via a security-context listen_fd.
    void onSecurityContextClientConnect(wl_client* client,
                                        const std::string& app_id,
                                        const std::string& sandbox_engine,
                                        const std::string& instance_id) noexcept;

    // --- ext-idle-notify-v1 ---
    static void bindExtIdleNotifier(wl_client*, void*, uint32_t, uint32_t);
    static void extIdleNotifierGetNotification(wl_client*, wl_resource*,
        uint32_t new_id, uint32_t timeout_msec, wl_resource* seat);
    static void extIdleNotifierDestroy(wl_client*, wl_resource*);
    static void extIdleNotificationDestroy(wl_client*, wl_resource*);

    // Raw display accessor for C callbacks that need wl_client_create.
    [[nodiscard]] wl_display* rawDisplay() const noexcept { return display_.get(); }

private:
    WaylandBridge() noexcept = default;
    [[nodiscard]] std::expected<void, BridgeError> init() noexcept;
    [[nodiscard]] std::expected<void, BridgeError> setupIpc() noexcept;
    [[nodiscard]] std::expected<void, BridgeError> setupKeymap() noexcept;

    // Send toplevel+surface configure events using current screen dimensions
    void sendConfigure(wl_resource* surface_resource, int32_t w, int32_t h) noexcept;

    // Build a wl_data_offer from current_selection_source_ and send it to device_res
    void sendSelectionTo(wl_resource* device_res) noexcept;

    // Translate an InputMessage from the xserver into Wayland pointer/keyboard events
    void handleInputMessage(const shared::InputMessage& msg) noexcept;

    // Signal xserver to hide or show the hardware cursor
    void signalCursorLock(bool locked) noexcept;

    // Send a surface's pending buffer to the xserver and release it to the client.
    // Used by both surfaceCommit and the sync-child drain in parent commit.
    static void applyCommit(SurfaceState* state, float sub_x = 0.0f, float sub_y = 0.0f) noexcept;

    UniqueWlDisplay display_{nullptr};
    wl_event_loop* event_loop_{nullptr};
    std::string socket_name_;

    wl_global* compositor_global_{nullptr};
    wl_global* subcompositor_global_{nullptr};

    // zwp_linux_dmabuf_v1 v4 feedback — format table shm fd + DRM render dev_t
    int      dmabuf_feedback_table_fd_{-1};
    uint32_t dmabuf_feedback_table_size_{0};
    uint64_t dmabuf_main_device_{0}; // dev_t of the DRM render node
    wl_global* idle_inhibit_manager_global_{nullptr};
    uint32_t   idle_inhibitor_count_{0}; // >0 means idle is suppressed
    wl_global* wl_drm_global_{nullptr};
    wl_global* single_pixel_buffer_manager_global_{nullptr};
    wl_global* text_input_manager_global_{nullptr};

    // All zwp_text_input_v3 resources (one per client seat binding)
    std::vector<wl_resource*> text_input_resources_;
    wl_global* layer_shell_global_{nullptr};
    wl_global* xdg_dialog_global_{nullptr};
    wl_global* fractional_scale_manager_global_{nullptr};
    wl_global* cursor_shape_manager_global_{nullptr};
    wl_global* xdg_shell_global_{nullptr};
    wl_global* dmabuf_global_{nullptr};
    wl_global* seat_global_{nullptr};
    wl_global* xdg_output_manager_global_{nullptr};
    wl_global* relative_pointer_manager_global_{nullptr};
    wl_global* pointer_constraints_global_{nullptr};
    wl_global* screencopy_manager_global_{nullptr};
    wl_global* decoration_manager_global_{nullptr};
    wl_global* presentation_global_{nullptr};
    wl_global* data_device_manager_global_{nullptr};
    wl_global* viewporter_global_{nullptr};
    wl_global* activation_global_{nullptr};
    uint32_t   next_activation_token_{1};
    wl_global* security_context_manager_global_{nullptr};

    // Clients that connected via a wp_security_context listen socket:
    // wl_client* → {app_id, sandbox_engine, instance_id}
    struct ClientSecCtx { std::string app_id, sandbox_engine, instance_id; };
    std::unordered_map<wl_client*, ClientSecCtx> client_security_contexts_;

    // Clipboard state
    wl_resource*              current_selection_source_{nullptr};
    std::vector<wl_resource*> data_device_resources_;

    // surface_id → pending wp_presentation_feedback resources (drained on surfaceCommit)
    std::unordered_map<uint32_t, std::vector<wl_resource*>> pending_feedbacks_;

    // wl_callback resources from wl_surface.frame() — fired on next DRM flip
    std::vector<wl_resource*> pending_frame_callbacks_;

    // xdg_popup resources that hold an implicit pointer grab (in grab order, outermost first)
    std::vector<wl_resource*> active_popup_grabs_;

    // One wl_global per physical output.
    // Parallel vectors: output_globals_[i] owns the global whose user data is output_global_data_[i].
    std::vector<wl_global*>                         output_globals_;
    std::vector<std::unique_ptr<OutputGlobalData>>  output_global_data_;
    std::vector<std::vector<wl_resource*>>          per_output_resources_;     // wl_output resources per output
    std::vector<std::vector<wl_resource*>>          per_output_xdg_resources_; // zxdg_output_v1 resources per output
    std::vector<shared::DisplayOutputInfo>          output_infos_;             // current layout

    std::unique_ptr<xserver::IpcSocket> xserver_client_;
    bool ipc_connected_{false};
    uint32_t next_surface_id_{100};
    uint32_t configure_serial_{1};   // must start at 1; serial 0 rejected by Qt/GTK
    uint32_t screen_width_{1920};
    uint32_t screen_height_{1080};

    // surface_id → wl_surface resource; used when SurfaceReady arrives from compositor
    std::unordered_map<uint32_t, wl_resource*> surface_resources_;

    // All currently bound wl_pointer resources (for sending motion/button events)
    std::vector<wl_resource*> pointer_resources_;
    // All currently bound zwp_relative_pointer_v1 resources
    std::vector<wl_resource*> relative_pointer_resources_;
    // Active pointer lock resource (at most one at a time in practice)
    wl_resource* locked_pointer_resource_{nullptr};
    bool         pointer_locked_{false};
    // Cursor position tracked inside the bridge (updated from MouseMove InputMessages)
    float cursor_x_{960.0f};
    float cursor_y_{540.0f};
    // Monotonic serial used for wl_pointer events
    uint32_t pointer_serial_{1000};

    // All currently bound wl_keyboard resources
    std::vector<wl_resource*> keyboard_resources_;
    // Surface that currently holds keyboard focus
    wl_resource* keyboard_focus_surface_{nullptr};
    // Monotonic serial used for wl_keyboard events
    uint32_t keyboard_serial_{2000};
    // XKB keymap fd (written to shm; shared read-only with clients)
    int keymap_fd_{-1};
    size_t keymap_size_{0};
    // Cached XKB modifier indices (populated in setupKeymap)
    uint32_t xkb_mod_shift_{0};
    uint32_t xkb_mod_ctrl_{2};
    uint32_t xkb_mod_alt_{3};
    uint32_t xkb_mod_logo_{6};

    // Pending screencopy copy() operation waiting for xserver frame response
    struct PendingScreencopy {
        wl_resource* frame_resource{nullptr};
        int          client_shm_fd{-1};
        int32_t      client_shm_offset{0};
        uint32_t     copy_stride{0};
        uint32_t     copy_height{0};
    };
    std::optional<PendingScreencopy> pending_screencopy_{};

    // ext-idle-notify-v1
    struct IdleNotification {
        wl_resource* resource;
        uint32_t     timeout_msec;
        bool         idled;      // true = idled event already sent
    };
    std::vector<IdleNotification> idle_notifications_;
    wl_global* ext_idle_notifier_global_{nullptr};
    uint32_t   last_activity_ms_{0}; // milliseconds since compositor start

    // Called by input dispatch on any user input
    void notifyActivity() noexcept;
    // Called by poll loop to check idle timeouts
    void pollIdleNotifications(uint32_t now_ms) noexcept;

};

// ---- Per-resource state structs ----

// User data stored in each wl_output wl_global — carries bridge pointer and output index.
struct OutputGlobalData {
    WaylandBridge* bridge{nullptr};
    uint32_t       output_idx{0};
};

// User data for each zxdg_output_v1 resource.
struct XdgOutputData {
    WaylandBridge* bridge{nullptr};
    uint32_t       output_idx{0};
};

// User data for each zwp_relative_pointer_v1 resource.
struct RelativePointerData {
    WaylandBridge* bridge{nullptr};
};

// User data for each zwp_locked_pointer_v1 resource.
struct LockedPointerData {
    WaylandBridge* bridge{nullptr};
    wl_resource*   surface{nullptr};
};

// User data for each zwp_confined_pointer_v1 resource.
struct ConfinedPointerData {
    WaylandBridge* bridge{nullptr};
    wl_resource*   surface{nullptr};
};

// User data for each wl_data_source resource.
struct DataSourceState {
    WaylandBridge*           bridge{nullptr};
    std::vector<std::string> mime_types;
};

// User data for each wl_data_offer resource.
struct DataOfferState {
    wl_resource* source_resource{nullptr}; // the wl_data_source this offer wraps
};

// User data for each wp_presentation_feedback resource.
struct PresentationFeedbackData {
    WaylandBridge* bridge{nullptr};
    uint32_t       surface_id{0};
};

// User data for each zwlr_screencopy_frame_v1 resource.
struct ScreencopyFrameData {
    WaylandBridge* bridge{nullptr};
    uint32_t       output_idx{0};
    uint32_t       width{0};
    uint32_t       height{0};
    uint32_t       stride{0};
};

// User data for each xdg_positioner resource.
struct PositionerState {
    int32_t  width{0}, height{0};
    int32_t  anchor_x{0}, anchor_y{0}, anchor_w{0}, anchor_h{0};
    uint32_t anchor{0};                // XDG_POSITIONER_ANCHOR_*
    uint32_t gravity{0};               // XDG_POSITIONER_GRAVITY_*
    uint32_t constraint_adjustment{0};
    int32_t  offset_x{0}, offset_y{0};
};

// User data for each xdg_popup resource.
struct PopupState {
    WaylandBridge* bridge{nullptr};
    SurfaceState*  surface_state{nullptr};  // surface that owns this popup
    SurfaceState*  parent_state{nullptr};   // parent xdg_surface's SurfaceState
    int32_t        x{0}, y{0};             // position relative to parent
    bool           has_grab{false};
};

struct PoolState {
    int fd{-1};
    int32_t size{0};
};

struct BufferState {
    int fd{-1};
    int32_t width{0};
    int32_t height{0};
    int32_t stride{0};
    int32_t offset{0};
    uint32_t format{0};
    // wp_single_pixel_buffer_v1: true when this is a solid-color buffer (no fd)
    bool solid_color{false};
    uint32_t solid_argb{0}; // packed ARGB 0xAARRGGBB
};

struct SurfaceState {
    uint32_t id{0};
    BufferState* pending_buffer{nullptr};
    wl_resource* attached_buffer_resource{nullptr}; // held for wl_buffer.release
    wl_resource* xdg_surface{nullptr};
    wl_resource* xdg_toplevel{nullptr};
    wl_resource* xdg_popup{nullptr};
    wl_resource* layer_surface{nullptr};            // zwlr_layer_surface_v1 (mutually exclusive with xdg roles)
    wl_resource* dialog_res{nullptr};               // xdg_dialog_v1 attached to this toplevel (if any)
    wl_resource* fractional_scale_res{nullptr};     // wp_fractional_scale_v1 for this surface (if any)
    bool configured{false};
    WaylandBridge* bridge{nullptr};
    // wp_viewport destination size (-1 = not set, use buffer dimensions)
    int32_t viewport_dst_w{-1};
    int32_t viewport_dst_h{-1};
    // wl_subcompositor: non-null when this surface has the subsurface role
    wl_resource* subsurface_resource{nullptr};
    // Ordered list of wl_subsurface child resources (z-order: first = bottommost)
    std::vector<wl_resource*> subsurface_children{};
    // wl_callback resources from wl_surface.frame() — moved to bridge queue on commit
    std::vector<wl_resource*> frame_callbacks{};
    // xdg_toplevel title (set via set_title)
    std::string title{};
    // xdg_surface.set_window_geometry — double-buffered, applied on wl_surface.commit.
    // geom_w == 0 means not set; buffer covers the full surface area.
    int32_t geom_x{0}, geom_y{0}, geom_w{0}, geom_h{0};
};

// User data for each wp_security_context_v1 resource.
struct SecurityContextState {
    WaylandBridge* bridge{nullptr};
    int            listen_fd{-1};
    int            close_fd{-1};
    wl_event_source* listen_source{nullptr};
    wl_event_source* close_source{nullptr};
    std::string    sandbox_engine;
    std::string    app_id;
    std::string    instance_id;
    bool           committed{false};
};

// Heap-allocated destroy listener so we can remove clients from the security-context map.
struct ClientSecCtxDestroyListener {
    wl_listener    listener;
    WaylandBridge* bridge{nullptr};
    wl_client*     client{nullptr};
};

// User data for each zwp_text_input_v3 resource.
struct TextInputState {
    WaylandBridge* bridge{nullptr};
    wl_resource*   focused_surface{nullptr}; // surface that sent enter (null when no focus)
    uint32_t       serial{0};                // counts commit() calls — used as done() serial
    bool           pending_enabled{false};   // double-buffered: set by enable/disable, applied on commit
    bool           enabled{false};           // current committed state
};

// User data for each wl_subsurface resource.
struct SubsurfaceState {
    WaylandBridge* bridge{nullptr};
    wl_resource*   surface_resource{nullptr}; // the wl_surface that owns the subsurface role
    wl_resource*   parent_resource{nullptr};  // parent wl_surface
    int32_t        pending_x{0};   // position pending until next parent (sync) or self (desync) commit
    int32_t        pending_y{0};
    int32_t        cur_x{0};       // currently committed position
    int32_t        cur_y{0};
    bool           sync{true};              // default: synchronized with parent
    bool           has_pending_commit{false}; // sync mode: commit cached, waiting for parent
};

// User data for each zwlr_layer_surface_v1 resource.
struct LayerSurfaceState {
    WaylandBridge* bridge{nullptr};
    SurfaceState*  surface_state{nullptr};
    uint32_t       layer{0};           // 0=background, 1=bottom, 2=top, 3=overlay
    uint32_t       anchor{0};          // bitmask: top(1)|bottom(2)|left(4)|right(8)
    int32_t        exclusive_zone{0};
    int32_t        margin_top{0}, margin_right{0}, margin_bottom{0}, margin_left{0};
    uint32_t       pending_w{0}, pending_h{0}; // 0 = compositor decides
    uint32_t       cfg_w{0}, cfg_h{0};         // size sent in configure event
    uint32_t       configure_serial{0};         // 0 until first configure sent
    bool           acked{false};                // true after ack_configure received
    bool           registered{false};           // true after LayerSurfaceCreated IPC sent
};

// User data for each wp_cursor_shape_device_v1 resource.
struct CursorShapeDeviceState {
    WaylandBridge* bridge{nullptr};
};

// User data for each wp_fractional_scale_v1 resource.
struct FractionalScaleState {
    WaylandBridge* bridge{nullptr};
    SurfaceState*  surface_state{nullptr};
};

// User data for each xdg_dialog_v1 resource.
struct DialogState {
    WaylandBridge* bridge{nullptr};
    SurfaceState*  surface_state{nullptr}; // the xdg_toplevel's SurfaceState
    bool           is_modal{false};
};

} // namespace fbx::xwayland
