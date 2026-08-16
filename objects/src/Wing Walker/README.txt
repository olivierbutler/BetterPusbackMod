Wing Walker Source
==================

The original Jungle Jim FBX and textures are retained under original/.
Attribution and license details are in ../../wing_walker/ATTRIBUTION.txt.

The runtime OBJ8 and texture atlases were generated with Blender 5.1.2:

  blender --background --factory-startup --python build_wing_walker.py -- \
    "original/source/Construction worker black hammer2.fbx" \
    "../../wing_walker"

The generator removes the hammer, authors the three held arm poses, adds the
marshalling wands, bakes the deformed mesh, creates the texture atlases, and
writes the X-Plane OBJ8 file. An optional third path writes pose-preview PNGs.
