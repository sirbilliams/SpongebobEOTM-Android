/*  Headless engine check: load every game .omt through OMT and print fingerprints of what the
    engine decoded (canvas pixels, mesh geometry/UVs/material links, material colours, sound PCM)
    plus OMT's trig tables. Run on x86-64 and on ARM64 (qemu) and diff the output.
    usage: OMT_DATA=<game>/Program_Executable_Files omt_datatest            */

#include "OMediaApplication.h"
#include "OMediaDataBase.h"
#include "OMediaFileStream.h"
#include "OMediaFilePath.h"
#include "OMediaCanvas.h"
#include "OMedia3DShape.h"
#include "OMedia3DMaterial.h"
#include "OMedia3DPolygon.h"
#include "OMediaSound.h"
#include "OMediaTrigo.h"
#include "OMediaError.h"
#include "OMediaSDLRetarget.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

static unsigned long long fnv(unsigned long long h, const void *p, size_t n)
{
	const unsigned char *b = (const unsigned char*)p;
	for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
	return h;
}
static unsigned long long fnvf(unsigned long long h, float f)
{
	int q = (int)(f * 64.0f);          // quantise: tolerate last-bit float differences
	return fnv(h, &q, sizeof(q));
}

void omt_main(void)
{
	OMediaDataBase::set_cache_size(0);
	omd_REGISTERCLASS(OMediaCanvas);
	omd_REGISTERCLASS(OMedia3DShape);
	omd_REGISTERCLASS(OMedia3DMaterial);
	omd_REGISTERCLASS(OMediaSound);

	// Trig tables
	unsigned long long th = 14695981039346656037ULL;
	for (int a = -70000; a < 70000; a += 7) { th = fnvf(th, omd_Cos(a) * 1000); th = fnvf(th, omd_Sin(a) * 1000); }
	for (int i = -1000; i <= 1000; i++) { int as = omd_ASin(i / 1000.0f), ac = omd_ATan2(i / 1000.0f, 0.5f); th = fnv(th, &as, 4); th = fnv(th, &ac, 4); }
	printf("trig %016llx\n", th);

	vector<string> files;
	DIR *d = opendir("omt");
	struct dirent *e;
	while (d && (e = readdir(d))) if (strstr(e->d_name, ".omt")) files.push_back(e->d_name);
	if (d) closedir(d);
	sort(files.begin(), files.end());

	int total_fail = 0;
	for (size_t fi = 0; fi < files.size(); fi++)
	{
		string fn = "omt/" + files[fi];
		try
		{
			OMediaFilePath path(fn);
			OMediaFileStream f; f.setpath(&path); f.open(omcfp_Read);
			OMediaDataBase *db = new OMediaDataBase(&f);
			omt_DBIterator it;
			unsigned long long hc = 14695981039346656037ULL, hs = hc, hm = hc, hw = hc;
			int nc = 0, ns = 0, nm = 0, nw = 0;

			for (OMediaDBObject *o = db->get_first_object(OMediaCanvas::db_type, it); o; o = db->get_next_object(it))
			{
				OMediaCanvas *c = (OMediaCanvas*)o;
				c->lock(omlf_Read);
				int w = c->get_width(), h = c->get_height();
				hc = fnv(hc, &w, 4); hc = fnv(hc, &h, 4);
				if (c->get_pixels()) hc = fnv(hc, c->get_pixels(), w * h * 4);
				if (getenv("OMT_TEST_VERBOSE"))
				{
					string cn; c->get_chunk_name(cn);
					unsigned long long one = c->get_pixels() ? fnv(14695981039346656037ULL, c->get_pixels(), w * h * 4) : 0;
					printf("  canvas %-24s %4dx%-4d %016llx\n", cn.c_str(), w, h, one);
					const char *dn = getenv("OMT_TEST_DUMP");
					if (dn && cn == dn && c->get_pixels())
					{ FILE *df = fopen("dump.rgba", "wb"); fwrite(c->get_pixels(), 1, w * h * 4, df); fclose(df); }
				}
				c->unlock();
				nc++;
			}
			for (OMediaDBObject *o = db->get_first_object(OMedia3DShape::db_type, it); o; o = db->get_next_object(it))
			{
				OMedia3DShape *s = (OMedia3DShape*)o;
				omt_VertexList *vl = s->get_vertices();
				for (omt_VertexList::iterator v = vl->begin(); v != vl->end(); v++)
				{ hs = fnvf(hs, (*v).x); hs = fnvf(hs, (*v).y); hs = fnvf(hs, (*v).z); }
				omt_PolygonList *pl = s->get_polygons();
				for (omt_PolygonList::iterator p = pl->begin(); p != pl->end(); p++)
				{
					omt_3DPolygonVertexList &pv = (*p).get_vertices();
					for (omt_3DPolygonVertexList::iterator q = pv.begin(); q != pv.end(); q++)
					{ int vi = (*q).vertex_index; hs = fnv(hs, &vi, 4); hs = fnvf(hs, (*q).u); hs = fnvf(hs, (*q).v); }
					string mn; if ((*p).get_material()) (*p).get_material()->get_chunk_name(mn);
					hs = fnv(hs, mn.c_str(), mn.size());
				}
				ns++;
			}
			for (OMediaDBObject *o = db->get_first_object(OMedia3DMaterial::db_type, it); o; o = db->get_next_object(it))
			{
				OMedia3DMaterial *m = (OMedia3DMaterial*)o;
				OMediaFARGBColor a; m->get_ambient(a);
				hm = fnvf(hm, a.red); hm = fnvf(hm, a.green); hm = fnvf(hm, a.blue);
				string tn; if (m->get_texture()) m->get_texture()->get_chunk_name(tn);
				hm = fnv(hm, tn.c_str(), tn.size());
				nm++;
			}
			for (OMediaDBObject *o = db->get_first_object(OMediaSound::db_type, it); o; o = db->get_next_object(it))
			{
				OMediaSDLRtgSound *r = (OMediaSDLRtgSound*)((OMediaSound*)o)->get_retarget();
				if (r->pcm) hw = fnv(hw, r->pcm, r->pcm_length);
				int fr = r->freq, ch = r->channels; hw = fnv(hw, &fr, 4); hw = fnv(hw, &ch, 4);
				nw++;
			}
			printf("%-28s canv %4d %016llx  shape %4d %016llx  mat %4d %016llx  wave %3d %016llx\n",
				   files[fi].c_str(), nc, hc, ns, hs, nm, hm, nw, hw);
			delete db;
		}
		catch (OMediaError &err)
		{
			printf("%-28s ERROR %d at %s:%d\n", files[fi].c_str(), (int)err.errorcode, err.file.c_str(), err.line);
			total_fail++;
		}
		fflush(stdout);
	}
	printf("archives %d failed %d\n", (int)files.size(), total_fail);
}
