#include "../stereokit.h"
#include "input_hand.h"
#include "input_hand_poses.h"

#include "../asset_types/assets.h"
#include "../asset_types/material.h"

namespace sk {

///////////////////////////////////////////

#define SK_FINGERS 5
#define SK_FINGERJOINTS 5
#define SK_SQRT2 1.41421356237f
#define SK_FINGER_SOLIDS 1

struct hand_mesh_t {
	mesh_t  mesh;
	vert_t *verts;
	int     vert_count;
	vind_t *inds;
	int     ind_count;
};

struct hand_state_t {
	hand_t      info;
	pose_t      pose_blend[5][5];
	solid_t     solids[SK_FINGER_SOLIDS];
	material_t  material;
	hand_mesh_t mesh;
	bool        visible;
	bool        enabled;
};

hand_state_t hand_state[2];

const float hand_joint_size [5] = {.01f,.026f,.023f,.02f,.015f}; // in order of hand_joint_. found by measuring the width of my pointer finger when flattened on a ruler
const float hand_finger_size[5] = {1.15f,1,1,.85f,.75f}; // in order of hand_finger_. Found by comparing the distal joint of my index finger, with my other distal joints

///////////////////////////////////////////

const hand_t &input_hand(handed_ hand) {
	return hand_state[hand].info;
}

///////////////////////////////////////////

void modify(pose_t *pose) {
	for (size_t i = 0; i < 25; i++) {
		pose[i].position *= 1.15f;
	}
}

void input_hand_init() {
	modify(&input_pose_fist   [0][0]);
	modify(&input_pose_neutral[0][0]);
	modify(&input_pose_point  [0][0]);
	modify(&input_pose_pinch  [0][0]);

	material_t hand_mat = material_copy_id("default/material");
	material_set_transparency(hand_mat, transparency_blend);

	gradient_t color_grad = gradient_create();
	gradient_add(color_grad, color128{ 1,1,1,0 }, 0);
	gradient_add(color_grad, color128{ 1,1,1,0 }, 0.2f);
	gradient_add(color_grad, color128{ 1,1,1,1 }, 0.9f);

	color32 gradient[16 * 16];
	for (int32_t y = 0; y < 16; y++) {
		color32 col = gradient_get32(color_grad, y/15.f);
	for (int32_t x = 0; x < 16; x++) {
		gradient[x + y * 16] = col;
	} }
	gradient_release(color_grad);

	tex_t gradient_tex = tex_create();
	tex_set_colors (gradient_tex, 16, 16, gradient);
	tex_set_address(gradient_tex, tex_address_clamp);
	material_set_texture(hand_mat, "diffuse", gradient_tex);
	material_set_queue_offset(hand_mat, -10);
	
	// Initialize the hands!
	for (size_t i = 0; i < handed_max; i++) {
		hand_state[i].visible  = true;
		hand_state[i].material = hand_mat;
		assets_addref(hand_state[i].material->header);

		hand_state[i].info.palm.orientation = quat_identity;
		hand_state[i].info.handedness = (handed_)i;
		input_hand_update_mesh((handed_)i);

		hand_state[i].solids[0] = solid_create(vec3_zero, quat_identity, solid_type_unaffected);
		solid_add_box    (hand_state[i].solids[0], vec3{ 0.03f, .1f, .2f });
		solid_set_enabled(hand_state[i].solids[0], false);

		memcpy(hand_state[i].pose_blend, input_pose_neutral, sizeof(pose_t) * SK_FINGERS * SK_FINGERJOINTS);
	}

	tex_release(gradient_tex);
	material_release(hand_mat);
}

///////////////////////////////////////////

void input_hand_shutdown() {
	for (size_t i = 0; i < handed_max; i++) {
		for (size_t f = 0; f < SK_FINGER_SOLIDS; f++) {
			solid_release(hand_state[i].solids[f]);
		}
		material_release(hand_state[i].material);
		mesh_release(hand_state[i].mesh.mesh);
		free(hand_state[i].mesh.inds);
		free(hand_state[i].mesh.verts);
	}
}

///////////////////////////////////////////

void input_hand_update() {
	
	for (size_t i = 0; i < handed_max; i++) {
		// Update hand states
		input_hand_state_update((handed_)i);

		// Update hand meshes
		bool tracked = hand_state[i].info.state & input_state_tracked;
		if (hand_state[i].visible && hand_state[i].material != nullptr && tracked) {
			input_hand_update_mesh((handed_)i);
			render_add_mesh(hand_state[i].mesh.mesh, hand_state[i].material, matrix_identity);
		}

		// Update hand physics
		solid_set_enabled(hand_state[i].solids[0], tracked);
		if (tracked) {
			solid_move(hand_state[i].solids[0], hand_state[i].info.palm.position, hand_state[i].info.palm.orientation);
		}
	}
}

///////////////////////////////////////////

void input_hand_state_update(handed_ handedness) {
	hand_t &hand = hand_state[handedness].info;

	// Update hand state based on inputs
	bool was_trigger = hand.state & input_state_pinch;
	bool was_gripped = hand.state & input_state_grip;
	// Clear all except tracking state
	hand.state &= input_state_tracked | input_state_untracked | input_state_justtracked;
	
	float finger_dist = 2 * cm2m + hand.fingers[hand_finger_index][hand_joint_tip].size + hand.fingers[hand_finger_thumb][hand_joint_tip].size;
	bool is_trigger = vec3_magnitude_sq((hand.fingers[hand_finger_index][hand_joint_tip].position - hand.fingers[hand_finger_thumb][hand_joint_tip].position)) < (finger_dist * finger_dist);
	bool is_grip =
		vec3_magnitude_sq((hand.fingers[hand_finger_index ][hand_joint_tip].position - hand.fingers[hand_finger_index ][hand_joint_metacarpal].position)) < ((4.f * cm2m) * (4.f * cm2m)) &&
		vec3_magnitude_sq((hand.fingers[hand_finger_middle][hand_joint_tip].position - hand.fingers[hand_finger_middle][hand_joint_metacarpal].position)) < ((4.f * cm2m) * (4.f * cm2m));

	if (was_trigger != is_trigger) hand.state |= is_trigger ? input_state_justpinch   : input_state_unpinch;
	if (was_gripped != is_grip)    hand.state |= is_grip    ? input_state_justgrip    : input_state_ungrip;
	if (is_trigger) hand.state |= input_state_pinch;
	if (is_grip)    hand.state |= input_state_grip;
}

///////////////////////////////////////////

hand_joint_t *input_hand_get_pose_buffer(handed_ hand) {
	return &hand_state[hand].info.fingers[0][0];
}

///////////////////////////////////////////

void input_hand_sim(handed_ handedness, const vec3 &hand_pos, const quat &orientation, bool tracked, bool trigger_pressed, bool grip_pressed) {
	hand_t &hand = hand_state[handedness].info;
	hand.palm.position    = hand_pos;
	hand.palm.orientation = orientation;
	
	// Update hand state based on inputs
	bool was_tracked = hand.state & input_state_tracked;
	if (was_tracked != tracked) hand.state |= tracked ? input_state_justtracked : input_state_untracked;
	if (tracked)                hand.state |= input_state_tracked;

	// only sim it if it's tracked
	if (tracked) {
		// Switch pose based on what buttons are pressed
		const pose_t *dest_pose;
		if (trigger_pressed && !grip_pressed)
			dest_pose = &input_pose_pinch[0][0];
		else if (trigger_pressed && grip_pressed)
			dest_pose = &input_pose_fist[0][0];
		else if (!trigger_pressed && grip_pressed)
			dest_pose = &input_pose_point[0][0];
		else
			dest_pose = &input_pose_neutral[0][0];

		// Blend our active pose with our desired pose, for smooth transitions
		// between poses
		float delta = time_elapsedf_unscaled() * 30;
		delta = delta>1?1:delta;
		for (size_t f = 0; f < 5; f++) {
		for (size_t j = 0; j < 5; j++) {
			pose_t *p = &hand_state[handedness].pose_blend[f][j];
			p->position    = vec3_lerp (p->position,    dest_pose[f * 5 + j].position,    delta);
			p->orientation = quat_slerp(p->orientation, dest_pose[f * 5 + j].orientation, delta);
		} }
	
		for (size_t f = 0; f < 5; f++) {
			const pose_t *finger = &hand_state[handedness].pose_blend[f][0];
		for (size_t j = 0; j < 5; j++) {
			vec3 pos = finger[j].position;
			quat rot = finger[j].orientation;
			if (handedness == handed_right) {
				// mirror along x axis, our pose data is for left hand
				pos.x = -pos.x;
				rot.y = -rot.y;
				rot.z = -rot.z;
			}
			hand.fingers[f][j].position    = orientation * pos + hand_pos;
			hand.fingers[f][j].orientation = rot * orientation;
			hand.fingers[f][j].size        = hand_finger_size[f] * hand_joint_size[j] * 0.25f;
		} }
	}
}

///////////////////////////////////////////

void input_hand_update_mesh(handed_ hand) {
	hand_mesh_t &data = hand_state[hand].mesh;

	// if this mesh hasn't been initialized yet
	if (data.verts == nullptr) {
		data.vert_count = 4 * SK_FINGERJOINTS * SK_FINGERS; // verts: per joint, per finger 
		data.ind_count  = (3 * 8 * (SK_FINGERJOINTS-1) + (4 * 3)) * (SK_FINGERS) ; // inds: per face, per connecting faces, per joint section, per finger, plus 2 caps
		data.verts      = (vert_t*)malloc(sizeof(vert_t) * data.vert_count);
		data.inds       = (vind_t*)malloc(sizeof(vind_t) * data.ind_count );

		int32_t ind = 0;
		for (vind_t f = 0; f < SK_FINGERS; f++) {
			vind_t start_vert =  f    * 4 * SK_FINGERJOINTS;
			vind_t end_vert   = (f+1) * 4 * SK_FINGERJOINTS;

			// start cap
			data.inds[ind++] = start_vert+0;
			data.inds[ind++] = start_vert+1;
			data.inds[ind++] = start_vert+2;

			data.inds[ind++] = start_vert+0;
			data.inds[ind++] = start_vert+2;
			data.inds[ind++] = start_vert+3;
		
			// tube faces
			for (vind_t j = 0; j < SK_FINGERJOINTS-1; j++) {
			for (vind_t c = 0; c < 4; c++) {
				vind_t curr1 = start_vert +  j    * 4 + c;
				vind_t next1 = start_vert + (j+1) * 4 + c;
				vind_t curr2 = start_vert +  j    * 4 + (c+1)%4;
				vind_t next2 = start_vert + (j+1) * 4 + (c+1)%4;
				data.inds[ind++] = curr1;
				data.inds[ind++] = next1;
				data.inds[ind++] = next2;

				data.inds[ind++] = curr1;
				data.inds[ind++] = next2;
				data.inds[ind++] = curr2;
			} }

			// end cap
			data.inds[ind++] = end_vert-2;
			data.inds[ind++] = end_vert-3;
			data.inds[ind++] = end_vert-4;

			data.inds[ind++] = end_vert-1;
			data.inds[ind++] = end_vert-2;
			data.inds[ind++] = end_vert-4;
		}

		// Generate uvs and colors for the mesh
		int v = 0;
		for (int f = 0; f < SK_FINGERS;      f++) {
		for (int j = 0; j < SK_FINGERJOINTS; j++) {
			float y = f == 0 ?
				(fmaxf(0,j-1) / (float)(SK_FINGERJOINTS-2)) :
				(j / (float)(SK_FINGERJOINTS-1));
			data.verts[v].uv  = { 0,y };
			data.verts[v].col = { 255,255,255,255 };
			v++;
			data.verts[v].uv  = { .5f,y };
			data.verts[v].col = { 255,255,255,255 };
			v++;
			data.verts[v].uv  = { 1,y };
			data.verts[v].col = { 255,255,255,255 };
			v++;
			data.verts[v].uv  = { .5f,y };
			data.verts[v].col = { 255,255,255,255 };
			v++;
		} }

		data.mesh = mesh_create();
		if (hand == handed_left)
			mesh_set_id(data.mesh, "default/mesh_lefthand");
		else
			mesh_set_id(data.mesh, "default/mesh_righthand");
		mesh_set_inds(data.mesh, data.inds, data.ind_count);
	}

	int v = 0;
	for (int f = 0; f < SK_FINGERS;      f++) {
	for (int j = 0; j < SK_FINGERJOINTS; j++) {
		const hand_joint_t &pose = hand_state[hand].info.fingers[f][j];

		// Make local right and up axis vectors
		vec3  right = pose.orientation * vec3_right;
		vec3  up    = pose.orientation * vec3_up;

		// Find the scale for this joint
		float scale = pose.size;
		if (f == 0 && j < 2) scale *= 0.5f; // thumb is too fat at the bottom

		// Use the local axis to create a ring of verts
		data.verts[v].norm = (up - right) * SK_SQRT2;
		data.verts[v].pos  = pose.position + data.verts[v].norm*scale;
		v++;
		data.verts[v].norm = (right + up) * SK_SQRT2;
		data.verts[v].pos  = pose.position + data.verts[v].norm*scale;
		v++;
		data.verts[v].norm = (right - up) * SK_SQRT2;
		data.verts[v].pos  = pose.position + data.verts[v].norm*scale;
		v++;
		data.verts[v].norm = (-up - right) * SK_SQRT2;
		data.verts[v].pos  = pose.position + data.verts[v].norm*scale;
		v++;
	} }

	// And update the mesh vertices!
	mesh_set_verts(data.mesh, data.verts, data.vert_count);
}

///////////////////////////////////////////

void input_hand_visible(handed_ hand, bool32_t visible) {
	hand_state[hand].visible = visible;
}

///////////////////////////////////////////

void input_hand_solid(handed_ hand, bool32_t solid) {
	for (size_t i = 0; i < SK_FINGER_SOLIDS; i++) {
		solid_set_enabled(hand_state[hand].solids[i], solid);
	}
}

///////////////////////////////////////////

void input_hand_material(handed_ hand, material_t material) {
	material_release(hand_state[hand].material);
	
	if (material != nullptr)
		assets_addref(material->header);

	hand_state[hand].material = material;
}

} // namespace sk