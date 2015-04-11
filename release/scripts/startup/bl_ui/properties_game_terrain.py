# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class TerrainButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "terrain"

class TERRAIN_PT_game_context_terrain(TerrainButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.scene) and (rd.use_game_engine)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        terrain = context.terrain
        space = context.space_data

        split = layout.split(percentage=0.65)
        if scene:
            split.template_ID(scene, "terrain", new="terrain.new")
        elif terrain:
            split.template_ID(space, "pin_id")

class TERRAIN_PT_game_terrain_chunk(TerrainButtonsPanel, Panel):
    bl_label = "Chunk"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.terrain)

    def draw(self, context):
        layout = self.layout

        terrain = context.terrain
        split = layout.split()

        col = split.column()
        sub = col.column()
        sub.prop(terrain, "max_level")
        sub.prop(terrain, "width")
        sub.prop(terrain, "chunk_size")

        col = split.column()
        sub = col.column()
        sub.prop(terrain, "distance")
        sub.prop(terrain, "physics_distance")

class TERRAIN_PT_game_terrain_mesh(TerrainButtonsPanel, Panel):
    bl_label = "Mesh"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.terrain)

    def draw(self, context):
        layout = self.layout

        terrain = context.terrain

        row = layout.row()
        row.column().prop(terrain, "material")
        row.column().prop(terrain, "vertex_subdivision")

class TERRAIN_PT_game_terrain_zones(TerrainButtonsPanel, Panel):
    bl_label = "Terrain Zones"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.terrain)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        terrain = scene.terrain
        row = layout.row()

        col = row.column()
        col.template_list("UI_UL_list", "zones", terrain, "zones", terrain.zones, "active_zone_index", rows=1)

        col = row.column(align=True)
        col.operator("terrain.zone_add", icon='ZOOMIN', text="")
        col.operator("terrain.zone_remove", icon='ZOOMOUT', text="")
        col.operator("terrain.zone_move", text="", icon='TRIA_UP').direction = 'UP'
        col.operator("terrain.zone_move", text="", icon='TRIA_DOWN').direction = 'DOWN'

class TERRAIN_PT_game_terrain_zones_mesh(TerrainButtonsPanel, Panel):
    bl_label = "Zone Mesh"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw_header(self, context):
        scene = context.scene
        terrain = scene.terrain
        zone = terrain.zones.active_zone
        if zone:
            self.layout.prop(zone, "use_mesh", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        terrain = scene.terrain
        row = layout.row()
        
        if terrain:
            zone = terrain.zones.active_zone

            if zone:
                layout.active = zone.use_mesh
                row = layout.row()
                row.column().prop(zone, "mesh")
                row.column().prop(zone, "use_mesh_vertex_color_interp")

class TERRAIN_PT_game_terrain_zones_heights(TerrainButtonsPanel, Panel):
    bl_label = "Heights"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        terrain = scene.terrain
        split = layout.split()

        if terrain:
            zone = terrain.zones.active_zone
            
            if zone:
                col = split.column()
                sub = col.column()
                sub.prop(zone, "use_perlin_noise")
                sub.prop(zone, "offset")
            
                col = split.column()
                sub = col.column()
                sub.active = zone.use_perlin_noise
                sub.prop(zone, "resolution")
                sub.prop(zone, "height")

class TERRAIN_PT_game_terrain_zones_clamp(TerrainButtonsPanel, Panel):
    bl_label = "Zone Clamp"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw_header(self, context):
        scene = context.scene
        terrain = scene.terrain
        zone = terrain.zones.active_zone
        if zone:
            self.layout.prop(zone, "use_clamp", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        terrain = scene.terrain
        row = layout.row()

        if terrain:
            zone = terrain.zones.active_zone
            
            if zone:
                layout.active = zone.use_clamp
                row = layout.row()
                row.column().prop(zone, "clamp_start")
                row.column().prop(zone, "clamp_end")

class TERRAIN_PT_game_terrain_zones_vertex_info(TerrainButtonsPanel, Panel):
    bl_label = "Zone Vertex Info"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_GAME'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        terrain = scene.terrain
        row = layout.row()

        if terrain:
            zone = terrain.zones.active_zone
            
            if zone:
                layout.active = zone.use_vertex_color
                row = layout.row()
                row.column().prop(zone, "use_vertex_color")
                row.column().prop(zone, "vertex_color")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)