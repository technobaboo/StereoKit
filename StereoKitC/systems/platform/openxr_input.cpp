#include "openxr.h"
#include "openxr_input.h"
#include "../hand/hand_oxr_controller.h"
#include "../input.h"

#include "platform_utils.h"
#include "../../libraries/array.h"
#include "../../stereokit.h"
#include "../../_stereokit.h"

#include <openxr/openxr.h>
#include <stdio.h>

namespace sk {

///////////////////////////////////////////

XrActionSet xrc_action_set;
XrAction    xrc_gaze_action;
XrAction    xrc_pose_action;
XrAction    xrc_point_action;
XrAction    xrc_select_action;
XrAction    xrc_grip_action;
XrPath      xrc_hand_subaction_path[2];
XrSpace     xrc_point_space[2];
XrSpace     xr_hand_space  [2] = {};
XrSpace     xr_gaze_space = {};
XrPath      xrc_pose_path  [2];

int32_t xr_gaze_pointer;

struct xrc_profile_info_t {
	const char *name;
	XrPath profile;
	quat   offset_rot[2];
	vec3   offset_pos[2];
};
array_t<xrc_profile_info_t> xrc_profile_offsets = {};
XrPath                      xrc_active_profile[2] = { 0xFFFFFFFF, 0xFFFFFFFF };

void oxri_set_profile(handed_ hand, XrPath profile);

///////////////////////////////////////////

bool oxri_init() {
	XrPath                               profile_path;
	XrActionCreateInfo                   action_info     = { XR_TYPE_ACTION_CREATE_INFO };
	XrInteractionProfileSuggestedBinding suggested_binds = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };

	xrc_offset_pos[0] = vec3_zero;
	xrc_offset_pos[1] = vec3_zero;
	xrc_offset_rot[0] = quat_identity;
	xrc_offset_rot[1] = quat_identity;

	xr_gaze_pointer = input_add_pointer(input_source_gaze | (xr_has_gaze ? input_source_gaze_eyes : input_source_gaze_head));

	XrActionSetCreateInfo actionset_info = { XR_TYPE_ACTION_SET_CREATE_INFO };
	snprintf(actionset_info.actionSetName,          sizeof(actionset_info.actionSetName),          "input");
	snprintf(actionset_info.localizedActionSetName, sizeof(actionset_info.localizedActionSetName), "Input");
	XrResult result = xrCreateActionSet(xr_instance, &actionset_info, &xrc_action_set);
	if (XR_FAILED(result)) {
		log_infof("xrCreateActionSet failed: [%s]", openxr_string(result));
		return false;
	}

	xrStringToPath(xr_instance, "/user/hand/left",  &xrc_hand_subaction_path[0]);
	xrStringToPath(xr_instance, "/user/hand/right", &xrc_hand_subaction_path[1]);

	// Create an action to track the position and orientation of the hands! This is
	// the controller location, or the center of the palms for actual hands.
	action_info.countSubactionPaths = _countof(xrc_hand_subaction_path);
	action_info.subactionPaths      = xrc_hand_subaction_path;
	action_info.actionType          = XR_ACTION_TYPE_POSE_INPUT;
	snprintf(action_info.actionName,          sizeof(action_info.actionName),          "hand_pose");
	snprintf(action_info.localizedActionName, sizeof(action_info.localizedActionName), "Hand Pose");
	result = xrCreateAction(xrc_action_set, &action_info, &xrc_pose_action);
	if (XR_FAILED(result)) {
		log_infof("xrCreateAction failed: [%s]", openxr_string(result));
		return false;
	}

	// Create an action to track the pointing position and orientation!
	action_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
	snprintf(action_info.actionName,          sizeof(action_info.actionName),          "hand_point");
	snprintf(action_info.localizedActionName, sizeof(action_info.localizedActionName), "Hand Point");
	result = xrCreateAction(xrc_action_set, &action_info, &xrc_point_action);
	if (XR_FAILED(result)) {
		log_infof("xrCreateAction failed: [%s]", openxr_string(result));
		return false;
	}

	// Create an action for listening to the select action! This is primary trigger
	// on controllers, and an airtap on HoloLens
	action_info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
	snprintf(action_info.actionName,          sizeof(action_info.actionName),          "select");
	snprintf(action_info.localizedActionName, sizeof(action_info.localizedActionName), "Select");
	result = xrCreateAction(xrc_action_set, &action_info, &xrc_select_action);
	if (XR_FAILED(result)) {
		log_infof("xrCreateAction failed: [%s]", openxr_string(result));
		return false;
	}

	action_info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
	snprintf(action_info.actionName,          sizeof(action_info.actionName),          "grip");
	snprintf(action_info.localizedActionName, sizeof(action_info.localizedActionName), "Grip");
	result = xrCreateAction(xrc_action_set, &action_info, &xrc_grip_action);
	if (XR_FAILED(result)) {
		log_infof("xrCreateAction failed: [%s]", openxr_string(result));
		return false;
	}

	if (xr_has_gaze) {
		action_info = { XR_TYPE_ACTION_CREATE_INFO };
		action_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
		snprintf(action_info.actionName,          sizeof(action_info.actionName),          "eye_gaze");
		snprintf(action_info.localizedActionName, sizeof(action_info.localizedActionName), "Eye Gaze");
		result = xrCreateAction(xrc_action_set, &action_info, &xrc_gaze_action);
		if (XR_FAILED(result)) {
			log_warnf("xrCreateAction failed: [%s]", openxr_string(result));
			return false;
		}

		XrPath gaze_path;
		xrStringToPath(xr_instance, "/user/eyes_ext/input/gaze_ext/pose", &gaze_path);
		XrActionSuggestedBinding bindings = {xrc_gaze_action, gaze_path};

		xrStringToPath(xr_instance, "/interaction_profiles/ext/eye_gaze_interaction", &profile_path);
		suggested_binds.interactionProfile     = profile_path;
		suggested_binds.suggestedBindings      = &bindings;
		suggested_binds.countSuggestedBindings = 1;
		result = xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds);
		if (XR_FAILED(result)) {
			log_warnf("Gaze xrSuggestInteractionProfileBindings failed: [%s]", openxr_string(result));
		}

		XrActionSpaceCreateInfo create_space = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
		create_space.action            = xrc_gaze_action;
		create_space.poseInActionSpace = { {0,0,0,1}, {0,0,0} };
		result = xrCreateActionSpace(xr_session, &create_space, &xr_gaze_space);
		if (XR_FAILED(result)) {
			log_warnf("Gaze xrCreateActionSpace failed: [%s]", openxr_string(result));
		}
	}

	// Bind the actions we just created to specific locations on the Khronos simple_controller
	// definition! These are labeled as 'suggested' because they may be overridden by the runtime
	// preferences. For example, if the runtime allows you to remap buttons, or provides input
	// accessibility settings.
	XrPath point_path [2];
	XrPath select_path[2];
	XrPath grip_path  [2];
	xrStringToPath(xr_instance, "/user/hand/left/input/grip/pose",  &xrc_pose_path[0]);
	xrStringToPath(xr_instance, "/user/hand/right/input/grip/pose", &xrc_pose_path[1]);
	xrStringToPath(xr_instance, "/user/hand/left/input/aim/pose",   &point_path[0]);
	xrStringToPath(xr_instance, "/user/hand/right/input/aim/pose",  &point_path[1]);

	// microsoft / motion_controller
	{
		xrStringToPath(xr_instance, "/user/hand/left/input/trigger/value",  &select_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/trigger/value", &select_path[1]);
		xrStringToPath(xr_instance, "/user/hand/left/input/squeeze/value",  &grip_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/squeeze/value", &grip_path[1]);
		XrActionSuggestedBinding bindings[] = {
			{ xrc_pose_action,   xrc_pose_path[0] }, { xrc_pose_action,   xrc_pose_path[1] },
			{ xrc_point_action,  point_path   [0] }, { xrc_point_action,  point_path   [1] },
			{ xrc_select_action, select_path  [0] }, { xrc_select_action, select_path  [1] },
			{ xrc_grip_action,   grip_path    [0] }, { xrc_grip_action,   grip_path    [1] },
		};

		xrStringToPath(xr_instance, "/interaction_profiles/microsoft/motion_controller", &profile_path);
		suggested_binds.interactionProfile     = profile_path;
		suggested_binds.suggestedBindings      = &bindings[0];
		suggested_binds.countSuggestedBindings = _countof(bindings);
		if (XR_SUCCEEDED(xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds))) {
			// Orientation fix for WMR vs. HoloLens controllers
			xrc_profile_info_t info;
			info.profile = profile_path;
			info.name    = "microsoft/motion_controller";
			if (sk_info.display_type == display_opaque) {
				info.offset_rot[handed_left ] = quat_from_angles(-45, 0, 0);
				info.offset_rot[handed_right] = quat_from_angles(-45, 0, 0);
				info.offset_pos[handed_left ] = { 0.01f, -0.01f, 0.015f };
				info.offset_pos[handed_right] = { 0.01f, -0.01f, 0.015f };
			} else {
				info.offset_rot[handed_left ] = quat_from_angles(-68, 0, 0);
				info.offset_rot[handed_right] = quat_from_angles(-68, 0, 0);
				info.offset_pos[handed_left ] = { 0, 0.005f, 0 };
				info.offset_pos[handed_right] = { 0, 0.005f, 0 };
			}
			xrc_profile_offsets.add(info);
		}
	}

	// htc / vive_controller
	{
		xrStringToPath(xr_instance, "/user/hand/left/input/trigger/value",  &select_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/trigger/value", &select_path[1]);
		xrStringToPath(xr_instance, "/user/hand/left/input/squeeze/value",  &grip_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/squeeze/value", &grip_path[1]);
		XrActionSuggestedBinding bindings[] = {
			{ xrc_pose_action,   xrc_pose_path[0] }, { xrc_pose_action,   xrc_pose_path[1] },
			{ xrc_point_action,  point_path   [0] }, { xrc_point_action,  point_path   [1] },
			{ xrc_select_action, select_path  [0] }, { xrc_select_action, select_path  [1] },
			{ xrc_grip_action,   grip_path    [0] }, { xrc_grip_action,   grip_path    [1] },
		};

		xrStringToPath(xr_instance, "/interaction_profiles/htc/vive_controller", &profile_path);
		suggested_binds.interactionProfile     = profile_path;
		suggested_binds.suggestedBindings      = &bindings[0];
		suggested_binds.countSuggestedBindings = _countof(bindings);
		if (XR_SUCCEEDED(xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds))) {
			// Orientation fix for HTC Vive controllers
			xrc_profile_info_t info;
			info.profile = profile_path;
			info.name    = "htc/vive_controller";
			info.offset_rot[handed_left ] = quat_from_angles(-40, 0, 0);
			info.offset_rot[handed_right] = quat_from_angles(-40, 0, 0);
			info.offset_pos[handed_left ] = {-0.035f, -0.00f, 0.00f };
			info.offset_pos[handed_right] = {0.035f, -0.00f, 0.00f };
			xrc_profile_offsets.add(info);
		}
	}

	// valve / index_controller
	{
		xrStringToPath(xr_instance, "/user/hand/left/input/trigger/value",  &select_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/trigger/value", &select_path[1]);
		xrStringToPath(xr_instance, "/user/hand/left/input/squeeze/value",  &grip_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/squeeze/value", &grip_path[1]);
		XrActionSuggestedBinding bindings[] = {
			{ xrc_pose_action,   xrc_pose_path[0] }, { xrc_pose_action,   xrc_pose_path[1] },
			{ xrc_point_action,  point_path   [0] }, { xrc_point_action,  point_path   [1] },
			{ xrc_select_action, select_path  [0] }, { xrc_select_action, select_path  [1] },
			{ xrc_grip_action,   grip_path    [0] }, { xrc_grip_action,   grip_path    [1] },
		};

		xrStringToPath(xr_instance, "/interaction_profiles/valve/index_controller", &profile_path);
		suggested_binds.interactionProfile     = profile_path;
		suggested_binds.suggestedBindings      = &bindings[0];
		suggested_binds.countSuggestedBindings = _countof(bindings);
		if (XR_SUCCEEDED(xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds))) {
			// Orientation fix for Valve Index controllers
			xrc_profile_info_t info;
			info.profile = profile_path;
			info.name    = "valve/index_controller";
			info.offset_rot[handed_left ] = quat_from_angles(-40, 0, 0);
			info.offset_rot[handed_right] = quat_from_angles(-40, 0, 0);
			info.offset_pos[handed_left ] = {-0.035f, -0.00f, 0.00f };
			info.offset_pos[handed_right] = { 0.035f, -0.00f, 0.00f };
			xrc_profile_offsets.add(info);
		}
	}

	// oculus / touch_controller
	{
		xrStringToPath(xr_instance, "/user/hand/left/input/trigger/value",  &select_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/trigger/value", &select_path[1]);
		xrStringToPath(xr_instance, "/user/hand/left/input/squeeze/value",  &grip_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/squeeze/value", &grip_path[1]);
		XrActionSuggestedBinding bindings[] = {
			{ xrc_pose_action,   xrc_pose_path[0] }, { xrc_pose_action,   xrc_pose_path[1] },
			{ xrc_point_action,  point_path   [0] }, { xrc_point_action,  point_path   [1] },
			{ xrc_select_action, select_path  [0] }, { xrc_select_action, select_path  [1] },
			{ xrc_grip_action,   grip_path    [0] }, { xrc_grip_action,   grip_path    [1] },
		};

		xrStringToPath(xr_instance, "/interaction_profiles/oculus/touch_controller", &profile_path);
		suggested_binds.interactionProfile     = profile_path;
		suggested_binds.suggestedBindings      = &bindings[0];
		suggested_binds.countSuggestedBindings = _countof(bindings);
		if (XR_SUCCEEDED(xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds))) {
			// Orientation fix for oculus touch controllers
			xrc_profile_info_t info;
			info.profile = profile_path;
			info.name    = "oculus/touch_controller";
			info.offset_rot[handed_left ] = quat_from_angles(-80, 0, 0);
			info.offset_rot[handed_right] = quat_from_angles(-80, 0, 0);
			info.offset_pos[handed_left ] = {-0.03f, 0.01f, 0.00f };
			info.offset_pos[handed_right] = { 0.03f, 0.01f, 0.00f };
			xrc_profile_offsets.add(info);
		}
	}

	// khr / simple_controller
#if !defined(SK_OS_ANDROID)
	{
		xrStringToPath(xr_instance, "/user/hand/left/input/select/click",  &select_path[0]);
		xrStringToPath(xr_instance, "/user/hand/right/input/select/click", &select_path[1]);
		XrActionSuggestedBinding bindings[] = {
			{ xrc_pose_action,   xrc_pose_path[0] }, { xrc_pose_action,   xrc_pose_path[1] },
			{ xrc_point_action,  point_path   [0] }, { xrc_point_action,  point_path   [1] },
			{ xrc_select_action, select_path  [0] }, { xrc_select_action, select_path  [1] },
		};

		xrStringToPath(xr_instance, "/interaction_profiles/khr/simple_controller", &profile_path);
		suggested_binds.interactionProfile     = profile_path;
		suggested_binds.suggestedBindings      = &bindings[0];
		suggested_binds.countSuggestedBindings = _countof(bindings);
		if (XR_SUCCEEDED(xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds))) {
			xrc_profile_info_t info;
			info.profile = profile_path;
			info.name    = "khr/simple_controller";
			info.offset_rot[handed_left ] = quat_identity;
			info.offset_rot[handed_right] = quat_identity;;
			info.offset_pos[handed_left ] = vec3_zero;
			info.offset_pos[handed_right] = vec3_zero;
			xrc_profile_offsets.add(info);
		}
	}
#endif

	// Create frames of reference for the pose actions
	for (int32_t i = 0; i < 2; i++) {
		XrActionSpaceCreateInfo action_space_info = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		action_space_info.poseInActionSpace = { {0,0,0,1}, {0,0,0} };
		action_space_info.subactionPath     = xrc_hand_subaction_path[i];
		action_space_info.action            = xrc_pose_action;
		result = xrCreateActionSpace(xr_session, &action_space_info, &xr_hand_space[i]);
		if (XR_FAILED(result)) {
			log_infof("xrCreateActionSpace failed: [%s]", openxr_string(result));
			return false;
		}

		action_space_info = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		action_space_info.poseInActionSpace = { {0,0,0,1}, {0,0,0} };
		action_space_info.subactionPath     = xrc_hand_subaction_path[i];
		action_space_info.action            = xrc_point_action;
		result = xrCreateActionSpace(xr_session, &action_space_info, &xrc_point_space[i]);
		if (XR_FAILED(result)) {
			log_infof("xrCreateActionSpace failed: [%s]", openxr_string(result));
			return false;
		}
	}

	// Attach the action set we just made to the session
	XrSessionActionSetsAttachInfo attach_info = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attach_info.countActionSets = 1;
	attach_info.actionSets      = &xrc_action_set;
	result = xrAttachSessionActionSets(xr_session, &attach_info);
	if (XR_FAILED(result)) {
		log_infof("xrAttachSessionActionSets failed: [%s]", openxr_string(result));
		return false;
	}

	oxri_set_profile(handed_left,  xrc_active_profile[handed_left ]);
	oxri_set_profile(handed_right, xrc_active_profile[handed_right]);
	return true;
}

///////////////////////////////////////////

void oxri_shutdown() {
	xrc_profile_offsets.free();
	if (xr_hand_space[0]) { xrDestroySpace    (xr_hand_space[0]); xr_hand_space[0] = {}; }
	if (xr_hand_space[1]) { xrDestroySpace    (xr_hand_space[1]); xr_hand_space[1] = {}; }
	if (xrc_action_set  ) { xrDestroyActionSet(xrc_action_set  ); xrc_action_set   = {}; }
}

///////////////////////////////////////////

void oxri_update_frame() {
	// Update our action set with up-to-date input data!
	XrActiveActionSet action_set = { };
	action_set.actionSet     = xrc_action_set;
	action_set.subactionPath = XR_NULL_PATH;
	XrActionsSyncInfo sync_info = { XR_TYPE_ACTIONS_SYNC_INFO };
	sync_info.countActiveActionSets = 1;
	sync_info.activeActionSets      = &action_set;
	xrSyncActions(xr_session, &sync_info);

	pointer_t* pointer = input_get_pointer(xr_gaze_pointer);
	// Gaze input
	if (xr_has_gaze) {
		XrActionStatePose    action_pose = {XR_TYPE_ACTION_STATE_POSE};
		XrActionStateGetInfo action_info = {XR_TYPE_ACTION_STATE_GET_INFO};
		action_info.action = xrc_gaze_action;
		xrGetActionStatePose(xr_session, &action_info, &action_pose);

		input_gaze_track_state = button_make_state(input_gaze_track_state & button_state_active, action_pose.isActive);
		pointer->tracked = input_gaze_track_state;

		if (action_pose.isActive && openxr_get_space(xr_gaze_space, &input_gaze_pose)) {
			pointer->ray.pos     = input_gaze_pose.position;
			pointer->ray.dir     = input_gaze_pose.orientation * vec3_forward;
			pointer->orientation = input_gaze_pose.orientation;
		}
	} else {
		input_gaze_track_state = button_make_state(input_gaze_track_state & button_state_active, true);
		memcpy(&input_gaze_pose, input_head(), sizeof(pose_t));
		pointer->ray.pos     = input_gaze_pose.position;
		pointer->ray.dir     = input_gaze_pose.orientation * vec3_forward;
		pointer->orientation = input_gaze_pose.orientation;
	}
}

///////////////////////////////////////////

void oxri_set_profile(handed_ hand, XrPath profile) {
	xrc_active_profile[hand] = profile;
	for (int32_t i = 0; i < xrc_profile_offsets.count; i++) {
		if (xrc_profile_offsets[i].profile == profile) {
			xrc_offset_pos[hand] = xrc_profile_offsets[i].offset_pos[hand];
			xrc_offset_rot[hand] = xrc_profile_offsets[i].offset_rot[hand];
			log_diagf("Switched %s controller profile to %s", hand == handed_left ? "left" : "right", xrc_profile_offsets[i].name);
			break;
		}
	}
}

///////////////////////////////////////////

void oxri_update_interaction_profile() {
	XrPath path[2];
	xrStringToPath(xr_instance, "/user/hand/left",  &path[handed_left]);
	xrStringToPath(xr_instance, "/user/hand/right", &path[handed_right]);

	XrInteractionProfileState active_profile = { XR_TYPE_INTERACTION_PROFILE_STATE };
	for (int32_t h = 0; h < handed_max; h++) {
		if (XR_FAILED(xrGetCurrentInteractionProfile(xr_session, path[h], &active_profile)))
			continue;
		if (active_profile.interactionProfile != xrc_active_profile[h])
			oxri_set_profile((handed_)h, active_profile.interactionProfile);
	}
}

} // namespace sk
