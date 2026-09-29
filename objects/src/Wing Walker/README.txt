Wing Walker Source
==================

The original Jungle Jim FBX and textures are retained under original/.
Attribution and license details are in ../../wing_walker/ATTRIBUTION.txt.

The runtime OBJ8 and texture atlases were generated with Blender 5.1.2:

  blender --background --factory-startup --python build_wing_walker.py -- \
    "original/source/Construction worker black hammer2.fbx" \
    "../../wing_walker"

The generator and airport_uniform.py create the airport employee variant:
- No hat: the original hard hat, including its brim, is deleted. A new scalp
  with short hair replaces it; blue corded earplugs remain visible.
- A lime safety vest with silver bands, navy short-sleeved shirt and trousers.
- New skinned bare arms and wrists, with separate palms, four curled fingers
  and opposing thumbs gripping black handles on the marshalling wands.
- STOP: forearms cross above the head, with depth separation at the crossing.
- STANDBY: both orange wands point outward and down at exactly 45 degrees.
- CLEAR: the existing raised green wand and final-clear light are retained.

An optional third path writes front/detail preview PNGs and a packed editable
airport-wing-walker.blend. The blend opens in standby with held pose keys at
frames 1 (stop), 40 (standby) and 80 (clear). Each pose has matching grips and
wands, including the upright green clear wand at frame 80. The materials
include the night texture for rendered previews. The runtime generator
independently bakes these three held poses. Python and NumPy bundled with Blender
are sufficient; no additional model downloads are required.

Runtime: 9,199 triangles per held pose, three pose ranges, a 2048x1024 diffuse
atlas and matching lit atlas. Identical vertex/normal/UV records are shared.
The standing worker is about 1.83 m tall. Orange and green wand tubes emit
light through the lit atlas. The vest fabric is softly lit yellow-green and
its silver strips are brighter on both front and rear. Skin, navy clothing,
ID badge and black wand handles do not self-illuminate.

The existing bp/anim/wing_walker_signal values are unchanged: 0 hidden,
1 stop, 2 standby, 3 clear. X-Plane switches held poses, as before; this OBJ
does not add smooth arm transitions to the plugin.

Validate from the repository root with:
  python tests/wing_walker_asset_test.py

Rendered previews and asset checks do not replace an in-simulator check.
