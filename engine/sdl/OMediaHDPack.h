/*  port: HD texture pack (docs/hd-pack.md) - not part of OMT 2.5.

    A pack is a folder hd/ in the game folder holding upscaled copies of the game's own textures under the
    names neg_hdtool / the texture extraction gives them:
        hd/omt/<database>/<id>.png   a canvas of omt/<database>.omt (lower case, no extension)
        hd/png/<name>.png            a loose texture of png/
        hd/grn/<model>_<n>.png       texture n of grn/<model>.grn (order of GrannyLockNextNewTexture)
    The folder is scanned once.  A canvas loaded from one of those sources is tagged with its pack name;
    when its GL texture is made, the HD picture (an integer multiple of the canvas size, same aspect) is
    uploaded instead of the canvas pixels.  The canvas itself keeps its size and pixels, so everything the
    game measures stays the original.  A canvas the game draws into loses its tag (and its HD texture).
    Without an hd/ folder nothing changes.                                                               */

#ifndef OMEDIA_HDPACK_H
#define OMEDIA_HDPACK_H

#include <string>

class OMediaCanvas;
class OMediaStream;
class OMediaStreamOperators;

namespace omhd
{
	// Tags the canvas with its pack name ("omt/level1/1234.png") when the pack has that file.
	void tag(OMediaCanvas *c, const std::string &rel);
	// Tags a canvas read from a loose file of png/ ("png/<name>.png") - stream is the file it was read from.
	void tag_file(OMediaCanvas *c, OMediaStreamOperators *stream);
	// The pack folder of a database read from a file: the file name, lower case, without ".omt".
	std::string db_name(OMediaStream *stream);

	// The HD picture for a tagged canvas, or NULL (not tagged, unreadable, wrong size or over the
	// limits; logged once).  'scale' gets the integer factor.  The caller deletes the canvas.
	// max_size is GL_MAX_TEXTURE_SIZE: a picture larger than that is resampled to the largest integer
	// factor that fits, or skipped.
	OMediaCanvas *load(OMediaCanvas *orig, int max_size, int &scale);

	// GPU memory accounting of the uploaded HD textures (bytes); add() returns false over the budget.
	bool add_bytes(OMediaCanvas *orig, double bytes);
	void sub_bytes(double bytes);
}

#endif
