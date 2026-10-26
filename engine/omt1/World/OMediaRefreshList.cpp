/*  OMT 1.x engine layer (OMT.dll, Aug 2002), rebuilt from the retail DLL's code. LGPL like OMT. */
#include "OMediaRefreshList.h"

OMediaRefreshList::OMediaRefreshList() {}
OMediaRefreshList::~OMediaRefreshList() {}

static inline bool omf_overlap(const OMediaShortRect &a, const OMediaShortRect &b)
{
	return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

static inline void omf_union(OMediaShortRect &d, const OMediaShortRect &s)
{
	if (s.left < d.left) d.left = s.left;
	if (s.top < d.top) d.top = s.top;
	if (s.bottom > d.bottom) d.bottom = s.bottom;
	if (s.right > d.right) d.right = s.right;
}

// 0x10084be0. A non-empty rectangle is merged into the first one it overlaps, then that one is
// merged into any other it now overlaps (the merged one removed), until one rectangle is left
// or none overlaps; otherwise it is appended.
void OMediaRefreshList::newrect(OMediaShortRect *r)
{
	if (!(r->left < r->right && r->top < r->bottom)) return;
	for (omt_ShortRectList::iterator i = rects.begin(); i != rects.end(); i++)
	{
		if (!omf_overlap(*r, *i)) continue;
		omf_union(*i, *r);
		omt_ShortRectList::iterator cur = i;
		for (;;)
		{
			omt_ShortRectList::iterator j;
			for (j = rects.begin(); j != rects.end(); j++)
				if (j != cur && omf_overlap(*cur, *j)) break;
			if (j == rects.end()) return;
			omf_union(*j, *cur);
			rects.erase(cur);
			cur = j;
			if (rects.size() == 1) return;
		}
	}
	rects.push_back(*r);
}

// 0x10084d70
bool OMediaRefreshList::touchrect(OMediaShortRect *r)
{
	for (omt_ShortRectList::iterator i = rects.begin(); i != rects.end(); i++)
		if ((*i).left < r->right && r->left < (*i).right && (*i).top < r->bottom && r->top < (*i).bottom) return true;
	return false;
}

// 0x10084dc0: this list becomes 'src' clipped to 'clip'
void OMediaRefreshList::clipped_copy(OMediaRefreshList *src, OMediaShortRect *clip)
{
	rects.clear();
	for (omt_ShortRectList::iterator i = src->rects.begin(); i != src->rects.end(); i++)
	{
		const OMediaShortRect &s = *i;
		if (clip->top < s.bottom && clip->left < s.right && s.top < clip->bottom && s.left < clip->right)
		{
			OMediaShortRect c;
			c.left = s.left < clip->left ? clip->left : s.left;
			c.top = s.top < clip->top ? clip->top : s.top;
			c.right = s.right < clip->right ? s.right : clip->right;
			c.bottom = s.bottom < clip->bottom ? s.bottom : clip->bottom;
			rects.push_back(c);
		}
	}
}

// 0x10084f00: appends (without merging)
void OMediaRefreshList::copy_or(OMediaRefreshList *src)
{
	for (omt_ShortRectList::iterator i = src->rects.begin(); i != src->rects.end(); i++) rects.push_back(*i);
}

// 0x10084f70
void OMediaRefreshList::copy_or_offset(OMediaRefreshList *src, short dx, short dy)
{
	for (omt_ShortRectList::iterator i = src->rects.begin(); i != src->rects.end(); i++)
	{
		OMediaShortRect r = *i;
		r.left += dx; r.right += dx; r.top += dy; r.bottom += dy;
		rects.push_back(r);
	}
}
