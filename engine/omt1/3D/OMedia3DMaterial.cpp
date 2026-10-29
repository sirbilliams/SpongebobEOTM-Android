/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMedia3DMaterial.h"
#include "OMediaStreamOperators.h"
#include "OMediaError.h"
#include <string.h>

// 0x1001c920
OMedia3DMaterial::OMedia3DMaterial()
{
	mode = ommm_Color;
	shaded = true;
	gouraud = false;
	no_fog = false;
	alpha = 1.0f;
	memset(emission, 0, sizeof(emission));
	memset(specular, 0, sizeof(specular));
	diffuse[0] = diffuse[1] = diffuse[2] = 1.0f;
	ambient[0] = ambient[1] = ambient[2] = 1.0f;
	layers = 0xffffffffUL;
	texture = NULL;
}

OMedia3DMaterial::~OMedia3DMaterial() {}		// 0x1001cba0

// 0x1001ce20
OMediaDBObject *OMedia3DMaterial::db_builder(void) { return new OMedia3DMaterial; }

// 0x1001d220: 0x34 bytes, +0x10 with a texture, +0x10 with a back-face material
unsigned int OMedia3DMaterial::get_approximate_size(void) { return texture ? 0x44 : 0x34; }

static void omf_read_rgb(OMediaStreamOperators &stream, float *c)
{
	unsigned short r, g, b;
	stream >> r;
	stream >> g;
	stream >> b;
	c[0] = r / 65535.0f;
	c[1] = g / 65535.0f;
	c[2] = b / 65535.0f;
}

// The retail 1.x material stream (version 1; OMT 2.5's reader has the same branch): type, alpha, no-fog, then
// per type the colours; a textured material ends in a link to the canvas.
void OMedia3DMaterial::read_class(OMediaStreamOperators &stream)
{
	OMediaDBObject::read_class(stream);
	texture = NULL;
	short version;
	stream >> version;
	if (version != 1)
	{
		printf("OMedia3DMaterial: stream version %d not supported (TODO)\n", (int)version);	// TODO: the 2.x format is not in the game's files
		return;
	}
	short mtype;
	stream >> mtype;
	if (mtype > 2) return;
	float transparency;
	stream >> transparency;
	stream >> no_fog;
	mode = mtype;
	if (mtype == 0)
	{
		omf_read_rgb(stream, diffuse);
		shaded = false;
	}
	else
	{
		omf_read_rgb(stream, emission);
		omf_read_rgb(stream, diffuse);
		omf_read_rgb(stream, specular);
		omf_read_rgb(stream, ambient);
		stream >> gouraud;
		if (mtype == 2)
		{
			stream >> shaded;
			if (!shaded) diffuse[0] = diffuse[1] = diffuse[2] = 1.0f;
			short contain;
			stream >> contain;
			if (contain == 1)
			{
				OMediaDBObjectStreamLink slink;
				stream >> slink;
				texture = (OMediaCanvas*)slink.get_object();
			}
		}
	}
	alpha = 1.0f - transparency;
}

void OMedia3DMaterial::write_class(OMediaStreamOperators &) { omd_EXCEPTION(omcerr_BadFormat); }	// port: the game never saves
