/*  port: std::sort as MSVC 6's library implements it (introsort-like quicksort down to runs of
    16, then one insertion sort). OMT.dll sorts 2D elements with it; elements with equal keys
    keep the order this algorithm gives them, so the draw order matches the original.        */
#pragma once
#ifndef OMEDIA1_MSVCSort_H
#define OMEDIA1_MSVCSort_H

template <class T> inline const T &omt_msvc_median(const T &a, const T &b, const T &c)
{
	if (a < b) return b < c ? b : (a < c ? c : a);
	return a < c ? a : (b < c ? c : b);
}

template <class T> T *omt_msvc_unguarded_partition(T *f, T *l, T piv)
{
	for (;; ++f)
	{
		for (; *f < piv; ++f) ;
		for (; piv < *--l; ) ;
		if (l <= f) return f;
		T t = *f; *f = *l; *l = t;
	}
}

template <class T> void omt_msvc_sort_rec(T *f, T *l)
{
	while (16 < l - f)
	{
		T *m = omt_msvc_unguarded_partition(f, l, omt_msvc_median(*f, *(f + (l - f) / 2), *(l - 1)));
		if (l - m <= m - f)
		{
			omt_msvc_sort_rec(m, l);
			l = m;
		}
		else
		{
			omt_msvc_sort_rec(f, m);
			f = m;
		}
	}
}

template <class T> void omt_msvc_sort(T *f, T *l)
{
	if (f == l) return;
	omt_msvc_sort_rec(f, l);
	for (T *m = f; ++m != l; )		// _Insertion_sort
	{
		T v = *m;
		if (v < *f)
		{
			for (T *p = m; p != f; --p) *p = *(p - 1);
			*f = v;
		}
		else
		{
			T *x = m;
			for (T *y = x; v < *--y; x = y) *x = *y;
			*x = v;
		}
	}
}

#endif
