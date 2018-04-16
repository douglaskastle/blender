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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_gpencilswirl.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_library_query.h"

#include "MOD_modifiertypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md)
{
	GpencilSwirlModifierData *gpmd = (GpencilSwirlModifierData *)md;
	gpmd->radius = 100;
	gpmd->angle = M_PI_2;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	GpencilSwirlModifierData *lmd = (GpencilSwirlModifierData *)md;
	if (lmd->object != NULL) {
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Swirl Modifier");
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Swirl Modifier");
	}
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Swirl Modifier");
}

static bool isDisabled(ModifierData *md, int UNUSED(userRenderParams))
{
	GpencilSwirlModifierData *mmd = (GpencilSwirlModifierData *)md;

	return !mmd->object;
}

static void foreachObjectLink(
	ModifierData *md, Object *ob,
	ObjectWalkFunc walk, void *userData)
{
	GpencilSwirlModifierData *mmd = (GpencilSwirlModifierData *)md;

	walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

ModifierTypeInfo modifierType_GpencilSwirl = {
	/* name */              "Swirl",
	/* structName */        "GpencilSwirlModifierData",
	/* structSize */        sizeof(GpencilSwirlModifierData),
	/* type */              eModifierTypeType_Gpencil,
	/* flags */             eModifierTypeFlag_GpencilMod | eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_GpencilVFX | eModifierTypeFlag_Single,

	/* copyData */          NULL,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* deformStroke */      NULL,
	/* generateStrokes */   NULL,
	/* bakeModifierGP */    NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};