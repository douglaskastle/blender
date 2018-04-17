/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GeometryExporter.h"
#include "AnimationExporter.h"
#include "AnimationClipExporter.h"
#include "BCAnimationSampler.h"
#include "MaterialExporter.h"
#include "collada_utils.h"



std::map<std::string, std::vector<std::string>> BC_CHANNEL_NAME_FROM_TYPE = {
	{ "color"         , { "R", "G", "B" } },
	{ "specular_color", { "R", "G", "B" } },
	{ "diffuse_color",  { "R", "G", "B" } },
	{ "alpha",          { "R", "G", "B" } },
	{ "scale",          { "X", "Y", "Z" } },
	{ "location",       { "X", "Y", "Z" } },
	{ "rotation_euler", { "X", "Y", "Z" } }
};

/*
 * Translation table to map FCurve animation types to Collada animation.
 * Todo: Maybe we can keep the names from the fcurves here instead of
 * mapping. However this is what i found in the old code. So keep
 * this map for now.
 */
std::map<BC_animation_transform_type, std::string> BC_ANIMATION_NAME_FROM_TYPE = {
	{ BC_ANIMATION_TYPE_ROTATION, "rotation" },
	{ BC_ANIMATION_TYPE_ROTATION_EULER, "rotation" },
	{ BC_ANIMATION_TYPE_ROTATION_QUAT, "rotation" },
	{ BC_ANIMATION_TYPE_SCALE, "scale" },
	{ BC_ANIMATION_TYPE_LOCATION, "location" },

	/* Materials */
	{ BC_ANIMATION_TYPE_SPECULAR_COLOR, "specular" },
	{ BC_ANIMATION_TYPE_DIFFUSE_COLOR, "diffuse" },
	{ BC_ANIMATION_TYPE_IOR, "index_of_refraction" },
	{ BC_ANIMATION_TYPE_SPECULAR_HARDNESS, "specular_hardness" },
	{ BC_ANIMATION_TYPE_ALPHA, "alpha" },

	/* Lamps */
	{ BC_ANIMATION_TYPE_LIGHT_COLOR, "color" },
	{ BC_ANIMATION_TYPE_FALL_OFF_ANGLE, "fall_off_angle" },
	{ BC_ANIMATION_TYPE_FALL_OFF_EXPONENT, "fall_off_exponent" },
	{ BC_ANIMATION_TYPE_BLENDER_DIST, "blender/blender_dist" },

	/* Cameras */
	{ BC_ANIMATION_TYPE_LENS, "xfov" },
	{ BC_ANIMATION_TYPE_XFOV, "xfov" },
	{ BC_ANIMATION_TYPE_XMAG, "xmag" },
	{ BC_ANIMATION_TYPE_ZFAR, "zfar" },
	{ BC_ANIMATION_TYPE_ZNEAR, "znear" },

	{ BC_ANIMATION_TYPE_UNKNOWN, "" }
};

std::string EMPTY_STRING;

std::string AnimationExporter::get_axis_name(std::string channel, int id)
{
	std::map<std::string, std::vector<std::string>>::const_iterator it;
	it = BC_CHANNEL_NAME_FROM_TYPE.find(channel);
	if (it == BC_CHANNEL_NAME_FROM_TYPE.end())
		return "";

	const std::vector<std::string> &subchannel = it->second;
	if (id >= subchannel.size())
		return "";
	return subchannel[id];
}

bool AnimationExporter::open_animation_container(bool has_container, Object *ob)
{
	if (!has_container) {
		char anim_id[200];
		sprintf(anim_id, "action_container-%s", translate_id(id_name(ob)).c_str());
		openAnimation(anim_id, id_name(ob));
	}
	return true;
}

void AnimationExporter::openAnimationWithClip(std::string action_id, std::string action_name)
{
	std::vector<std::string> anim_meta_entry;
	anim_meta_entry.push_back(translate_id(action_id));
	anim_meta_entry.push_back(action_name);
	anim_meta.push_back(anim_meta_entry);

	openAnimation(translate_id(action_id), action_name);
}

void AnimationExporter::close_animation_container(bool has_container)
{
	if (has_container)
		closeAnimation();
}

bool AnimationExporter::exportAnimations(Scene *sce)
{
	bool has_anim_data = BCAnimationSampler::has_animations(sce, this->export_settings->export_set);
	if (has_anim_data) {
		animation_sampler = new BCAnimationSampler();

		this->scene = sce;

		std::set<Object *> animated_subset;
		BCAnimationSampler::get_animated_subset(animated_subset, this->export_settings->export_set);

		LinkNode *node;
		for (node = this->export_settings->export_set; node; node = node->next) {
			Object *ob = (Object *)node->link;
			if (animated_subset.find(ob) != animated_subset.end()) {
				animation_sampler->add_object(ob);
			}
		}

		animation_sampler->sample_scene(scene,
			export_settings->sampling_rate,
			/*keyframe_at_end = */ true,
			export_settings->open_sim,
			export_settings->keep_keyframes,
			export_settings->export_animation_type
			);

		openLibrary();

		std::set<Object *>::iterator it;
		for (it = animated_subset.begin(); it != animated_subset.end(); ++it) {
			Object *ob = *it;
			exportAnimation(ob, *animation_sampler);
		}

		delete animation_sampler;
		closeLibrary();

#if 0
		/* TODO: If all actions shall be exported, we need to call the
		 * AnimationClipExporter which will figure out which actions
		 * need to be exported for which objects
		 */ 
		if (this->export_settings->include_all_actions) {
			AnimationClipExporter ace(eval_ctx, sw, export_settings, anim_meta);
			ace.exportAnimationClips(sce);
		}
#endif
	}
	return has_anim_data;
}


/* called for each exported object */
void AnimationExporter::exportAnimation(Object *ob, BCAnimationSampler &sampler)
{
	bool container_is_open = false;

	//Transform animations (trans, rot, scale)
	container_is_open = open_animation_container(container_is_open, ob);

	/* Now take care of the Object Animations
	 * Note: For Armatures the skeletal animation has already been exported (see above)
	 * However Armatures also can have Object animation.
	 */
	bool export_tm_curves = this->export_settings->export_transformation_type == BC_TRANSFORMATION_TYPE_TRANSROTLOC;
	if (!export_tm_curves) {
		export_matrix_animation(ob, sampler); // export all transform_curves as one single matrix animation
	}

	export_curve_animation_set(ob, sampler, export_tm_curves);

	if (ob->type == OB_ARMATURE) {

#ifdef WITH_MORPH_ANIMATION
		/* TODO: This needs to be handled by extra profiles, postponed for now */
		export_morph_animation(ob);
#endif

		/* Export skeletal animation (if any) */
		bArmature *arm = (bArmature *)ob->data;
		for (Bone *root_bone = (Bone *)arm->bonebase.first; root_bone; root_bone = root_bone->next)
			export_bone_animations_recursive(ob, root_bone, sampler);
	}

	close_animation_container(container_is_open);
}

/*
 * Export all animation FCurves of an Object.
 *
 * Note: This uses the keyframes as sample points,
 * and exports "baked keyframes" while keeping the tangent information
 * of the FCurves intact. This works for simple cases, but breaks
 * especially when negative scales are involved in the animation.
 * And when parent inverse matrices are involved (when exporting
 * object hierarchies)
 *
 */
void AnimationExporter::export_curve_animation_set(Object *ob, BCAnimationSampler &sampler, bool export_tm_curves)
{
	BCFrameSampleMap samples;
	BCAnimationCurveMap curves;

	sampler.get_curves(curves, ob);
	sampler.get_samples(samples, ob);

	BCAnimationCurveMap::iterator it;
	for (it = curves.begin(); it != curves.end(); ++it) {
		BCAnimationCurve &curve = it->second;
		if (curve.get_channel_target() == "rotation_quaternion") {
			/*
			   Can not export Quaternion animation in Collada as far as i know)
			   Maybe automatically convert to euler rotation?
			   Discard for now.
			*/
			continue;
		}

		if (!export_tm_curves && curve.is_transform_curve()) {
			/* The transform curves will be eported as single matrix animation
			 * So no need to export the curves here again.
			 */
			continue;
		}

		sampler.add_value_set(curve, samples, this->export_settings->export_animation_type);  // prepare curve
		if (curve.is_flat())
			continue;

		export_curve_animation(ob, curve);
	}
}

void AnimationExporter::export_matrix_animation(Object *ob, BCAnimationSampler &sampler)
{
	std::vector<float> frames;
	sampler.get_frame_set(frames, ob);
	if (frames.size() > 0) {
		BCMatrixSampleMap samples;
		bool is_flat = sampler.get_samples(samples, ob);
		if (!is_flat) {
			bAction *action = bc_getSceneObjectAction(ob);
			std::string name = id_name(ob);
			std::string action_name = (action == nullptr) ? name + "-action" : id_name(action);
			std::string channel_type = "transform";
			std::string axis = "";
			std::string id = bc_get_action_id(action_name, name, channel_type, axis);

			std::string target = translate_id(name) + '/' + channel_type;

			export_collada_matrix_animation(id, name, target, frames, samples);
		}
	}
}

//write bone animations in transform matrix sources
void AnimationExporter::export_bone_animations_recursive(Object *ob, Bone *bone, BCAnimationSampler &sampler)
{
	std::vector<float> frames;
	sampler.get_frame_set(frames, ob, bone);
	
	if (frames.size()) {
		BCMatrixSampleMap samples;
		bool is_flat = sampler.get_samples(samples, ob, bone);
		if (!is_flat) {
			export_bone_animation(ob, bone, frames, samples);
		}
	}

	for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next)
		export_bone_animations_recursive(ob, child, sampler);
}

#ifdef WITH_MORPH_ANIMATION
/* TODO: This function needs to be implemented similar to the material animation export
   So we have to update BCSample for this to work.
*/
void AnimationExporter::export_morph_animation(Object *ob, BCAnimationSampler &sampler)
{ 
	FCurve *fcu;
	Key *key = BKE_key_from_object(ob);
	if (!key) return;

	if (key->adt && key->adt->action) {
		fcu = (FCurve *)key->adt->action->curves.first;
		
		while (fcu) {
			BC_animation_transform_type tm_type = get_transform_type(fcu->rna_path);

			create_keyframed_animation(ob, fcu, tm_type, true, sampler);
			
			fcu = fcu->next;
		}
	}

}
#endif

/* Euler sources from quternion sources 
 * Important: We assume the object has a scene action.
 * If it has not, then Blender will die
*/
void AnimationExporter::get_eul_source_for_quat(std::vector<float> &values, Object *ob)
{
	bAction *action = bc_getSceneObjectAction(ob);

	FCurve *fcu = (FCurve *)action->curves.first;
	const int keys = fcu->totvert;
	std::vector<std::vector<float>> quats;
	quats.resize(keys);
	for (int i = 0; i < keys; i++)
		quats[i].resize(4);

	int curve_count = 0;
	while (fcu) {
		std::string transformName = bc_string_after(fcu->rna_path, '.');

		if (transformName == "rotation_quaternion" ) {
			curve_count += 1;
			for (int i = 0; i < fcu->totvert; i++) {
				std::vector<float> &quat = quats[i];
				quat[fcu->array_index] = fcu->bezt[i].vec[1][1];
			}
			if (curve_count == 4)
				break; /* Quaternion curves can not use more the 4 FCurves!*/
		}
		fcu = fcu->next;
	}

	float feul[3];
	for (int i = 0; i < keys; i++) {
		std::vector<float> &quat = quats[i];
		quat_to_eul(feul, &quat[0]);

		for (int k = 0; k < 3; k++)
			values.push_back(feul[k]);
	}
}

/*
* In some special cases the exported Curve needs to be replaced
* by a modified curve (for collada purposes)
* This method checks if a conversion is necessary and if applicable
* returns a pointer to the modified BCAnimationCurve.
* IMPORTANT: the modified curve must be deleted by the caller when no longer needed
* if no conversion is needed this method returns a nullptr;
*/
BCAnimationCurve *AnimationExporter::get_modified_export_curve(Object *ob, BCAnimationCurve curve)
{
	BC_animation_transform_type tm_type = curve.get_transform_type();
	BCAnimationCurve *mcurve = nullptr;
	if (tm_type == BC_ANIMATION_TYPE_LENS) {

		/* Create an xfov curve */

		BCFrameSampleMap sample_map;
		animation_sampler->get_samples(sample_map, ob);
		mcurve = new BCAnimationCurve(curve);
		mcurve->set_transform_type("xfov", 0);
		mcurve->reset_values();
		// now tricky part: transform the fcurve
		const BCValueMap &value_map = curve.get_value_map();
		BCValueMap::const_iterator it;
		for (it = value_map.begin(); it != value_map.end(); ++it) {
			int frame = it->first;
			float value = it->second;
			const BCSample *sample = sample_map[frame];
			if (sample)
			{
				/* recalculate the value of xfov in degrees */
				const BCCamera &camera = sample->get_camera();
				float lens = camera.lens;
				float sensor = camera.xsensor;
				value = RAD2DEGF(focallength_to_fov(lens, sensor));
				mcurve->add_value(value, frame, /*modify_curve=*/true);
			}
		}
		mcurve->clean_handles(); // to reset the handles
	}
	return mcurve;
}

/* convert f-curves to animation curves and write
* Important: We assume the object has a scene action.
* If it has not, then Blender will die!
*/
void AnimationExporter::export_curve_animation(
	Object *ob,
	BCAnimationCurve &icurve)
{
	std::string channel = icurve.get_channel_target();
	BC_animation_curve_type channel_type = icurve.get_channel_type();

	/*
	 * Some curves can not be exported as is and need some conversion
	 * For more information see implementation oif get_modified_export_curve()
	 * note: if mcurve is not nullptr then it must be deleted at end of this method;
	 */

	BCAnimationCurve *mcurve = get_modified_export_curve(ob, icurve);
	BCAnimationCurve &curve = (mcurve) ? *mcurve : icurve;

	int array_index = curve.get_array_index();
	std::string axis = get_axis_name(channel, array_index); // RGB or XYZ

	std::string action_name;
	bAction *action = bc_getSceneObjectAction(ob);
	action_name = (action) ? id_name(action) : "constraint_anim";

	const std::string curve_name = curve.get_animation_name(ob);
	std::string id = bc_get_action_id(action_name, curve_name, channel, axis, ".");
	
	std::string target = translate_id(curve_name);

	if (channel_type == BC_ANIMATION_CURVE_TYPE_MATERIAL) {
		int material_index = curve.get_tag();
		Material *ma = give_current_material(ob, material_index + 1);
		if (ma) {
			target = id_name(ma) + "-effect/common/" + get_collada_sid(curve, axis);
		}
	}
	else {
		target += "/" + get_collada_sid(curve, axis);
	}

	export_collada_curve_animation( id, curve_name, target, axis, curve);

	if (mcurve) {
		delete mcurve; // The modified curve is no longer needed
	}
}

void AnimationExporter::export_bone_animation(Object *ob, Bone *bone, BCFrames &frames, BCMatrixSampleMap &samples)
{
	bAction* action = bc_getSceneObjectAction(ob);
	std::string bone_name(bone->name);
	std::string name = id_name(ob);
	std::string id = bc_get_action_id(id_name(action), name, bone_name, "pose_matrix");
	std::string target = translate_id(id_name(ob) + "_" + bone_name) + "/transform";

	export_collada_matrix_animation(id, name, target, frames, samples);
}

bool AnimationExporter::is_bone_deform_group(Bone *bone)
{   
	bool is_def;
	//Check if current bone is deform
	if ((bone->flag & BONE_NO_DEFORM) == 0) return true;
	//Check child bones
	else {
		for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
			//loop through all the children until deform bone is found, and then return
			is_def = is_bone_deform_group(child);
			if (is_def) return true;
		}
	}
	//no deform bone found in children also
	return false;
}


void AnimationExporter::export_collada_curve_animation(
	std::string id,
	std::string name,
	std::string target,
	std::string axis,
	const BCAnimationCurve &curve)
{
	BCFrames frames;
	BCValues values;
	curve.get_sampled_frames(frames);
	curve.get_sampled_values(values);
	BC_animation_transform_type tm_type = curve.get_transform_type();

	fprintf(stdout, "Export animation curve %s (%d control points)\n", id.c_str(), int(frames.size()));
	openAnimation(id, name);
	std::string intangent_id;
	std::string outtangent_id;
	bool has_tangents = false;
	bool is_rot = curve.is_rot();

	std::string input_id = collada_source_from_values(BC_ANIMATION_TYPE_TIMEFRAME, COLLADASW::InputSemantic::INPUT, frames, false, id, axis);
	std::string output_id = collada_source_from_values(tm_type, COLLADASW::InputSemantic::OUTPUT, values, is_rot, id, axis);

	std::string interpolation_id;
	if (this->export_settings->keep_smooth_curves)
		interpolation_id = collada_interpolation_source(curve, id, axis, &has_tangents);
	else
		interpolation_id = collada_linear_interpolation_source(frames.size(), id);

	if (has_tangents) {
		intangent_id = collada_tangent_from_curve(COLLADASW::InputSemantic::IN_TANGENT, curve, frames, id, axis);
		outtangent_id = collada_tangent_from_curve(COLLADASW::InputSemantic::OUT_TANGENT, curve, frames, id, axis);
	}

	std::string sampler_id = std::string(id) + SAMPLER_ID_SUFFIX;
	COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);

	sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(EMPTY_STRING, input_id));
	sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(EMPTY_STRING, output_id));
	sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(EMPTY_STRING, interpolation_id));

	if (has_tangents) {
		sampler.addInput(COLLADASW::InputSemantic::IN_TANGENT, COLLADABU::URI(EMPTY_STRING, intangent_id));
		sampler.addInput(COLLADASW::InputSemantic::OUT_TANGENT, COLLADABU::URI(EMPTY_STRING, outtangent_id));
	}

	addSampler(sampler);
	addChannel(COLLADABU::URI(EMPTY_STRING, sampler_id), target);

	closeAnimation();
}

void AnimationExporter::export_collada_matrix_animation(std::string id, std::string name, std::string target, BCFrames &frames, BCMatrixSampleMap &samples)
{
	fprintf(stdout, "Export animation matrix %s (%d control points)\n", id.c_str(), int(frames.size()));

	openAnimationWithClip(id, name);

	std::string input_id = collada_source_from_values(BC_ANIMATION_TYPE_TIMEFRAME, COLLADASW::InputSemantic::INPUT, frames, false, id, "");
	std::string output_id = collada_source_from_values(samples, id);
	std::string interpolation_id = collada_linear_interpolation_source(frames.size(), id);

	std::string sampler_id = std::string(id) + SAMPLER_ID_SUFFIX;
	COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);

	sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(EMPTY_STRING, input_id));
	sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(EMPTY_STRING, output_id));
	sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(EMPTY_STRING, interpolation_id));

	// Matrix animation has no tangents

	addSampler(sampler);
	addChannel(COLLADABU::URI(EMPTY_STRING, sampler_id), target);

	closeAnimation();
}

std::string AnimationExporter::get_semantic_suffix(COLLADASW::InputSemantic::Semantics semantic)
{
	switch (semantic) {
		case COLLADASW::InputSemantic::INPUT:
			return INPUT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::OUTPUT:
			return OUTPUT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::INTERPOLATION:
			return INTERPOLATION_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::IN_TANGENT:
			return INTANGENT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::OUT_TANGENT:
			return OUTTANGENT_SOURCE_ID_SUFFIX;
		default:
			break;
	}
	return "";
}

void AnimationExporter::add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
	COLLADASW::InputSemantic::Semantics semantic,
	bool is_rot, 
	const std::string axis, 
	bool transform)
{
	switch (semantic) {
		case COLLADASW::InputSemantic::INPUT:
			param.push_back("TIME");
			break;
		case COLLADASW::InputSemantic::OUTPUT:
			if (is_rot) {
				param.push_back("ANGLE");
			}
			else {
				if (axis != "") {
					param.push_back(axis);
				}
				else 
				if (transform) {
					param.push_back("TRANSFORM");
				}
				else {     //assumes if axis isn't specified all axises are added
					param.push_back("X");
					param.push_back("Y");
					param.push_back("Z");
				}
			}
			break;
		case COLLADASW::InputSemantic::IN_TANGENT:
		case COLLADASW::InputSemantic::OUT_TANGENT:
			param.push_back("X");
			param.push_back("Y");
			break;
		default:
			break;
	}
}

/*
Use this when the curve has different keyframes than the underlaying FCurve.
This can happen when the curve contains sample points. However
currently the BCAnimationSampler makes sure that sampled points
are added to the FCurve, hence this function will always find a matching
keyframe;
*/
int AnimationExporter::get_point_in_curve(const BCAnimationCurve &curve, float val, COLLADASW::InputSemantic::Semantics semantic, bool is_angle, float *values)
{
	const FCurve *fcu = curve.get_fcurve();
	int lower_index = curve.closest_index_below(val);
	return get_point_in_curve(&fcu->bezt[lower_index], semantic, is_angle, values);
}


int AnimationExporter::get_point_in_curve(BezTriple *bezt, COLLADASW::InputSemantic::Semantics semantic, bool is_angle, float *values)
{
	int length;
	switch (semantic) {
	case COLLADASW::InputSemantic::INPUT:
		length = 1;
		values[0] = BCAnimationCurve::get_time(bezt, scene);
		break;
	case COLLADASW::InputSemantic::OUTPUT:
		length = 1;
		values[0] = BCAnimationCurve::get_value(bezt, is_angle);
		break;

	case COLLADASW::InputSemantic::IN_TANGENT:
		length = 2;
		BCAnimationCurve::get_in_tangent(bezt, scene, values, is_angle);
		break;

	case COLLADASW::InputSemantic::OUT_TANGENT:
		length = 2;
		BCAnimationCurve::get_out_tangent(bezt, scene, values, is_angle);
		break;

	default:
		length = 0;
		break;
	}
	return length;
}

std::string AnimationExporter::collada_tangent_from_curve(COLLADASW::InputSemantic::Semantics semantic, const BCAnimationCurve &curve, std::vector<float>frames, const std::string& anim_id, std::string axis_name)
{
	std::string channel = curve.get_channel_target();

	const std::string source_id = anim_id + get_semantic_suffix(semantic);

	bool is_angle = (bc_startswith(channel, "rotation") || channel == "spot_size");
	bool is_euler = (channel == "rotation_euler");

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(curve.size());
	source.setAccessorStride(2);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, is_angle, axis_name, false);

	source.prepareToAppendValues();

	std::vector<float> values;
	curve.get_sampled_values(values);

	const FCurve *fcu = curve.get_fcurve(); // need this to get the original tangents

	for (unsigned int frame_index = 0; frame_index < values.size(); frame_index++) {
		float sampled_val = values[frame_index];

		if (is_angle) {
			sampled_val = RAD2DEGF(sampled_val);
		}

		float vals[3]; // be careful!
		int length = get_point_in_curve(curve, frames[frame_index], semantic, is_angle, vals);
		float offset = 0;
		float bases[3];
		int len = get_point_in_curve(curve, frames[frame_index], COLLADASW::InputSemantic::OUTPUT, is_angle, bases);
		sampled_val += vals[1] - bases[0];

		source.appendValues(vals[0]);
		source.appendValues(sampled_val);

	}
	source.finish();
	return source_id;
}

std::string AnimationExporter::collada_source_from_values(
	BC_animation_transform_type tm_type,
	COLLADASW::InputSemantic::Semantics semantic,
	std::vector<float> &values,
	bool is_rot,
	const std::string& anim_id,
	const std::string axis_name)
{
	/* T can be float, int or double */

	int stride = 1;
	int entry_count = values.size() / stride;
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(entry_count);
	source.setAccessorStride(stride);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, is_rot, axis_name, false);

	source.prepareToAppendValues();

	for (int i = 0; i < entry_count; i++) {
		float val = values[i];
		switch (tm_type) {
		case BC_ANIMATION_TYPE_TIMEFRAME:
			val = FRA2TIME(val);
			break;
		case BC_ANIMATION_TYPE_ROTATION_EULER:
		case BC_ANIMATION_TYPE_ROTATION:
			val = RAD2DEGF(val);
			break;
		default: break;
		}
		source.appendValues(val);
	}

	source.finish();

	return source_id;
}

/*
 * Create a collada matrix source for a set of samples
*/
std::string AnimationExporter::collada_source_from_values(BCMatrixSampleMap &samples, const std::string &anim_id)
{
	COLLADASW::InputSemantic::Semantics semantic = COLLADASW::InputSemantic::OUTPUT;
	std::string source_id = anim_id + get_semantic_suffix(semantic);

	COLLADASW::Float4x4Source source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(samples.size());
	source.setAccessorStride(16);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	add_source_parameters(param, semantic, false, "", true);

	source.prepareToAppendValues();

	BCMatrixSampleMap::iterator it;
	int j = 0;
	int precision = (this->export_settings->limit_precision) ? 6 : -1; // could be made configurable
	for (it = samples.begin(); it != samples.end(); it++) {
		const BCMatrix *sample = it->second;
		double daemat[4][4];
		sample->get_matrix(daemat, true, precision );
		source.appendValues(daemat);
	}

	source.finish();
	return source_id;
}

std::string AnimationExporter::collada_interpolation_source(const BCAnimationCurve &curve,
	const std::string& anim_id, 
	const std::string axis,
	bool *has_tangents)
{
	std::string source_id = anim_id + get_semantic_suffix(COLLADASW::InputSemantic::INTERPOLATION);

	COLLADASW::NameSource source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(curve.size());
	source.setAccessorStride(1);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("INTERPOLATION");

	source.prepareToAppendValues();

	*has_tangents = false;

	const FCurve *fcu = curve.get_fcurve();
	std::vector<float>frames;
	curve.get_sampled_frames(frames);

	for (unsigned int i = 0; i < curve.size(); i++) {
		float frame = frames[i];
		int ipo = curve.get_ipo(frame);
		if (ipo == BEZT_IPO_BEZ) {
			source.appendValues(BEZIER_NAME);
			*has_tangents = true;
		}
		else if (ipo == BEZT_IPO_CONST) {
			source.appendValues(STEP_NAME);
		}
		else { // BEZT_IPO_LIN
			source.appendValues(LINEAR_NAME);
		}
	}
	// unsupported? -- HERMITE, CARDINAL, BSPLINE, NURBS

	source.finish();

	return source_id;
}

std::string AnimationExporter::collada_linear_interpolation_source(int tot, const std::string& anim_id)
{
	std::string source_id = anim_id + get_semantic_suffix(COLLADASW::InputSemantic::INTERPOLATION);

	COLLADASW::NameSource source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(tot);
	source.setAccessorStride(1);

	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("INTERPOLATION");

	source.prepareToAppendValues();

	for (int i = 0; i < tot; i++) {
		source.appendValues(LINEAR_NAME);
	}

	source.finish();

	return source_id;
}

extern std::map<std::string, BC_animation_transform_type> BC_ANIMATION_TYPE_FROM_NAME;

BC_animation_transform_type AnimationExporter::get_transform_type(std::string path)
{
	BC_animation_transform_type tm_type;
	// when given rna_path, overwrite tm_type from it
	std::string name = bc_string_after(path, '.');
	std::map<std::string, BC_animation_transform_type>::iterator type_it = BC_ANIMATION_TYPE_FROM_NAME.find(name);
	tm_type = (type_it != BC_ANIMATION_TYPE_FROM_NAME.end()) ? type_it->second : BC_ANIMATION_TYPE_UNKNOWN;

	return tm_type;
}

const std::string AnimationExporter::get_collada_name(BC_animation_transform_type tm_type) const
{
	std::map<BC_animation_transform_type, std::string>::iterator name_it = BC_ANIMATION_NAME_FROM_TYPE.find(tm_type);
	std::string tm_name = name_it->second;
	return tm_name;
}

/*
 * Assign sid of the animated parameter or transform for rotation,
 * axis name is always appended and the value of append_axis is ignored
 */
std::string AnimationExporter::get_collada_sid(const BCAnimationCurve &curve, const std::string axis_name)
{
	BC_animation_transform_type tm_type = curve.get_transform_type();
	std::string tm_name = get_collada_name(tm_type);

	bool is_angle = (
		tm_type == BC_ANIMATION_TYPE_ROTATION_EULER ||
		tm_type == BC_ANIMATION_TYPE_ROTATION_QUAT ||
		tm_type == BC_ANIMATION_TYPE_ROTATION
		);


	if (tm_name.size()) {
		if (is_angle)
			return tm_name + std::string(axis_name) + ".ANGLE";
		else
			if (axis_name != "")
				return tm_name + "." + std::string(axis_name);
			else
				return tm_name;
	}

	return tm_name;
}
