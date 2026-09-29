import bpy
import math
import os
import sys
from mathutils import Matrix, Vector

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from airport_uniform import (create_airport_atlas, dress_worker,
                             hearing_protection, swatch_uv, assign_swatch,
                             bare_head, bare_arms, grip_hand)


DATAREF = "bp/anim/wing_walker_signal"
POSE_VALUES = {"stop": 1.0, "standby": 2.0, "clear": 3.0}


def reset_pose(armature, clear_animation=True):
    if clear_animation:
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


def solve_arm(armature, side, hand_target, depth_offset=0.0):
    upper = armature.pose.bones[f"CC_Base_{side}_Upperarm"]
    forearm = armature.pose.bones[f"CC_Base_{side}_Forearm"]
    hand = armature.pose.bones[f"CC_Base_{side}_Hand"]
    shoulder = upper.head.copy()
    target = Vector(hand_target)
    target.y = shoulder.y + depth_offset

    delta = target - shoulder
    distance = min(delta.length, upper.length + forearm.length - 0.001)
    direction = delta.normalized()
    upper_len = upper.length
    forearm_len = forearm.length
    along = ((upper_len * upper_len - forearm_len * forearm_len +
              distance * distance) / (2.0 * distance))
    height = math.sqrt(max(upper_len * upper_len - along * along, 0.0))
    midpoint = shoulder + direction * along
    perpendicular = Vector((-direction.z, 0, direction.x)).normalized()
    candidates = (midpoint + perpendicular * height,
                  midpoint - perpendicular * height)
    if side == "L":
        elbow = max(candidates, key=lambda point: point.x)
    else:
        elbow = min(candidates, key=lambda point: point.x)

    aim_bone(upper, elbow)
    aim_bone(forearm, target)
    hand_direction = (forearm.tail - forearm.head).normalized()
    aim_bone(hand, hand.head + hand_direction * hand.length)


def pose_worker(armature, pose_name, clear_animation=True):
    reset_pose(armature, clear_animation)
    if pose_name == "stop":
        solve_arm(armature, "L", (-5.0, 6.0, 205.0),depth_offset=5.5)
        solve_arm(armature, "R", (5.0, 6.0, 205.0),depth_offset=-5.5)
    elif pose_name == "standby":
        solve_arm(armature, "L", (60.4, 6.0, 122.2))
        solve_arm(armature, "R", (-60.4, 6.0, 122.2))
    elif pose_name == "clear":
        solve_arm(armature, "L", (60.4, 6.0, 122.2))
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
        if pose_name == "standby" or (pose_name == "clear" and side == "L"):
            direction = Vector((1 if side == "L" else -1,0,-1)).normalized()
        if pose_name == "clear" and side == "R":
            direction = Vector((0.0, 0.0, 1.0))
            color_name = "green"
            color = (0.04, 1.0, 0.08)
        arm_direction = (hand_point-world_bone_point(armature,forearm.head)).normalized()
        hands, handle_centre = grip_hand(side, hand_point, arm_direction,
                                         direction, uniform_material)
        wands.extend((obj,"worker") for obj in hands)
        start = handle_centre + direction * .068
        end = handle_centre + direction * .378
        wand = cylinder_between(
            f"wand_{pose_name}_{side}_{color_name}", start, end, 0.016,
            color)
        assign_swatch(wand, color_name, uniform_material)
        wands.append((wand, color_name))
        handle = cylinder_between(f"wand_{pose_name}_{side}_handle",
            handle_centre - direction * .060, start, .0165, (.04,.04,.04))
        assign_swatch(handle, "black", uniform_material)
        wands.append((handle, "black"))
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
                    texture_u = max(0.0, min(1.0, float(uv.x)))
                    texture_v = max(0.0, min(1.0, float(uv.y)))
                else:
                    texture_u, texture_v = swatch_uv(uv_mode)
                # Rotate the source character 180 degrees around Blender Z,
                # then convert Blender XYZ to X-Plane X/Y/-Z. The FBX rest
                # pose faces Blender -Y; this makes the OBJ face X-Plane -Z.
                xplane_coordinate = (-coordinate.x * .94, coordinate.z * .94,
                                     coordinate.y * .94)
                xplane_normal = (-normal.x, normal.z, normal.y)
                vertices.append((*xplane_coordinate, *xplane_normal,
                                 texture_u, texture_v))
                indices.append(len(indices))
    finally:
        evaluated.to_mesh_clear()


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
            if len(group) == 10:
                stream.write("IDX10\t" + "\t".join(str(index) for index in group) + "\n")
            else:
                for index in group:
                    stream.write(f"IDX\t{index}\n")
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
reset_pose(armature)
source_image = bpy.data.images["remesh_11_combined_Bake_Diffuse"]
atlas = create_airport_atlas(source_image,
    os.path.join(output_dir, "wing_walker.png"),
    os.path.join(output_dir, "wing_walker_lit.png"))
uniform_material = dress_worker(worker, atlas)
earplugs = [*hearing_protection(uniform_material),bare_head(uniform_material)]
arms = bare_arms(worker,armature,uniform_material)

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.studio_light = "paint.sl"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "BOTH"
scene.display.shading.show_specular_highlight = False
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
    for arm in arms:
        append_mesh(vertices,indices,arm,depsgraph,"worker",ground_offset)
    for accessory in earplugs:
        append_mesh(vertices, indices, accessory, depsgraph, "worker", ground_offset)
    for wand, color_name in wands:
        append_mesh(vertices, indices, wand, depsgraph, color_name,
                    ground_offset)
    pose_ranges[pose_name] = (start, len(indices) - start)
    if pose_name == "clear":
        tip = tips["R"] + Vector((0.0, 0.0, ground_offset))
        clear_light = (-tip.x * .94, tip.z * .94, tip.y * .94)
    if preview_dir is not None:
        for obj in bpy.context.scene.objects:
            obj.hide_render = obj not in {
                worker, *earplugs, *arms, *[wand for wand, _ in wands]
            }
        render_pose(scene, os.path.join(preview_dir, f"{pose_name}.png"))
        if pose_name == "standby":
            camera.location = (2.6,-4.4,1.75)
            point_camera(camera,(0,0,1.2))
            render_pose(scene,os.path.join(preview_dir,"standby-quarter.png"))
            camera.location = (1.5,-2.8,1.95)
            camera.data.lens = 90
            point_camera(camera,(0,0,1.65))
            render_pose(scene,os.path.join(preview_dir,"uniform-detail.png"))
            camera.location=(1.1,-1.7,1.4);camera.data.lens=110
            point_camera(camera,(.66,0,1.15))
            render_pose(scene,os.path.join(preview_dir,"hand-detail.png"))
            camera.location = (0,-5.1,1.35);camera.data.lens=55
            point_camera(camera,(0,0,1.22))
    for wand, _ in wands:
        bpy.data.objects.remove(wand, do_unlink=True)

# Weld identical exported vertex records, keeping all UV/normal seams.
unique = {}; packed = []; remapped = []
for vertex in vertices:
    key = tuple(round(value, 8) for value in vertex)
    if key not in unique:
        unique[key] = len(packed); packed.append(vertex)
    remapped.append(unique[key])
vertices, indices = packed, remapped
write_obj8(os.path.join(output_dir, "wing_walker.obj"), vertices, indices,
           pose_ranges, clear_light)
if preview_dir is not None:
    reset_pose(armature)
    for pose_name, frame in (("stop",1),("standby",40),("clear",80)):
        pose_worker(armature,pose_name,clear_animation=False)
        for bone in armature.pose.bones:
            bone.rotation_mode = 'QUATERNION'
            bone.keyframe_insert(data_path='rotation_quaternion',frame=frame)
            bone.keyframe_insert(data_path='location',frame=frame)
    scene.frame_start=1;scene.frame_end=80
    # The editable file mirrors the runtime's three held signals, including
    # a separate upright green clear wand and matching grip at frame 80.
    for layer in armature.animation_data.action.layers:
        for strip in layer.strips:
            for channelbag in strip.channelbags:
                for curve in channelbag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation='CONSTANT'
    poses=(("stop",1),("standby",40),("clear",80))
    for pose_name,frame in poses:
        scene.frame_set(frame)
        wands,_=make_wands(armature,pose_name)
        bpy.context.view_layer.update()
        for obj,_ in wands:
            matrix=obj.matrix_world.copy()
            side='L' if obj.name.startswith('L ') or '_L_' in obj.name else 'R'
            obj.parent=armature;obj.parent_type='BONE'
            obj.parent_bone=f'CC_Base_{side}_Hand'
            bpy.context.view_layer.update()
            obj.matrix_world=matrix
            for signal_name,signal_frame in poses:
                obj.hide_render=signal_name!=pose_name
                obj.hide_viewport=signal_name!=pose_name
                obj.keyframe_insert(data_path='hide_render',frame=signal_frame)
                obj.keyframe_insert(data_path='hide_viewport',frame=signal_frame)
    for obj in [worker,*earplugs,*arms]:
        obj.hide_render=False
    scene.frame_set(40)
    bpy.context.view_layer.update()
    bpy.ops.file.pack_all()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(preview_dir,"airport-wing-walker.blend"))
print("BP_WING_WALKER_BUILD_COMPLETE")
print({"vertices": len(vertices), "indices": len(indices),
       "pose_ranges": pose_ranges, "clear_light": clear_light})
