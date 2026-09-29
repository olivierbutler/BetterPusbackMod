"""Airport uniform, fitted bare forearms and hearing protection for the source rig.

Preserves Jungle Jim's face, body and skinning. All new uniform
graphics are authored here; no downloaded clothing textures are required.
"""
import math
import bpy
import bmesh
import numpy as np
from mathutils import Vector

PALETTE = {
    "navy": (0.115, 0.155, 0.215),
    "skin": (0.30, 0.22, 0.175),
    "hair": (0.09, 0.061, 0.044),
    "black": (0.045, 0.050, 0.055),
    "silver": (0.73, 0.77, 0.79),
    "orange": (1.0, 0.22, 0.025),
    "green": (0.04, 1.0, 0.08),
    "plug": (0.06, 0.68, 0.93),
}


def swatch_uv(name):
    n = list(PALETTE).index(name)
    return (0.78125 + (n % 4) * 0.0625,
            0.25 + (n // 4) * 0.5)


def create_airport_atlas(source, diffuse_path, lit_path):
    if tuple(source.size) != (1024,1024):
        raise ValueError('The source diffuse texture must be 1024 x 1024')
    # Left half retains the original UV atlas for face, hands and shoes.
    # Middle quarter is a spatially mapped vest; right quarter is a palette.
    pixels = np.zeros((1024, 2048, 4), dtype=np.float32)
    pixels[:, :, 3] = 1
    original = np.empty(1024 * 1024 * 4, dtype=np.float32)
    source.pixels.foreach_get(original)
    pixels[:, :1024] = original.reshape(1024, 1024, 4)
    yy, xx = np.mgrid[0:1024, 0:256]
    x = (xx / 255 - 0.5) * 0.60
    z = 1.06 + yy / 1023 * 0.68
    fabric = 0.97 + 0.018 * np.sin(xx * 2.8) * np.sin(yy * 2.1)
    vest = np.zeros((1024, 256, 3), dtype=np.float32)
    vest[:] = (0.77, 0.92, 0.065)
    # Two waist bands plus shoulder strips, repeated on front and rear.
    stripes = ((abs(x) > .084) & (abs(x) < .130) & (z > 1.29))
    stripes |= ((z > 1.20) & (z < 1.247)) | ((z > 1.355) & (z < 1.402))
    stripe_border = ((abs(x) > .079) & (abs(x) < .135) & (z > 1.287))
    stripe_border |= ((z > 1.195) & (z < 1.252)) | ((z > 1.350) & (z < 1.407))
    vest[stripe_border] = (.23, .27, .22)
    vest[stripes] = PALETTE['silver']
    rear=vest.copy()
    rear[(z < 1.088)|(z > 1.67)]=PALETTE['navy']
    # Open V above the zipper, binding, hem and subtle lower pocket seams.
    neckline = z > 1.535 + np.minimum(abs(x), .16) * 1.10
    vest[neckline] = PALETTE['navy']
    vest[(abs(x) < .012) & ~neckline] = (.055, .068, .070)
    vest[(abs(x) < .003) & ~neckline] = (.40, .43, .40)
    vest[z < 1.088] = PALETTE['navy']
    pockets = (abs(x) > .042) & (abs(x) < .145) & (z > 1.130) & (z < 1.136)
    vest[pockets] = (.30, .37, .07)
    # Small neutral ID badge with legible-at-close-range graphic lines.
    badge = (x > .037) & (x < .080) & (z > 1.438) & (z < 1.500)
    vest[badge] = (.83, .85, .82)
    vest[badge & (z > 1.480)] = (.06, .15, .25)
    for row in (1.453, 1.463, 1.473):
        vest[badge & (x > .044) & (x < .073) & (abs(z-row) < .001)] = (.20, .25, .28)
    pixels[:, 1024:1280, :3] = vest * fabric[:, :, None]
    pixels[:, 1280:1536, :3] = rear * fabric[:, :, None]
    for i, color in enumerate(PALETTE.values()):
        row, col = i // 4, i % 4
        pixels[row*512:(row+1)*512, 1536+col*128:1536+(col+1)*128, :3] = color
    # A separate forehead patch, away from the centre sampled by the arms.
    pixels[350:490,1664:1792,:3] = (.235,.170,.130)
    rng=np.random.default_rng(714)
    hair_noise=rng.uniform(.78,1.23,(512,128,1))
    pixels[:512,1792:1920,:3] *= hair_noise
    lit = np.zeros_like(pixels)
    lit[:, :, 3] = 1
    # Low yellow-green visibility on the vest fabric, brighter silver strips.
    # Match the final front/rear artwork so the neckline, zip and ID stay dark.
    for uniform, start in ((vest,1024),(rear,1280)):
        night=np.zeros_like(uniform)
        fabric_mask=np.all(np.isclose(uniform,(.77,.92,.065)),axis=2)
        strip_mask=np.all(np.isclose(uniform,PALETTE['silver']),axis=2)
        night[fabric_mask]=(.085,.105,.006)
        night[strip_mask]=(.36,.39,.37)
        lit[:,start:start+256,:3]=night* fabric[:,:,None]
    n = list(PALETTE).index('green')
    row, col = n // 4, n % 4
    lit[row*512:(row+1)*512, 1536+col*128:1536+(col+1)*128, :3] = PALETTE['green']
    n=list(PALETTE).index('orange');row,col=n//4,n%4
    lit[row*512:(row+1)*512,1536+col*128:1536+(col+1)*128,:3]=(.90,.13,.012)
    for name, path, data in [('airport_uniform', diffuse_path, pixels),
                              ('airport_uniform_lit', lit_path, lit)]:
        image = bpy.data.images.new(name, width=2048, height=1024, alpha=True)
        image.pixels.foreach_set(data.ravel())
        image.filepath_raw = path
        image.file_format = 'PNG'
        image.save()
        if name == 'airport_uniform':
            atlas = image
    return atlas


def atlas_material(atlas):
    mat = bpy.data.materials.new('Airport ramp uniform')
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    tex = nodes.new('ShaderNodeTexImage'); tex.image = atlas
    mat.node_tree.links.new(tex.outputs['Color'], nodes.get('Principled BSDF').inputs['Base Color'])
    nodes.get('Principled BSDF').inputs['Roughness'].default_value = .78
    night=nodes.new('ShaderNodeTexImage')
    night.name='Night vest and wand illumination'
    night.image=bpy.data.images['airport_uniform_lit']
    mat.node_tree.links.new(night.outputs['Color'],nodes.get('Principled BSDF').inputs['Emission Color'])
    nodes.get('Principled BSDF').inputs['Emission Strength'].default_value=1.0
    nodes.active=tex
    return mat


def assign_swatch(obj, name, material):
    obj.data.materials.clear(); obj.data.materials.append(material)
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name='UVMap')
    for loop in uv.data:
        loop.uv = swatch_uv(name)
    for face in obj.data.polygons:
        face.use_smooth = True


def dress_worker(worker, atlas):
    mesh = worker.data
    source_pixels=np.empty(2048*1024*4,dtype=np.float32)
    atlas.pixels.foreach_get(source_pixels)
    source_pixels=source_pixels.reshape(1024,2048,4)
    # Completely remove the hard hat and the old sleeves/hands. Fresh exposed
    # arms and gripping hands are separate anatomical meshes, not wand ends.
    bm = bmesh.new(); bm.from_mesh(mesh)
    original_uv=bm.loops.layers.uv.active
    helmet=[]
    for face in bm.faces:
        if face.calc_center_median().z < 179:
            continue
        uvcentre=sum((loop[original_uv].uv for loop in face.loops),Vector((0,0)))/len(face.loops)
        r,g,b=source_pixels[min(1023,int(uvcentre.y*1024)),min(1023,int(uvcentre.x*1024)),:3]
        if b < g*.45 and g > .10 and r < g*1.6:
            helmet.append(face)
    bmesh.ops.delete(bm,geom=helmet,context='FACES')
    for cut, normal in ((42.0,(1,0,0)),(-42.0,(-1,0,0))):
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                              plane_co=(cut, 0, 0), plane_no=normal,
                              clear_outer=True, dist=.00001)
    bmesh.ops.bisect_plane(bm, geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                          plane_co=(0,0,184.0),plane_no=(0,0,1),
                          clear_outer=True,dist=.00001)
    # Clean uniform boundaries instead of assigning whole crossing triangles.
    for cut in (-25.5,25.5):
        bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                              plane_co=(cut,0,0),plane_no=(1,0,0),dist=.00001)
    for cut in (10.5,106.5,166.5):
        bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),
                              plane_co=(0,0,cut),plane_no=(0,0,1),dist=.00001)
    bm.to_mesh(mesh); bm.free(); mesh.update()
    rest = [v.co.copy() for v in mesh.vertices]
    uv = mesh.uv_layers.active
    for v in mesh.vertices:
        x, y, z = v.co
        ax = abs(x)
        # Tuck the construction jacket's lapels into a neat polo neckline.
        if ax < 22 and 160 < z < 174 and y < -2:
            v.co.y = max(y, -5.2 + (z-160)*.23)
    mesh.update()
    material = atlas_material(atlas)
    for face in mesh.polygons:
        center = sum((rest[i] for i in face.vertices), Vector())/len(face.vertices)
        x, y, z = center / 100
        ax = abs(x)
        sample=sum((uv.data[i].uv for i in face.loop_indices),Vector((0,0)))/len(face.loop_indices)
        r,g,b=source_pixels[min(1023,int(sample.y*1024)),min(1023,int(sample.x*1024)),:3]
        construction_orange = r > g*1.8 and r > .35
        helmet_yellow = b < g*.40 and g > .13 and r < g*1.5
        if z > 1.665 and ax < .15:
            region = 'navy' if construction_orange else 'original'
            if z < 1.74 and r > g*1.5 and r > .25:
                region = 'navy'
            if helmet_yellow:
                region = 'skin'
        elif ax > .255 and z > 1.39:
            region = 'navy'
        elif z < .105:
            region = 'navy' if construction_orange else 'original'
        elif z < 1.065:
            region = 'navy'
        else:
            region = 'vest'
        for loop_index in face.loop_indices:
            if region == 'original':
                uv.data[loop_index].uv.x *= .5
            elif region == 'vest':
                co = rest[mesh.loops[loop_index].vertex_index]/100
                base=.5 if y < .04 else .625
                uv.data[loop_index].uv = (base + min(.997, max(.003,co.x/.6+.5))*.125,
                    min(.997,max(.003,(co.z-1.06)/.68)))
            else:
                uv.data[loop_index].uv = swatch_uv(region)
        face.material_index = 0
        face.use_smooth = True
    mesh.materials.clear(); mesh.materials.append(material)
    for side,sign in [('L',1),('R',-1)]:
        # Match the separate exposed arm exactly at the sleeve opening.
        ids=[v.index for v in mesh.vertices if 36 < v.co.x*sign < 42.01 and v.co.z>145]
        for group in worker.vertex_groups:
            group.remove(ids)
        worker.vertex_groups[f'CC_Base_{side}_Upperarm'].add(ids,1,'REPLACE')
    return material


def make_mesh(name, vertices, faces, material, swatch):
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces);mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(mesh);bm.free()
    obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
    assign_swatch(obj,swatch,material)
    return obj


def bare_head(material):
    """New scalp geometry: no portion of the original hard hat is reused."""
    vertices=[];faces=[];segments=24
    rings=[(1.840,.089,.111),(1.853,.090,.112),(1.873,.090,.111),
           (1.886,.087,.106),(1.910,.075,.090),(1.930,.052,.062),
           (1.941,.025,.029)]
    for row,(z,rx,ry) in enumerate(rings):
        for i in range(segments):
            a=2*math.pi*i/segments
            hairline=1.847 + .026*max(0,-math.sin(a))**3
            height=(1.840+(hairline-1.840)*.45) if row==1 else hairline if row==2 else z
            vertices.append((rx*math.cos(a),.008+ry*math.sin(a),height))
    for row in range(len(rings)-1):
        for i in range(segments):
            a=row*segments+i;b=row*segments+(i+1)%segments
            faces.append((a,b,b+segments,a+segments))
    vertices.append((0,.008,1.946))
    for i in range(segments):
        faces.append(((len(rings)-1)*segments+i,
                      (len(rings)-1)*segments+(i+1)%segments,len(vertices)-1))
    obj=make_mesh('Bare head with short cropped hair',vertices,faces,material,'hair')
    uv=obj.data.uv_layers.active
    for face in obj.data.polygons:
        for loop in face.loop_indices:
            co=obj.data.vertices[obj.data.loops[loop].vertex_index].co
            if face.index < 2*segments:
                uv.data[loop].uv=(.84375,.410)
            else:
                uv.data[loop].uv=(.877+(.5+co.x/.19)*.056,
                                  .02+(co.z-1.84)/.11*.44)
    return obj


def bare_arms(worker, armature, material):
    objects=[];segments=16
    sections=[(38,6.3,6.6),(40.5,6.8,7.0),(43.5,6.8,7.0),(47,6.0,6.3),(51.3,5.1,5.3),
              (54.0,5.5,5.7),(58.0,5.6,5.9),(63,4.9,5.2),(68,4.1,4.4),
              (73,3.3,3.7),(77,2.8,3.1)]
    for side,sign in [('L',1),('R',-1)]:
        vertices=[];faces=[]
        for x,ry,rz in sections:
            for i in range(segments):
                a=2*math.pi*i/segments
                vertices.append((sign*x,6.4+ry*math.cos(a),161.5+rz*math.sin(a)))
        for row in range(len(sections)-1):
            for i in range(segments):
                a=row*segments+i;b=row*segments+(i+1)%segments
                faces.append((a,b,b+segments,a+segments))
        obj=make_mesh(f'{side} full bare arm and wrist',vertices,faces,material,'skin')
        obj.matrix_world=worker.matrix_world.copy()
        upper=obj.vertex_groups.new(name=f'CC_Base_{side}_Upperarm')
        lower=obj.vertex_groups.new(name=f'CC_Base_{side}_Forearm')
        for row,(x,_,_) in enumerate(sections):
            t=max(0,min(1,(x-48.5)/5.5));ids=list(range(row*segments,(row+1)*segments))
            if t<1:upper.add(ids,1-t,'REPLACE')
            if t>0:lower.add(ids,t,'REPLACE')
        mod=obj.modifiers.new('Armature','ARMATURE');mod.object=armature
        objects.append(obj)
    return objects


def grip_hand(side, wrist, forearm_direction, wand_direction, material):
    """Palm, knuckles, four curled fingers and opposing thumb around a handle.

    Returns hand pieces and the handle centre. All dimensions are in metres.
    """
    d=Vector(wand_direction).normalized()
    forward=Vector((0,-1,0))
    across=d.cross(forward).normalized()
    wrist=Vector(wrist)
    centre=wrist+Vector(forearm_direction).normalized()*.068
    objects=[]
    def ellipsoid(name, pos, scale):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,location=pos)
        o=bpy.context.object;o.name=f'{side} {name}'
        from mathutils import Matrix
        o.rotation_euler=Matrix((d,forward,across)).transposed().to_euler()
        o.scale=scale;assign_swatch(o,'skin',material);objects.append(o)
        return o
    def tube(name, points, radius):
        vs=[];fs=[];sides=8
        for i,p in enumerate(points):
            tangent=Vector(points[min(i+1,len(points)-1)])-Vector(points[max(i-1,0)])
            tangent.normalize();u=tangent.cross(d)
            if u.length<.01:u=tangent.cross(forward)
            u.normalize();v=tangent.cross(u).normalized()
            for n in range(sides):
                a=2*math.pi*n/sides
                vs.append(Vector(p)+radius*(math.cos(a)*u+math.sin(a)*v))
        for i in range(len(points)-1):
            for n in range(sides):
                a=i*sides+n;b=i*sides+(n+1)%sides;fs.append((a,b,b+sides,a+sides))
        fs.append(tuple(range(sides-1,-1,-1)))
        fs.append(tuple((len(points)-1)*sides+n for n in range(sides)))
        o=make_mesh(f'{side} {name}',vs,fs,material,'skin');objects.append(o)
    # Rounded palm sits behind the handle. A distinct wrist joins the forearm.
    ellipsoid('wrist',wrist+Vector(forearm_direction)*.013,(.031,.027,.030))
    ellipsoid('palm',centre-forward*.025,(.052,.024,.037))
    handle_centre=centre+forward*.004
    for i,along in enumerate((-.029,-.009,.011,.031)):
        r=.024 if i in (1,2) else .022
        path=[]
        for degrees in (-150,-100,-50,0,50,95):
            angle=math.radians(degrees)
            path.append(handle_centre+d*along+r*(math.cos(angle)*forward+math.sin(angle)*across))
        tube(f'curled finger {i+1}',path,.010 if i<3 else .009)
    # Thumb opposes the curled fingers and crosses the near side of the grip.
    thumb=[centre-d*.041-forward*.021-across*.020,
           centre-d*.034+forward*.004-across*.038,
           centre-d*.011+forward*.026-across*.029,
           centre+d*.006+forward*.031-across*.015]
    tube('opposing thumb',thumb,.012)
    ellipsoid('thumb tip',thumb[-1],(.012,.012,.012))
    return objects, handle_centre


def hearing_protection(material):
    result = []
    for side in (-1,1):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=6,
            location=(side*.100,-.008,1.804), scale=(.010,.009,.009))
        obj=bpy.context.object; obj.name=f'Foam earplug {side}'
        assign_swatch(obj,'plug',material); result.append(obj)
    # Cord stays behind the neck with a short visible drop below each ear.
    points=[(-.104,-.006,1.801),(-.106,.020,1.755),(-.084,.080,1.715),
            (0,.106,1.704),(.084,.080,1.715),(.106,.020,1.755),(.104,-.006,1.801)]
    for i,(a,b) in enumerate(zip(points,points[1:])):
        delta=Vector(b)-Vector(a)
        bpy.ops.mesh.primitive_cylinder_add(vertices=6,radius=.0022,depth=delta.length,
            location=(Vector(a)+Vector(b))*.5)
        obj=bpy.context.object;obj.name=f'Earplug cord {i}'
        obj.rotation_euler=delta.to_track_quat('Z','Y').to_euler()
        assign_swatch(obj,'navy',material);result.append(obj)
    return result
