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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_world.c
 *  \ingroup RNA
 */


#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_terrain_types.h"

#include "WM_types.h"

#ifndef RNA_RUNTIME

void RNA_def_terrain(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Terrain", "ID");
	RNA_def_struct_ui_text(srna, "Terrain",
	                       "Terrain datablock describing the environment and ambient lighting of a scene");
	RNA_def_struct_ui_icon(srna, ICON_WIRE);

	prop = RNA_def_property(srna, "max_level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxlevel");
	RNA_def_property_range(prop, 0, 16);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 3);
	RNA_def_property_ui_text(prop, "Max Level", "The maximum number of level");

	prop = RNA_def_property(srna, "width", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "width");
	RNA_def_property_range(prop, 0, 16384);
	RNA_def_property_ui_text(prop, "Width", "");

	prop = RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "distance");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Distance", "");

	prop = RNA_def_property(srna, "chunk_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "chunksize");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Chunk Size", "");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Height", "");
}

#endif