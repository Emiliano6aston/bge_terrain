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
#include "RNA_access.h"

#include "rna_internal.h"

#include "DNA_terrain_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static PointerRNA rna_Terrain_active_zone_get(PointerRNA *ptr)
{
	Terrain *terrain = (Terrain *)ptr->data;
	TerrainZone *zone = BLI_findlink(&terrain->zones, terrain->active_zoneindex);
	return rna_pointer_inherit_refine(ptr, &RNA_TerrainZone, zone);
}

static void rna_Terrain_active_zone_set(PointerRNA *ptr, PointerRNA value)
{
	Terrain *terrain = (Terrain *)ptr->data;
	TerrainZone *zone = (TerrainZone *)value.data;
	terrain->active_zoneindex = BLI_findindex(&terrain->zones, zone);
}

static int rna_Terrain_active_zone_index_get(PointerRNA *ptr)
{
	Terrain *terrain = (Terrain *)ptr->data;
	return terrain->active_zoneindex;
}

static void rna_Terrain_active_zone_index_set(PointerRNA *ptr, int value)
{
	Terrain *terrain = (Terrain *)ptr->data;
	terrain->active_zoneindex = value;
}

#else

static void rna_def_terrain_zone(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "TerrainZone", NULL);
	RNA_def_struct_sdna(srna, "TerrainZone");
	RNA_def_struct_nested(brna, srna, "Terrain");
	RNA_def_struct_ui_text(srna, "Terrain Zone", "");

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Line Set Name", "Terrain Zone set name");
	RNA_def_struct_name_property(srna, prop);

	prop = RNA_def_property(srna, "mesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mesh");
	RNA_def_property_struct_type(prop, "Mesh");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mesh", "");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "height");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Height", "");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Offset", "");

	prop = RNA_def_property(srna, "resolution", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "resolution");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Resolution", "");
}

static void rna_def_terrain_zone_collection(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "TerrainZoneCollection");
	srna = RNA_def_struct(brna, "TerrainZoneCollection", NULL);
	RNA_def_struct_sdna(srna, "Terrain");
	RNA_def_struct_ui_text(srna, "Render Layers", "Collection of render layers");

	prop = RNA_def_property(srna, "active_zone", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "TerrainZone");
	RNA_def_property_pointer_funcs(prop, "rna_Terrain_active_zone_get",
	                                     "rna_Terrain_active_zone_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Render Layer", "Active Render Layer");

	prop = RNA_def_property(srna, "active_zone_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Terrain_active_zone_index_get",
	                                     "rna_Terrain_active_zone_index_set", NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Render Layer", "Active Render Layer");
}

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
	RNA_def_property_ui_range(prop, 0, 16, 1, 0);
	RNA_def_property_ui_text(prop, "Max Level", "");

	prop = RNA_def_property(srna, "vertex_subdivision", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "vertexsubdivision");
	RNA_def_property_range(prop, 4, 32);
	RNA_def_property_ui_range(prop, 4, 32, 2, 0);
	RNA_def_property_ui_text(prop, "Vertex Subdivision", "");

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

	prop = RNA_def_property(srna, "noise_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noisesize");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Noise Size", "");

	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "material");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Material", "");

	rna_def_terrain_zone(brna);

	prop = RNA_def_property(srna, "zones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "zones", NULL);
	RNA_def_property_struct_type(prop, "TerrainZone");
	RNA_def_property_ui_text(prop, "Terrain Zones", "");
	rna_def_terrain_zone_collection(brna, prop);
}

#endif