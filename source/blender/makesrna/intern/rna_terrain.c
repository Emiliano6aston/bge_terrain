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

#include "DNA_texture_types.h"
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

static EnumPropertyItem prop_noise_basis_items[] = {
	{TEX_BLENDER, "BLENDER_ORIGINAL", 0, "Blender Original",
	              "Noise algorithm - Blender original: Smooth interpolated noise"},
	{TEX_STDPERLIN, "ORIGINAL_PERLIN", 0, "Original Perlin",
	                "Noise algorithm - Original Perlin: Smooth interpolated noise"},
	{TEX_NEWPERLIN, "IMPROVED_PERLIN", 0, "Improved Perlin",
	                "Noise algorithm - Improved Perlin: Smooth interpolated noise"},
	{TEX_VORONOI_F1, "VORONOI_F1", 0, "Voronoi F1",
	                 "Noise algorithm - Voronoi F1: Returns distance to the closest feature point"},
	{TEX_VORONOI_F2, "VORONOI_F2", 0, "Voronoi F2",
	                 "Noise algorithm - Voronoi F2: Returns distance to the 2nd closest feature point"},
	{TEX_VORONOI_F3, "VORONOI_F3", 0, "Voronoi F3",
	                 "Noise algorithm - Voronoi F3: Returns distance to the 3rd closest feature point"},
	{TEX_VORONOI_F4, "VORONOI_F4", 0, "Voronoi F4",
	                 "Noise algorithm - Voronoi F4: Returns distance to the 4th closest feature point"},
	{TEX_VORONOI_F2F1, "VORONOI_F2_F1", 0, "Voronoi F2-F1", "Noise algorithm - Voronoi F1-F2"},
	{TEX_VORONOI_CRACKLE, "VORONOI_CRACKLE", 0, "Voronoi Crackle",
	                      "Noise algorithm - Voronoi Crackle: Voronoi tessellation with sharp edges"},
	{TEX_CELLNOISE, "CELL_NOISE", 0, "Cell Noise",
	                "Noise algorithm - Cell Noise: Square cell tessellation"},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem prop_musgrave_type[] = {
	{TEX_MFRACTAL, "MULTIFRACTAL", 0, "Multifractal", "Use Perlin noise as a basis"},
	{TEX_RIDGEDMF, "RIDGED_MULTIFRACTAL", 0, "Ridged Multifractal",
	               "Use Perlin noise with inflection as a basis"},
	{TEX_HYBRIDMF, "HYBRID_MULTIFRACTAL", 0, "Hybrid Multifractal",
	               "Use Perlin noise as a basis, with extended controls"},
	{TEX_FBM, "FBM", 0, "fBM", "Fractal Brownian Motion, use Brownian noise as a basis"},
	{TEX_HTERRAIN, "HETERO_TERRAIN", 0, "Hetero Terrain", "Similar to multifractal"},
	{0, NULL, 0, NULL, NULL}
};

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

	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_ACTIVE);
	RNA_def_property_ui_text(prop, "Active", "");

	prop = RNA_def_property(srna, "use_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_MESH);
	RNA_def_property_ui_text(prop, "Use Mesh", "");

	prop = RNA_def_property(srna, "mesh", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mesh");
	RNA_def_property_struct_type(prop, "Mesh");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Mesh", "");

	prop = RNA_def_property(srna, "use_mesh_vertex_color_interp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_MESH_VERTEX_COLOR_INTERP);
	RNA_def_property_ui_text(prop, "Use Mesh Vertex Color Interpolation", "");

	prop = RNA_def_property(srna, "use_noise", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_PERLIN_NOISE);
	RNA_def_property_ui_text(prop, "Use Noise", "");

	prop = RNA_def_property(srna, "noise_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "noiseheight");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Noise Height", "");

	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "offset");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Offset", "");

	prop = RNA_def_property(srna, "resolution", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "resolution");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Resolution", "");

	prop = RNA_def_property(srna, "octaves", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "octaves");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Octaves", "");

	prop = RNA_def_property(srna, "lacunarity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "lacunarity");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Lacunarity", "Gap between successive frequencies");

	prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gain");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Gain", "The gain multiplier");

	prop = RNA_def_property(srna, "dimension_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "H");
	RNA_def_property_range(prop, 0.0001, 2);
	RNA_def_property_ui_text(prop, "Highest Dimension", "Highest fractal dimension");

	prop = RNA_def_property(srna, "musgrave_offset", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "musgraveoffset");
	RNA_def_property_range(prop, 0, 6);
	RNA_def_property_ui_text(prop, "Offset", "The fractal offset");

	prop = RNA_def_property(srna, "noise_basis", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "noisebasis");
	RNA_def_property_enum_items(prop, prop_noise_basis_items);
	RNA_def_property_ui_text(prop, "Noise Basis", "Noise basis used for turbulence");

	prop = RNA_def_property(srna, "musgrave_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "musgravetype");
	RNA_def_property_enum_items(prop, prop_musgrave_type);
	RNA_def_property_ui_text(prop, "Musgrave Type", "Fractal noise algorithm");

	prop = RNA_def_property(srna, "use_clamp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_CLAMP);
	RNA_def_property_ui_text(prop, "Use Clamp", "");

	prop = RNA_def_property(srna, "use_clamp_mesh", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_CLAMP_MESH);
	RNA_def_property_ui_text(prop, "Use Clamp Mesh", "");

	prop = RNA_def_property(srna, "clamp_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clampstart");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Clamp Start", "");

	prop = RNA_def_property(srna, "clamp_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clampend");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Clamp End", "");

	prop = RNA_def_property(srna, "use_image", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_IMAGE);
	RNA_def_property_ui_text(prop, "Use Image", "");

	prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "image");
	RNA_def_property_struct_type(prop, "Image");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Image", "");

	prop = RNA_def_property(srna, "image_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "imageheight");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Image Height", "");

	prop = RNA_def_property(srna, "use_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_USE_OBJECT);
	RNA_def_property_ui_text(prop, "Use Objects", "");

	prop = RNA_def_property(srna, "use_clamp_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_CLAMP_OBJECT);
	RNA_def_property_ui_text(prop, "Use Clamp Objects", "");

	prop = RNA_def_property(srna, "group_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "groupobject");
	RNA_def_property_struct_type(prop, "Group");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Group Object", "");

	prop = RNA_def_property(srna, "object_influence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "objectinfluence");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Object Influence", "");

	prop = RNA_def_property(srna, "use_uv_texture_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_USE_UV_TEXTURE_COLOR);
	RNA_def_property_ui_text(prop, "Use UV Texture Color", "");

	prop = RNA_def_property(srna, "use_height_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_USE_HEIGHT_COLOR);
	RNA_def_property_ui_text(prop, "Use Height Color", "");

	prop = RNA_def_property(srna, "use_color_dividor", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", TERRAIN_ZONE_DIVIDE_COLOR);
	RNA_def_property_ui_text(prop, "Use Color Dividor", "");

	prop = RNA_def_property(srna, "uv_channel", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "uvchannel");
	RNA_def_property_range(prop, 2, 15);
	RNA_def_property_ui_text(prop, "UV Channel", "");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "color");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Color", "");

	prop = RNA_def_property(srna, "color_dividor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colordividor");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_text(prop, "Color dividor", "");
}

static void rna_def_terrain_zone_collection(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

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

	prop = RNA_def_property(srna, "camera_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "cameradistance");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Camera Distance", "");

	prop = RNA_def_property(srna, "object_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "objectdistance");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Object Distance", "");

	prop = RNA_def_property(srna, "chunk_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "chunksize");
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Chunk Size", "");

	prop = RNA_def_property(srna, "margin_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "marginfactor");
	RNA_def_property_range(prop, 1.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Margin Factor", "");

	prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "material");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Material", "");

	prop = RNA_def_property(srna, "min_physics_level", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "minphysicslevel");
	RNA_def_property_range(prop, 0, 16);
	RNA_def_property_ui_text(prop, "Min Physics Level", "");

	prop = RNA_def_property(srna, "debug_draw_boxes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "debugmode", DEBUG_DRAW_BOXES);
	RNA_def_property_ui_text(prop, "Debug Node Boxes", "");

	prop = RNA_def_property(srna, "debug_draw_lines", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "debugmode", DEBUG_DRAW_LINES);
	RNA_def_property_ui_text(prop, "Debug Node Lines", "");

	prop = RNA_def_property(srna, "debug_draw_centers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "debugmode", DEBUG_DRAW_CENTERS);
	RNA_def_property_ui_text(prop, "Debug Node Centers", "");

	prop = RNA_def_property(srna, "debug_warnings", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "debugmode", DEBUG_WARNINGS);
	RNA_def_property_ui_text(prop, "Debug Warnings", "");

	prop = RNA_def_property(srna, "debug_errors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "debugmode", DEBUG_ERRORS);
	RNA_def_property_ui_text(prop, "Debug Errors", "");

	prop = RNA_def_property(srna, "debug_time", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "debugmode", DEBUG_TIME);
	RNA_def_property_ui_text(prop, "Debug Time", "");

	prop = RNA_def_property(srna, "debug_time_frame", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "debugtimeframe");
	RNA_def_property_range(prop, 0, 100000);
	RNA_def_property_ui_text(prop, "Debug Time Frame", "");

	rna_def_terrain_zone(brna);

	prop = RNA_def_property(srna, "zones", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "zones", NULL);
	RNA_def_property_struct_type(prop, "TerrainZone");
	RNA_def_property_ui_text(prop, "Terrain Zones", "");
	rna_def_terrain_zone_collection(brna, prop);
}

#endif
