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
* The Original Code is Copyright (C) 2008 Blender Foundation.
* All rights reserved.
*
* Contributor(s): Blender Foundation
*
* ***** END GPL LICENSE BLOCK *****
*/

#ifndef __BC_SAMPLE_H__
#define __BC_SAMPLE_H__

#include <string>
#include <map>
#include <algorithm>

extern "C"
{
#include "BLI_math_rotation.h"
#include "DNA_object_types.h"
#include "DNA_armature_types.h"
#include "DNA_material_types.h"
#include "DNA_lamp_types.h"
#include "DNA_camera_types.h"
}

/*
 * The list of currently supported animation types
 * TODO: Maybe this can be made more general.
*/
typedef enum BC_animation_transform_type {

	/* Translation channels */
	BC_ANIMATION_TYPE_ROTATION_EULER = 0,
	BC_ANIMATION_TYPE_ROTATION_QUAT = 1,
	BC_ANIMATION_TYPE_SCALE = 2,
	BC_ANIMATION_TYPE_LOCATION = 3,

	/* Material channels */
	BC_ANIMATION_TYPE_SPECULAR_HARDNESS = 4,
	BC_ANIMATION_TYPE_SPECULAR_COLOR = 5,
	BC_ANIMATION_TYPE_DIFFUSE_COLOR = 6,
	BC_ANIMATION_TYPE_ALPHA = 7,
	BC_ANIMATION_TYPE_IOR = 8,

	/* Lamp channels */
	BC_ANIMATION_TYPE_LIGHT_COLOR,
	BC_ANIMATION_TYPE_LIGHT_FALLOFF_ANGLE,
	BC_ANIMATION_TYPE_LIGHT_FALLOFF_EXPONENT,
	BC_ANIMATION_TYPE_LIGHT_BLENDER_DIST,

	/* Camera channels */
	BC_ANIMATION_TYPE_LENS,
	BC_ANIMATION_TYPE_XFOV,
	BC_ANIMATION_TYPE_SENSOR_X,
	BC_ANIMATION_TYPE_SENSOR_Y,
	BC_ANIMATION_TYPE_XMAG,
	BC_ANIMATION_TYPE_ZFAR,
	BC_ANIMATION_TYPE_ZNEAR,

	/* other */
	BC_ANIMATION_TYPE_ROTATION,
	BC_ANIMATION_TYPE_UNKNOWN = -1,
	BC_ANIMATION_TYPE_TIMEFRAME = -2
} BC_animation_transform_type;


class BCMaterial {
public:
	float specular_hardness;
	float specular_color[3];
	float diffuse_color[3];
	float alpha;
	float ior;
};

class BCLamp {
public:
	float light_color[3];
	float falloff_angle;
	float falloff_exponent;
	float blender_dist;
};

class BCCamera {
public:
	float lens;
	float xfov;
	float xsensor;
	float ysensor;
	float xmag;
	float zfar;
	float znear;
};


/*
Matrix is a convenience typedef for float[4][4]
BCMatrix is the class version of it. I tried to get away
without the typedef but then i get into trouble with the
matrix math functions of blender
*/

typedef float(Matrix)[4][4];
typedef float(BCEuler)[3];
typedef float(BCScale)[3];
typedef float(BCQuat)[4];
typedef float(BCSize)[3];
typedef float(BCLocation)[3];

class BCMatrix {

private:
	mutable float size[3];
	mutable float rot[3];
	mutable float loc[3];
	mutable float q[4];

	/* Private methods */
	void unit();
	void copy(Matrix &r, Matrix &a);
	void set_transform(Object *ob);

public:
	mutable float matrix[4][4];

	float(&location() const)[3];
	float(&rotation() const)[3];
	float(&scale() const)[3];
	float(&quat() const)[4];

	BCMatrix(Matrix &mat);
	BCMatrix(Object *ob);
	void set_transform(Matrix &mat);

	void get_matrix(double(&mat)[4][4], const bool transposed = false, const int precision = -1) const;
	const bool in_range(const BCMatrix &other, float distance) const;
	/* Convenient helper functions */
	static void sanitize(Matrix &matrix, int precision);
	static void transpose(Matrix &matrix);
};

typedef std::map<int, BCMaterial *> BCMaterialMap;
typedef std::map<Bone *, BCMatrix *> BCBoneMatrixMap;

class BCSample{
private:
	
	/* For Object Transformations */
	BCMatrix matrix;

	/* XXX: The following parts are exclusive, each BCSample has 
	   at most one of those filled with data. 
	   Maybe we can make a union here?
	*/

	BCMaterialMap material_map; /* For Material animation */
	BCBoneMatrixMap bone_matrix_map; /* For Armature animation */
	BCLamp lamp; /* For Lamp channels */
	BCCamera camera; /* For Camera channels */

public:
	BCSample(Object *ob); // calculate object transforms from object
	~BCSample();

	// Add sampled data for animated channels (see BC_animation_transform_type above)

	void set_material(Material *ma); // add an animated material
	void set_lamp(Lamp *lamp);
	void set_camera(Camera *camera);
	const BCCamera &get_camera() const;
	const BCLamp &get_lamp() const;
	const BCMaterial &get_material(int index);

	void set_bone(Bone *bone, Matrix &mat); // add an animated bone pose matrix

	const bool set_vector(BC_animation_transform_type channel, float val[3]);
	const bool set_value(BC_animation_transform_type channel, const int array_index, float val);

	// used for exporting all other channels
	const bool get_value(BC_animation_transform_type channel, const int array_index, float *val) const;
	const bool get_value(BC_animation_transform_type channel, const int array_index, float *val, int ma_index) const;

	// used for export as Matrixdata (note: Bone animation always uses this export type!)
	const BCMatrix *get_matrix() const; // always returns a matrix
	const BCMatrix *get_matrix(Bone *bone) const; // returns nullptr if bone is not animated

};

#endif