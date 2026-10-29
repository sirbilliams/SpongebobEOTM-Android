# OMT 1.x 3D path and Granny characters (engine/omt1/3D), lane a

Rebuilt from OMT.dll (addresses in the headers). Test: `omt1_3d_test <game folder> [scene] [shot folder]`
(headless; loads the scene with the scene loader, its `sceneart/<scene>.omt` background, puts every CHAR record's
model at its position with its idle animation, renders frames 0/30/60 -> `frame-1/31/61.bmp`, the numbers are
presents). ASan/UBSan clean for the new code (only the known shared `OMediaRLEPacker.cpp:81` misaligned load).

## Classes
| class | file | state |
|---|---|---|
| OMedia3DWorld | 3D/OMedia3DWorld | ctor 0x10036550 fields: `sort_mode` +0x7c, `focal` +0x80 (400), `far_z` +0x84 (3200), `near_z` +0x88 (10), `ambient` +0x8c (0x4444), `rasterizer` +0xa0, lights list +0x98 |
| OMedia3DVPModel | same | render 0x10033660, prepare_viewport_logic 0x10033490, transform_point 0x10033380, project_points 0x10034eb0 (camera: `inv_rotate(-ax,-ay,-az)` of `p - viewpos`, `sx = x/z*focal + w/2 + left`, `sy = -y/z*focal + h/2 + top`), normal_to_rgb_material 0x100351e0 (lighting); clipping is a near-plane clip. TODO-log: test_click 0x10035e10 |
| OMedia3DElement / ShapeElement / Light | 3D/OMedia3DElement | ctor/dtor, get_type, **update 0x10004e90**, set_angle 0x100055c0, Light ctor 0x1001c380 (kind +0xfc 0/1/2, range +0x108, falloff +0x10c, mask +0x110), Light::set_angle 0x1001c5c0, update_world_ptr 0x1001c500 (light list). The gen_* functions (0x10005ab0..0x10008320) feed the software rasterizer and are not rebuilt (`// port:`) |
| OMedia3DShape / Polygon | 3D/OMedia3DShape | 1.x polygon (0x70 bytes: 3-4 vertex + normal indices, u/v per vertex, material); shape = vertex / normal / polygon vectors, `scale` 0x1001fc40, `prepare(0x20)` = polygon normals. Other prepare steps are TODO-log |
| OMedia3DMaterial | 3D/OMedia3DMaterial | db class '3DMa' (0x33444d61), the 1.x stream (version 1, 52 bytes in `models/*/*.omt`): mode, alpha, colours, texture canvas link |
| OMedia3DNavigator | 3D/OMedia3DNavigator | find_direction_vector 0x1001db60 done; update / update_angles / set_angle (x87 angle-from-matrix code) TODO-log: sb2.exe never switches navigator mode on |
| OMediaAutoMove / AutoRotate | World/OMediaWorldPosition.cpp | already complete (checked against 0x10037af0 / 0x10037cd0) |
| OMedia3DRasterizerDesc / Manager | 3D/OMedia3DRasterizerDesc | `build()` 0x1001f380 now gives the GL rasterizer |
| OMedia3DAbstractRasterizer | 3D/OMedia3DWorld.h | vtable 0x103a9554: +4 start(gfx,x,y,w,h,far), +8 rasterize(poly), +0xc end, +0x10 process_background, +0x14 set_anti_aliasing, +0x18 set_texture_filtering |
| OMedia3DGLRasterizer | 3D/OMedia3DGLRasterizer | `// port:` the GL rasterizer: draws into the GL surface of the target world (the screen framebuffer) inside the viewport rectangle, depth cleared there, perspective-correct textures from the material's canvas, colour-keyed texels cut out, blended (alpha) polygons last and far-to-near |
| OMedia3DGrannyModel | 3D/OMedia3DGrannyModel | the engine API for a Granny character (below) |

Texture canvases ('Canv' of a model .omt, stream attributes 0x100c) go to `OMediaGfxWorldFactory::create_texture`,
which needs the current rasterizer's engine: building the rasterizer sets it (OMT.dll global 0x103ce0a4) and the
factory registers the memory pixmap world as texture world of that engine (`// port:`).

## Engine API for a character (`OMedia3DGrannyModel`)
```
OMedia3DGrannyModel::init(game_dir);                       // once (GrannyOpenVersion "1.2b", AllowTextureFormat 4)
OMedia3DGrannyModel *m = new OMedia3DGrannyModel;
m->load(dir + "/models/people/sb_model");                  // .grn + .omt, any letter case
m->play(dir + "/models/people/sb_idle1.grn", speed, 9999, blend_in);
OMedia3DShapeElement *e = new OMedia3DShapeElement;
e->setshape(m->get_shape());  e->link(world3d);  e->place(x, y, z);  e->set_angle(0, heading, 0);
// per frame: OMedia3DGrannyModel::advance_time(seconds); m->update_pose(); world3d->update(); world2d->update();
```
3D scene as sb2.exe's scene sets it up (0x4033f0): `OMedia3DWorld` (`set_framerate_mode(omfrc_Max)`,
`setbackground(NULL)` so the 2D art shows through, `focal` = camera +0x188, ambient = camera +0x18c.. bytes,
`rasterizer`), an `OMediaViewPort` on it placed at the camera position (+0x174..) with the camera angles
(`deg * 0x4000 / 360`, +0x180..), and an `OMediaVPElement` showing that viewport in the 2D world
(`OMediaVPElement::draw_full` renders the viewport into the 2D world's target).

## Which exe function calls which engine method
| sb2.exe | engine |
|---|---|
| 0x402c60 / 0x402d10 (game light class) | OMedia3DLight ctor 0x1001c380 (+ `kind = 2`), `unlink` 0x1001c4f0, dtor 0x1001c460 |
| 0x402e30 (character, a ShapeElement subclass, vtable 0x44d83c) | OMedia3DShapeElement ctor 0x100313a0, `new OMedia3DShape(1)` 0x1001f500, `setshape` 0x10031450 (`getshape` = its vt+0x9c) |
| 0x4030f0 / 0x421dd0 / 0x440c40 | OMedia3DShapeElement dtor 0x10031410 |
| 0x4033f0 (scene 3D init), 0x436f30 | OMedia3DWorld ctor; vt+0x14(1) set_framerate_mode, vt+0x24(0) setbackground; fields +0x80 +0x84 +0x88 +0x8c +0xa0; OMediaViewPort ctor, vt `place` / `set_angle`; OMediaVPElement |
| 0x403810 (LoadModel) | GrannyOpenModel / GetToolCoordinateSystemForModel / RecommendTransform / TransformModel / OpenSequence (-> `OMedia3DGrannyModel::load`) |
| 0x403e10 (BindTextures) | db `get_object('3DMa', name)` -> OMedia3DMaterial, GrannyUnlockNewTexture(cookie = material) |
| 0x4039c0 (BuildShape) | OMedia3DPolygon ctor 0x1001eec0, `set_material` 0x100321f0, dtor 0x1001ef00, `operator=` 0x1001f230 (0x4063c0), copy ctor 0x1001ee30 (0x4066f0); shape vt+0x2c `scale`, vt+0x20 `prepare(0x20)` |
| 0x405620 / 0x421e10 / 0x440c90 (per-frame update) | OMedia3DElement::update 0x10004e90; pose copy (x = granny y, y = z, z = x), `scale`, `prepare(0x20)` (-> `update_pose`); 0x4053c0 swaps a polygon material (`set_material`) |
| 0x4097d0 | registers '3DMa' with OMedia3DMaterial::db_builder 0x1001ce20 |
| 0x40ceb0 / 0x40cfe0 (Init/UnInitRasterizer) | OMedia3DRasterizerDesc::build 0x1001f380; rasterizer vt+0x14(3), vt+0x18(3); dtor |
| 0x4184f0 | OMediaMathTools::is_point_in_polygon (shared math) |
| 0x41e3d0, 0x434650, 0x435160 | OMedia3DVector::quick_magnitude (shared math) |
| 0x4234d0, 0x42f1a0, 0x433a60, 0x433f00, 0x434650 | OMedia3DVector::angles (shared math) |

## Granny coordinate system (checked on the real files)
`GrannyRecommendTransform(tool origin, tool axes, origin 0, wanted axes {-a0,0,0, 0,0,a5, 0,a7,0})` as in
`tools/granny/xftest.cpp`: X mirrored, Y/Z swapped, the tool's scale kept, so the models stay in the artists' units
(SpongeBob ~135 high, Patrick ~206) with the origin at the feet; the per-frame copy then maps (x,y,z) = (gy,gz,gx).
Polygon vertex order is (0,2,1) of the Granny triangle, as in the exe. Textures: the .grn names its bitmaps
(`...\sponge.bmp`); granny_compat does not report the 3DS material name of a new texture, so the n-th texture is
bound to the 3DMa named like the n-th bitmap (fallback: the database's first material) (`// port:`).

## Not done / open
- Lighting is floats, not the 16-bit fixed point of 0x100351e0 (and specular is not added); the scene's LITE
  record fields (`kind`, range/falloff/colour) are mapped by the test only: the exe's own light class (0x402c60,
  +0x3f = 2) is the other lane's.
- Character heading units, the scene's per-character scale (CHAR +0x23c = 50) and what the START points do are the
  game side's: the test uses CHAR +0x240 as the Y angle and +0x230..0x238 as the position; the start points of
  c1l0s1 are all zero.
- Picking (`test_click`), the shape converters, fog, sprites/tiles/particles (OMedia3DSprite, Tile, ParticleEmitter,
  HeightGrid) and the generation functions are not rebuilt.

## Installed-model regression catalogue

`omt1_models_test <game dir> <shot dir>` audits every `.grn` and renders each mesh-bearing
file through the actual engine path; `python3 tools/test/model-sheet.py <shot dir>` adds a labelled PNG sheet.
The Marlin multi-mesh UV and per-mesh material corrections, verification and decoder limitations
are documented in [marlin-model.md](marlin-model.md).
