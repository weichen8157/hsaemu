#ifndef __HSA_PROFILE_H__
#define __HSA_PROFILE_H__

typedef struct _hsa_cpu_cache_profile
{
	unsigned int cache;/* cache access          */
	unsigned int cache_miss;/* cache miss       */
}hsa_cpu_cache_t;

typedef struct _hsa_profile
{
	unsigned int ins;  /* total instruction     */

	unsigned int barr; /* barrier instruction   */
	unsigned int atom; /* atomic instruction    */
	unsigned int sfu;  /* special function unit */

	unsigned int gst;  /* global store          */
	unsigned int gld;  /* global load           */
	unsigned int lst;  /* group store           */
	unsigned int lld;  /* group load            */

	unsigned int tlb;  /* tlb miss              */
	unsigned int page; /* page fault            */

	unsigned int cache;/* cache access          */
	unsigned int cache_miss;/* cache miss       */
}hsa_profile_t;

#define HSA_PROFILE_INITIALIZER {0,0,0,0,0,0,0,0,0,0,0,0}
#define HSA_CPU_CACHE_PROFILE_INITIALIZER {0,0}

void hsa_show_prof(hsa_profile_t *p);
void hsa_clear_prof(hsa_profile_t *p);
void hsa_update_prof(hsa_profile_t *dst, hsa_profile_t *src);

#endif

