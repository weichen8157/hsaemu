#include <stdio.h>
#include <string.h>
#include "hsa_prof.h"

void hsa_show_prof(hsa_profile_t *p)
{
	float ratio = (float)p->ins;
	ratio -= (p->atom + p->gst + p->gld + p->lst + p->lld);
	ratio /= ((float)p->ins);

	fprintf(stderr, "HSA profile counter info:\n"
		"\ttotal instruction      = %u \n"
		"\tbarrier instruction    = %u \n"
		"\tatomic instruction     = %u \n"
		"\tspecial function unit  = %u \n"
		"\tglobal store           = %u \n"
		"\tglobal load            = %u \n"
		"\tgroup store            = %u \n"
		"\tgroup load             = %u \n"
		"\ttlb miss               = %u \n"
		"\tpage fault             = %u \n"
		"\tcache access           = %u \n"
		"\tcache miss             = %u \n"
		"\tcomputation/communication ratio = %f\n",
		p->ins, p->barr, p->atom, p->sfu,
		p->gst, p->gld,
		p->lst, p->lld,
		p->tlb, p->page,
		p->cache, p->cache_miss,
		ratio);
}

void hsa_clear_prof(hsa_profile_t *p)
{
	memset(p, 0, sizeof(hsa_profile_t));
}

void hsa_update_prof(hsa_profile_t *dst, hsa_profile_t *src)
{
	hsa_profile_t d = (*dst);
	hsa_profile_t s = (*src);
	d.ins  += s.ins;
	d.barr += s.barr;
	d.atom += s.atom;
	d.sfu  += s.sfu;
	d.gst  += s.gst;
	d.gld  += s.gld;
	d.lst  += s.lst;
	d.lld  += s.lld;
	d.tlb  += s.tlb; 
	d.page += s.page;
	d.cache += s.cache;
	d.cache_miss += s.cache_miss;
	(*dst) = d;
}
