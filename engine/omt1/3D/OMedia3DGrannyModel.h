/*  port: the engine-side API for a Granny character of sb2.exe (OMT.dll has no such class; sb2.exe's own
    character class, vtable 0x44d83c, does all of this inline). It wraps what that class asks of the engine and
    of game/granny (granny_compat): load a model (.grn + the .omt next to it with its materials), play an
    animation, and each frame copy the skinned pose into an OMedia3DShape that an OMedia3DShapeElement shows in
    an OMedia3DWorld. The mapping to the exe's functions is in docs/engine-omt1-3d.md.                        */
#pragma once
#ifndef OMEDIA1_3DGrannyModel_H
#define OMEDIA1_3DGrannyModel_H

#include "OMedia3DElement.h"
#include "OMedia3DMaterial.h"
#include <string>
#include <vector>

class OMediaDataBase;
class OMediaFileStream;

class OMedia3DGrannyModel
{
	public:
	// once per process (sb2.exe 0x4a... start-up: GrannyOpenVersion("1.2b"...), GrannyAllowTextureFormat(4)). game_dir
	// holds the player's granny.dll, whose tables the texture decoder reads (the textures themselves come from the .omt).
	static bool init(const std::string &game_dir);
	static void advance_time(double seconds);			// GrannyAdvanceTime (0x405620: once per frame, elapsed ms / 1000)

	OMedia3DGrannyModel();
	~OMedia3DGrannyModel();

	// 0x403810: open "<base>.grn" and "<base>.omt" (any letter case), move the model into the engine's axes
	// (GrannyGetToolCoordinateSystemForModel / RecommendTransform / TransformModel), open the sequence,
	// 0x403e10: bind the textures to the .omt materials, 0x4039c0: build the shape (BuildShape).
	bool load(const std::string &base_path);
	// 0x4048xx / 0x4033e0..: open an animation file (same transform) and start it as the playing control.
	// speed: param 2, loops: param 3 (9999 = endless), blend_in: param 6; the previous control blends out.
	bool play(const std::string &anim_path, float speed = 1.0f, int loops = 9999, float blend_in = 0.0f);
	// 0x405620 (per frame, after OMedia3DElement::update): the skinned pose into the shape (x = granny y, y = z,
	// z = x), scaled, polygon normals.
	void update_pose(void);

	// port: has the playing animation run to its end (loop count reached)? true when nothing plays. Drives the script's
	// "play animation and wait" and the return to the idle animation.
	bool animation_expired(void) const;

	// sb2.exe 0x433780 / 0x433730 (character vt+0x12c / +0x130): this model's sequence hangs at its bone `bone` from
	// `parent_bone` of the parent's sequence (GrannyGetBoneIndexFromName, GrannyAttachSequence with 0x1ff, 0x1ff); false: a
	// bone is missing or Granny refused.  detach: GrannyDetachSequence.
	bool attach_to(OMedia3DGrannyModel *parent, const char *bone, const char *parent_bone);
	void detach(void);
	inline OMedia3DShape *get_shape(void) {return shape;}
	inline float get_scale(void) const {return scale;}
	inline void set_scale(float s) {scale = s;}
	inline bool is_loaded(void) const {return loaded;}
	inline const std::string &get_error(void) const {return error;}

	protected:
	void build_shape(void);
	void bind_textures(const std::string &grn_path);
	void clear_control(void);

	unsigned int	model[2], sequence[2];
	unsigned int	control[3];
	bool			have_control, loaded;
	std::vector<unsigned int>	anims;			// opened animation handles (2 words each), closed at the end
	OMedia3DShape	*shape;
	float			scale;
	std::string		error;
	OMediaFileStream	*db_file;
	OMediaDataBase	*db;
};

#endif
