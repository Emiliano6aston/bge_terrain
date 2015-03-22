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
    COMPAT_ENGINES = {'BLENDER_GAME'}

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
            
class TERRAIN_PT_game_terrain(TerrainButtonsPanel, Panel):
    bl_label = "Terrain"
    COMPAT_ENGINES = {'BLENDER_GAME'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene.terrain)

    def draw(self, context):
        layout = self.layout

        terrain = context.terrain

        row = layout.row()
        row.column().prop(terrain, "max_level")
        row.column().prop(terrain, "width")

        row = layout.row()
        row.column().prop(terrain, "distance")
        row.column().prop(terrain, "chunk_size")

        row = layout.row()
        row.column().prop(terrain, "height")