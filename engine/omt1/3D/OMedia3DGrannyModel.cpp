/*  port: see OMedia3DGrannyModel.h */
#include "OMedia3DGrannyModel.h"
#include "granny_compat.h"
#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaError.h"
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static granny_uint32 omv_version = 0;

// A path whose components may differ in letter case from the installed files (sb2.exe's names do): the real one, or "".
static std::string omf_find_case(const std::string &path)
{
	struct stat st;
	if (!stat(path.c_str(), &st)) return path;
	std::string out;
	size_t pos = 0;
	if (!path.empty() && path[0] == '/') {out = "/"; pos = 1;}
	while (pos < path.size())
	{
		size_t e = path.find('/', pos);
		if (e == std::string::npos) e = path.size();
		std::string comp = path.substr(pos, e - pos);
		pos = e + 1;
		if (comp.empty()) continue;
		std::string dir = out.empty() ? "." : out;
		DIR *d = opendir(dir.c_str());
		if (!d) return "";
		std::string hit;
		for (dirent *de = readdir(d); de; de = readdir(d))
			if (!strcasecmp(de->d_name, comp.c_str())) {hit = de->d_name; if (hit == comp) break;}
		closedir(d);
		if (hit.empty()) return "";
		out += (out.empty() || out[out.size() - 1] == '/') ? hit : "/" + hit;
	}
	return out;
}

bool OMedia3DGrannyModel::init(const std::string &game_dir)
{
	if (omv_version) return true;
	std::string dll = omf_find_case(game_dir + "/granny.dll");
	if (!dll.empty()) GrannyCompatSetDllPath(dll.c_str());
	if (GrannyOpenVersion("1.2b", "win32", "10-4-2000", "(C) Copyright 1999-2000 RAD Game Tools, Inc.  All Rights Reserved.", &omv_version))
		return false;
	GrannyAllowTextureFormat(omv_version, 4);
	return true;
}

void OMedia3DGrannyModel::advance_time(double seconds)
{
	if (omv_version) GrannyAdvanceTime(omv_version, seconds);
}

OMedia3DGrannyModel::OMedia3DGrannyModel()
{
	model[0] = model[1] = sequence[0] = sequence[1] = 0;
	control[0] = control[1] = control[2] = 0;
	have_control = loaded = false;
	shape = new OMedia3DShape;
	scale = 1.0f;
	db_file = NULL;
	db = NULL;
}

OMedia3DGrannyModel::~OMedia3DGrannyModel()
{
	if (have_control) clear_control();
	if (sequence[0]) GrannyCloseSequence(sequence[0], sequence[1]);
	for (size_t i = 0; i + 1 < anims.size(); i += 2) GrannyCloseAnimation(anims[i], anims[i + 1]);
	if (model[1]) GrannyCloseModel(model[0], model[1]);
	delete shape;
	delete db;
	delete db_file;
}

// the transform of a model or animation into the engine's axes. sb2.exe (0x403810): the file's tool system, the
// wanted one (origin 0, X mirrored, Y/Z swapped), GrannyRecommendTransform, then Transform*.
static void omf_xform(bool is_model, unsigned int h0, unsigned int h1)
{
	float to[3] = {0, 0, 0}, ta[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
	if (is_model) GrannyGetToolCoordinateSystemForModel(h0, h1, to, ta);
	else GrannyGetToolCoordinateSystemForAnimation(h0, h1, to, ta);
	// the wanted system as the exe builds it (0x403810: zeros with two of the tool's axis entries copied in) and as
	// tools/granny/xftest.cpp checked: X mirrored, Y and Z swapped, the tool's scale kept (units stay the artists')
	float wo[3] = {0, 0, 0}, wa[9] = {-ta[0], 0, 0, 0, 0, ta[5], 0, ta[7], 0};
	float T[3], M[9];
	if (GrannyRecommendTransform(omv_version, to, ta, wo, wa, T, M)) return;
	if (is_model) GrannyTransformModel(h0, h1, T, M);
	else GrannyTransformAnimation(h0, h1, T, M);
}

bool OMedia3DGrannyModel::load(const std::string &base)
{
	if (!omv_version) {error = "OMedia3DGrannyModel::init not called"; return false;}
	std::string grn = omf_find_case(base + ".grn"), omt = omf_find_case(base + ".omt");
	if (grn.empty()) {error = "no model file " + base + ".grn"; return false;}
	if (GrannyOpenModel(omv_version, grn.c_str(), model)) {error = "GrannyOpenModel failed"; return false;}
	omf_xform(true, model[0], model[1]);
	if (GrannyOpenSequence(model[0], model[1], sequence)) {error = "GrannyOpenSequence failed"; return false;}
	if (!omt.empty())
	{
		try
		{
			OMediaFilePath *path = new OMediaFilePath(omt);
			db_file = new OMediaFileStream;
			db_file->setpath(path);
			db_file->open(omcfp_Read);
			db = new OMediaDataBase(db_file);
		}
		catch (OMediaError &e) {printf("OMedia3DGrannyModel: %s: OMT error %d\n", omt.c_str(), (int)e.errorcode); db = NULL;}
	}
	bind_textures();
	build_shape();
	loaded = true;
	return true;
}

// 0x403e10 BindTextures: every new texture of the model gets the material named like it as its cookie.
void OMedia3DGrannyModel::bind_textures(void)
{
	// the textures are delivered after the first render lock
	granny_uint32 lock[2];
	if (GrannyLockSequenceForRendering(sequence[0], sequence[1], 0x3b, lock)) return;
	granny_uint32 n = 0;
	GrannyUnlockRendering(lock[0], lock[1]);
	if (GrannyGetNewTexturesLeft(omv_version, &n)) return;
	for (granny_uint32 i = 0; i < n; i++)
	{
		granny_new_texture tex;
		if (GrannyLockNextNewTexture(omv_version, &tex)) continue;
		OMedia3DMaterial *m = NULL;
		if (db)
		{
			std::string nm = GrannyCompatTextureMaterialName(&tex);
			if (!nm.empty() && db->object_exists(OMedia3DMaterial::db_type, nm))
				m = (OMedia3DMaterial*)db->get_object(OMedia3DMaterial::db_type, nm);
			if (!m)		// port: a name the file does not give -> the database's first material
			{
				omt_DBIterator it;
				m = (OMedia3DMaterial*)db->get_first_object(OMedia3DMaterial::db_type, it);
			}
		}
		GrannyUnlockNewTexture(&tex, (granny_cookie)m);
	}
}

// 0x4039c0 BuildShape: one vertex/normal block per rendering state, one triangle per Granny triangle with the
// state's material, vertices in the order (0, 2, 1) like the exe.
void OMedia3DGrannyModel::build_shape(void)
{
	std::vector<OMedia3DPoint> &vs = *shape->get_vertices();
	std::vector<OMedia3DVector> &ns = *shape->get_normals();
	std::vector<OMedia3DPolygon> &ps = *shape->get_polygons();
	vs.clear(); ns.clear(); ps.clear();
	granny_uint32 lock[2], count = 0;
	if (GrannyLockSequenceForRendering(sequence[0], sequence[1], 0x3b, lock)) return;
	if (!GrannyGetRenderingStatesLeft(lock[0], lock[1], &count))
	{
		for (granny_uint32 s = 0; s < count; s++)
		{
			granny_rendering_state st;
			if (GrannyLockNextRenderingState(lock[0], lock[1], &st)) continue;
			size_t base = vs.size();
			const float *tc = (const float*)st.texcoords.data;
			for (granny_uint32 i = 0; st.positions.present && i < st.positions.count; i++)
			{
				const float *p = (const float*)st.positions.data + i * 3;
				vs.push_back(OMedia3DPoint(p[1], p[2], p[0]));
				OMedia3DVector nv(0, 1, 0);
				if (st.normals.present && i < st.normals.count)
				{
					const float *n = (const float*)st.normals.data + i * 3;
					nv.set(n[1], n[2], n[0]);
				}
				ns.push_back(nv);
			}
			OMedia3DMaterial *mat = (OMedia3DMaterial*)st.texture_cookie;
			for (granny_uint32 t = 0; st.triangles.present && t < st.triangles.count; t++)
			{
				const unsigned short *ix = (const unsigned short*)st.triangles.data + t * 3;
				OMedia3DPolygon pl;
				pl.num_points = 3;
				static const int order[3] = {0, 2, 1};
				for (int k = 0; k < 3; k++)
				{
					unsigned short i = ix[order[k]];
					pl.vertex[k] = base + i;
					pl.normal_index[k] = base + i;
					if (st.texcoords.present && tc && i < st.texcoords.count) {pl.u[k] = tc[i * 3]; pl.v[k] = tc[i * 3 + 1];}
				}
				pl.set_material(mat);
				ps.push_back(pl);
			}
			GrannyUnlockRenderingState(&st);
		}
	}
	GrannyUnlockRendering(lock[0], lock[1]);
	shape->prepare(0x20);
	printf("SHAPEINFO: VERTS=%ld POLYS=%ld\n", (long)vs.size(), (long)ps.size());
}

void OMedia3DGrannyModel::clear_control(void)
{
	GrannyRemoveControlledAnimation(control[0], control[1], control[2]);
	GrannyReleaseControlledAnimation(control[0], control[1], control[2]);
	have_control = false;
}

bool OMedia3DGrannyModel::play(const std::string &anim_path, float speed, int loops, float blend_in)
{
	if (!loaded) return false;
	std::string f = omf_find_case(anim_path);
	if (f.empty()) {error = "no animation file " + anim_path; return false;}
	granny_uint32 a[2];
	if (GrannyOpenAnimation(omv_version, f.c_str(), a)) {error = "GrannyOpenAnimation failed"; return false;}
	omf_xform(false, a[0], a[1]);
	anims.push_back(a[0]);
	anims.push_back(a[1]);
	if (have_control) clear_control();		// port: the exe blends the old control out; here it is dropped
	if (GrannyAddControlledAnimation(sequence[0], sequence[1], a[0], a[1], control)) {error = "GrannyAddControlledAnimation failed"; return false;}
	have_control = true;
	GrannySetAnimationParameter(control[0], control[1], control[2], GrannyParamSpeed, speed);
	GrannySetAnimationParameter(control[0], control[1], control[2], GrannyParamLoopCount, loops);
	GrannySetAnimationParameter(control[0], control[1], control[2], GrannyParamBlendInTime, blend_in);
	return true;
}

bool OMedia3DGrannyModel::animation_expired(void) const
{
	if (!loaded || !have_control) return true;
	granny_uint32 expired = 0;
	if (GrannyHasControlExpired(control[0], control[1], control[2], &expired)) return true;
	return expired != 0;
}

// 0x405620's second half (the code after the Granny clock): lock the sequence for positions + normals (0x11) and
// copy them, in the exe's axis order, into the shape; scale; polygon normals.
void OMedia3DGrannyModel::update_pose(void)
{
	if (!loaded) return;
	std::vector<OMedia3DPoint> &vs = *shape->get_vertices();
	std::vector<OMedia3DVector> &ns = *shape->get_normals();
	granny_uint32 lock[2], count = 0;
	if (GrannyLockSequenceForRendering(sequence[0], sequence[1], 0x11, lock)) return;
	if (!GrannyGetRenderingStatesLeft(lock[0], lock[1], &count))
	{
		size_t vi = 0, ni = 0;
		for (granny_uint32 s = 0; s < count; s++)
		{
			granny_rendering_state st;
			if (GrannyLockNextRenderingState(lock[0], lock[1], &st)) continue;
			for (granny_uint32 i = 0; st.positions.present && i < st.positions.count && vi < vs.size(); i++, vi++)
			{
				const float *p = (const float*)st.positions.data + i * 3;
				vs[vi].x = p[1]; vs[vi].y = p[2]; vs[vi].z = p[0];
			}
			for (granny_uint32 i = 0; st.normals.present && i < st.normals.count && ni < ns.size(); i++, ni++)
			{
				const float *n = (const float*)st.normals.data + i * 3;
				ns[ni].x = n[1]; ns[ni].y = n[2]; ns[ni].z = n[0];
			}
			GrannyUnlockRenderingState(&st);
		}
	}
	GrannyUnlockRendering(lock[0], lock[1]);
	shape->scale(scale, scale, scale);
	shape->prepare(0x20);
}
