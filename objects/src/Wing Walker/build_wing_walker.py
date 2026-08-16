import bpy
import math
import os
import sys
from array import array
from mathutils import Matrix, Vector


DATAREF = "bp/anim/wing_walker_signal"
POSE_VALUES = {"stop": 1.0, "standby": 2.0, "clear": 3.0}


def reset_pose(armature):
    armature.animation_data_clear()
    for pose_bone in armature.pose.bones:
        pose_bone.matrix_basis.identity()
    bpy.context.view_layer.update()


def aim_bone(pose_bone, target):
    target = Vector(target)
    current = pose_bone.tail - pose_bone.head
    desired = target - pose_bone.head
    if desired.length < 1e-6:
        return
    rotation = current.rotation_difference(desired)
    pose_bone.matrix = (
        Matrix.Translation(pose_bone.head)
        @ rotation.to_matrix().to_4x4()
        @ Matrix.Translation(-pose_bone.head)
        @ pose_bone.matrix
    )
    bpy.context.view_layer.update()


def solve_arm(armature, side, hand_target):
    upper = armature.pose.bones[f"CC_Base_{side}_Upperarm"]
    forearm = armature.pose.bones[f"CC_Base_{side}_Forearm"]
    hand = armature.pose.bones[f"CC_Base_{side}_Hand"]
    shoulder = upper.head.copy()
    target = Vector(hand_target)
    target.y = shoulder.y

    delta = Vector((target.x - shoulder.x, target.z - shoulder.z))
    distance = min(delta.length, upper.length + forearm.length - 0.001)
    direction = delta.normalized()
    upper_len = upper.length
    forearm_len = forearm.length
    along = ((upper_len * upper_len - forearm_len * forearm_len +
              distance * distance) / (2.0 * distance))
    height = math.sqrt(max(upper_len * upper_len - along * along, 0.0))
    midpoint = Vector((shoulder.x, shoulder.z)) + direction * along
    perpendicular = Vector((-direction.y, direction.x))
    candidates = (midpoint + perpendicular * height,
                  midpoint - perpendicular * height)
    if side == "L":
        elbow_2d = max(candidates, key=lambda point: point.x)
    else:
        elbow_2d = min(candidates, key=lambda point: point.x)
    elbow = Vector((elbow_2d.x, shoulder.y, elbow_2d.y))

    aim_bone(upper, elbow)
    aim_bone(forearm, target)
    hand_direction = (forearm.tail - forearm.head).normalized()
    aim_bone(hand, hand.head + hand_direction * hand.length)


def pose_worker(armature, pose_name):
    reset_pose(armature)
    if pose_name == "stop":
        solve_arm(armature, "L", (-5.0, 6.0, 205.0))
        solve_arm(armature, "R", (5.0, 6.0, 205.0))
    elif pose_name == "standby":
        solve_arm(armature, "L", (47.0, 6.0, 113.0))
        solve_arm(armature, "R", (-47.0, 6.0, 113.0))
    elif pose_name == "clear":
        solve_arm(armature, "L", (47.0, 6.0, 113.0))
        solve_arm(armature, "R", (-30.0, 6.0, 205.0))
    else:
        raise ValueError(pose_name)
    bpy.context.view_layer.update()


def make_material(name, color):
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name)
        material.diffuse_color = (*color, 1.0)
    return material


def cylinder_between(name, start, end, radius, color):
    start = Vector(start)
    end = Vector(end)
    direction = end - start
    midpoint = (start + end) * 0.5
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=10, radius=radius, depth=direction.length,
        location=midpoint)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    obj.data.materials.append(make_material(f"{name}_material", color))
    bpy.context.view_layer.update()
    return obj


def world_bone_point(armature, point):
    return armature.matrix_world @ point


def make_wands(armature, pose_name):
    wands = []
    tips = {}
    for side in ("L", "R"):
        hand = armature.pose.bones[f"CC_Base_{side}_Hand"]
        forearm = armature.pose.bones[f"CC_Base_{side}_Forearm"]
        hand_point = world_bone_point(armature, hand.head)
        direction = world_bone_point(armature, hand.head) - world_bone_point(
            armature, forearm.head)
        direction.normalize()
        color_name = "orange"
        color = (1.0, 0.18, 0.015)
        if pose_name == "clear" and side == "R":
            direction = Vector((0.0, 0.0, 1.0))
            color_name = "green"
            color = (0.04, 1.0, 0.08)
        start = hand_point - direction * 0.04
        end = hand_point + direction * 0.43
        wand = cylinder_between(
            f"wand_{pose_name}_{side}_{color_name}", start, end, 0.025,
            color)
        wands.append((wand, color_name))
        tips[side] = end
    return wands, tips


def evaluated_min_z(obj, depsgraph):
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(preserve_all_data_layers=True,
                             depsgraph=depsgraph)
    try:
        matrix = evaluated.matrix_world
        return min((matrix @ vertex.co).z for vertex in mesh.vertices)
    finally:
        evaluated.to_mesh_clear()


def append_mesh(vertices, indices, obj, depsgraph, uv_mode, ground_offset):
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(preserve_all_data_layers=True,
                             depsgraph=depsgraph)
    try:
        mesh.calc_loop_triangles()
        matrix = evaluated.matrix_world
        normal_matrix = matrix.to_3x3().inverted().transposed()
        uv_layer = mesh.uv_layers.active
        corner_normals = mesh.corner_normals
        for triangle in mesh.loop_triangles:
            for loop_index in triangle.loops:
                loop = mesh.loops[loop_index]
                coordinate = matrix @ mesh.vertices[loop.vertex_index].co
                coordinate.z += ground_offset
                normal = (normal_matrix @
                          corner_normals[loop_index].vector).normalized()
                if uv_mode == "worker" and uv_layer is not None:
                    uv = uv_layer.data[loop_index].uv
                    texture_u = max(0.0, min(1.0, float(uv.x))) * 0.5
                    texture_v = max(0.0, min(1.0, float(uv.y)))
                elif uv_mode == "green":
                    texture_u, texture_v = 0.875, 0.5
                else:
                    texture_u, texture_v = 0.625, 0.5
                # Rotate the source character 180 degrees around Blender Z,
                # then convert Blender XYZ to X-Plane X/Y/-Z. The FBX rest
                # pose faces Blender -Y; this makes the OBJ face X-Plane -Z.
                xplane_coordinate = (-coordinate.x, coordinate.z,
                                     coordinate.y)
                xplane_normal = (-normal.x, normal.z, normal.y)
                vertices.append((*xplane_coordinate, *xplane_normal,
                                 texture_u, texture_v))
                indices.append(len(indices))
    finally:
        evaluated.to_mesh_clear()


def create_atlas(source_image, texture_path, lit_texture_path):
    source_width, source_height = source_image.size
    if (source_width, source_height) != (1024, 1024):
        raise RuntimeError(
            f"Unexpected source texture size: {source_width}x{source_height}")
    source_pixels = array("f", [0.0]) * (source_width * source_height * 4)
    source_image.pixels.foreach_get(source_pixels)

    atlas_width = 2048
    atlas_height = 1024
    atlas_pixels = array("f", [0.0]) * (atlas_width * atlas_height * 4)
    lit_pixels = array("f", [0.0]) * (atlas_width * atlas_height * 4)
    for y in range(atlas_height):
        source_start = y * source_width * 4
        atlas_start = y * atlas_width * 4
        atlas_pixels[atlas_start:atlas_start + source_width * 4] = \
            source_pixels[source_start:source_start + source_width * 4]
        for x in range(source_width, atlas_width):
            index = (y * atlas_width + x) * 4
            if x < 1536:
                atlas_pixels[index:index + 4] = array(
                    "f", (1.0, 0.18, 0.015, 1.0))
                lit_pixels[index:index + 4] = array(
                    "f", (0.0, 0.0, 0.0, 1.0))
            else:
                atlas_pixels[index:index + 4] = array(
                    "f", (0.04, 1.0, 0.08, 1.0))
                lit_pixels[index:index + 4] = array(
                    "f", (0.04, 1.0, 0.08, 1.0))
        for x in range(source_width):
            lit_pixels[(y * atlas_width + x) * 4 + 3] = 1.0

    atlas = bpy.data.images.new("wing_walker_atlas", width=atlas_width,
                                height=atlas_height, alpha=True)
    atlas.pixels.foreach_set(atlas_pixels)
    atlas.filepath_raw = texture_path
    atlas.file_format = "PNG"
    atlas.save()

    lit_atlas = bpy.data.images.new("wing_walker_lit_atlas",
                                    width=atlas_width, height=atlas_height,
                                    alpha=True)
    lit_atlas.pixels.foreach_set(lit_pixels)
    lit_atlas.filepath_raw = lit_texture_path
    lit_atlas.file_format = "PNG"
    lit_atlas.save()


def write_obj8(path, vertices, indices, pose_ranges, clear_light):
    with open(path, "w", encoding="ascii", newline="\n") as stream:
        stream.write("I\n800\nOBJ\n\n")
        stream.write(
            f"POINT_COUNTS\t{len(vertices)}\t0\t0\t{len(indices)}\n")
        stream.write("TEXTURE\twing_walker.png\n")
        stream.write("TEXTURE_LIT\twing_walker_lit.png\n\n")
        for vertex in vertices:
            stream.write("VT\t" + "\t".join(
                f"{value:.8f}" for value in vertex) + "\n")
        stream.write("\n")
        for offset in range(0, len(indices), 10):
            group = indices[offset:offset + 10]
            prefix = "IDX10" if len(group) == 10 else "IDX"
            stream.write(prefix + "\t" + "\t".join(
                str(index) for index in group) + "\n")
        stream.write("\nATTR_shiny_rat\t0.08\n")
        for pose_name in ("stop", "standby", "clear"):
            start, count = pose_ranges[pose_name]
            value = POSE_VALUES[pose_name]
            stream.write("ANIM_begin\n")
            # ANIM_show only cancels a hidden state while the dataref is in
            # range; it does not hide otherwise-visible geometry by itself.
            # Establish an always-hidden baseline for all runtime values, then
            # reveal exactly one held pose.
            stream.write(f"ANIM_hide\t0\t3\t{DATAREF}\n")
            stream.write(
                f"ANIM_show\t{value - 0.25:.2f}\t{value + 0.25:.2f}\t"
                f"{DATAREF}\n")
            stream.write(f"TRIS\t{start}\t{count}\n")
            if pose_name == "clear":
                x, y, z = clear_light
                stream.write(
                    "LIGHT_PARAM\tfull_custom_halo\t"
                    f"{x:.6f}\t{y:.6f}\t{z:.6f}\t"
                    "0.04\t1\t0.08\t1\t14\t0\t0\t-1\t0.70\n")
            stream.write("ANIM_end\n")


def point_camera(camera, target):
    direction = Vector(target) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def render_pose(scene, output_path):
    scene.render.filepath = output_path
    bpy.ops.render.render(write_still=True)


arguments = sys.argv[sys.argv.index("--") + 1:]
fbx_path = os.path.abspath(arguments[0])
output_dir = os.path.abspath(arguments[1])
preview_dir = os.path.abspath(arguments[2]) if len(arguments) >= 3 else None
os.makedirs(output_dir, exist_ok=True)
if preview_dir is not None:
    os.makedirs(preview_dir, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx_path, use_anim=True)
armature = bpy.data.objects["Armature"]
worker = bpy.data.objects["CC3_Base_Plus"]
hammer = bpy.data.objects.get("Hammer")
if hammer is not None:
    bpy.data.objects.remove(hammer, do_unlink=True)

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.studio_light = "paint.sl"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "BOTH"
scene.render.resolution_x = 720
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.world = bpy.data.worlds.new("Wing Walker Preview World")
scene.world.color = (0.06, 0.07, 0.08)
bpy.ops.object.camera_add(location=(0.0, -5.1, 1.35))
camera = bpy.context.object
camera.data.lens = 55
point_camera(camera, (0.0, 0.0, 1.22))
scene.camera = camera

vertices = []
indices = []
pose_ranges = {}
clear_light = None
depsgraph = bpy.context.evaluated_depsgraph_get()

for pose_name in ("stop", "standby", "clear"):
    pose_worker(armature, pose_name)
    wands, tips = make_wands(armature, pose_name)
    bpy.context.view_layer.update()
    ground_offset = -evaluated_min_z(worker, depsgraph)
    start = len(indices)
    append_mesh(vertices, indices, worker, depsgraph, "worker", ground_offset)
    for wand, color_name in wands:
        append_mesh(vertices, indices, wand, depsgraph, color_name,
                    ground_offset)
    pose_ranges[pose_name] = (start, len(indices) - start)
    if pose_name == "clear":
        tip = tips["R"] + Vector((0.0, 0.0, ground_offset))
        clear_light = (-tip.x, tip.z, tip.y)
    if preview_dir is not None:
        for obj in bpy.context.scene.objects:
            obj.hide_render = obj not in {
                worker, *[wand for wand, _ in wands]
            }
        render_pose(scene, os.path.join(preview_dir, f"{pose_name}.png"))
    for wand, _ in wands:
        bpy.data.objects.remove(wand, do_unlink=True)

source_image = bpy.data.images["remesh_11_combined_Bake_Diffuse"]
create_atlas(
    source_image,
    os.path.join(output_dir, "wing_walker.png"),
    os.path.join(output_dir, "wing_walker_lit.png"),
)
write_obj8(os.path.join(output_dir, "wing_walker.obj"), vertices, indices,
           pose_ranges, clear_light)
print("BP_WING_WALKER_BUILD_COMPLETE")
print({"vertices": len(vertices), "indices": len(indices),
       "pose_ranges": pose_ranges, "clear_light": clear_light})
