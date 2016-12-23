#include "patch.h"

namespace patch { namespace ups {
//TODO: HEAVY cleanups needed here
static uint32_t read32(uint8_t * ptr)
{
	uint32_t out;
	out =ptr[0];
	out|=ptr[1]<<8;
	out|=ptr[2]<<16;
	out|=ptr[3]<<24;
	return out;
}

#define error(which) do { error=which; goto exit; } while(0)
#define assert_sum(a,b) do { if (SIZE_MAX-(a)<(b)) error(e_too_big); } while(0)
#define assert_shift(a,b) do { if (SIZE_MAX>>(b)<(a)) error(e_too_big); } while(0)
result apply(const file& patch_, const file& source_, file& target_)
{
	if (patch_.size()<4+2+12) return e_broken;
	
	struct mem patch = patch_.mmap();
	struct mem in = source_.mmap();
	struct mem out_;
	struct mem * out = &out_;
	result error;
	out->len=0;
	out->ptr=NULL;
	
	if (true)
	{
#define readpatch8() (*(patchat++))
#define readin8() (*(inat++))
#define writeout8(byte) (*(outat++)=byte)
		
#define decodeto(var) \
				do { \
					var=0; \
					unsigned int shift=0; \
					while (true) \
					{ \
						uint8_t next=readpatch8(); \
						assert_shift(next&0x7F, shift); \
						size_t addthis=(next&0x7F)<<shift; \
						assert_sum(var, addthis); \
						var+=addthis; \
						if (next&0x80) break; \
						shift+=7; \
						assert_sum(var, 1U<<shift); \
						var+=1<<shift; \
					} \
				} while(false)
		
		bool backwards=false;
		
		uint8_t * patchat=patch.ptr;
		uint8_t * patchend=patch.ptr+patch.len-12;
		
		if (readpatch8()!='U') error(e_broken);
		if (readpatch8()!='P') error(e_broken);
		if (readpatch8()!='S') error(e_broken);
		if (readpatch8()!='1') error(e_broken);
		
		size_t inlen;
		size_t outlen;
		decodeto(inlen);
		decodeto(outlen);
		if (inlen!=in.len)
		{
			size_t tmp=inlen;
			inlen=outlen;
			outlen=tmp;
			backwards=true;
		}
		if (inlen!=in.len) error(e_not_this);
		
		out->len=outlen;
		out->ptr=(uint8_t*)malloc(outlen);
		memset(out->ptr, 0, outlen);
		
		//uint8_t * instart=in.ptr;
		uint8_t * inat=in.ptr;
		uint8_t * inend=in.ptr+in.len;
		
		//uint8_t * outstart=out->ptr;
		uint8_t * outat=out->ptr;
		uint8_t * outend=out->ptr+out->len;
		
		while (patchat<patchend)
		{
			size_t skip;
			decodeto(skip);
			while (skip>0)
			{
				uint8_t out;
				if (inat>=inend) out=0;
				else out=readin8();
				if (outat<outend) writeout8(out);
				skip--;
			}
			uint8_t tmp;
			do
			{
				tmp=readpatch8();
				uint8_t out;
				if (inat>=inend) out=0;
				else out=readin8();
				if (outat<outend) writeout8(out^tmp);
			}
			while (tmp);
		}
		if (patchat!=patchend) error(e_broken);
		while (outat<outend) writeout8(0);
		while (inat<inend) (void)readin8();
		
		uint32_t crc_in_expected=read32(patchat);
		uint32_t crc_out_expected=read32(patchat+4);
		uint32_t crc_patch_expected=read32(patchat+8);
		
		uint32_t crc_in=crc32(in.v());
		uint32_t crc_out=crc32(out->v());
		uint32_t crc_patch=crc32(patch.v().slice(0, patch.len-4));
		
		if (inlen==outlen)
		{
			if ((crc_in!=crc_in_expected || crc_out!=crc_out_expected) &&
			    (crc_in!=crc_out_expected || crc_out!=crc_in_expected))
			{
				error(e_not_this);
			}
		}
		else
		{
			if (!backwards)
			{
				if (crc_in!=crc_in_expected) error(e_not_this);
				if (crc_out!=crc_out_expected) error(e_not_this);
			}
			else
			{
				if (crc_in!=crc_out_expected) error(e_not_this);
				if (crc_out!=crc_in_expected) error(e_not_this);
			}
		}
		if (crc_patch!=crc_patch_expected) error(e_broken);
		
		target_.write(out->v());
		free(out->ptr);
		patch_.unmap(patch.v());
		source_.unmap(in.v());
		return e_ok;
#undef read8
#undef decodeto
#undef write8
	}
	
exit:
	
	free(out->ptr);
	patch_.unmap(patch.v());
	source_.unmap(in.v());
	out->len=0;
	out->ptr=NULL;
	return error;
}

#if 0
//Sorry, no undocumented features here. The only thing that can change an UPS patch is swapping the two sizes and checksums, and I don't create anyways.
#endif
}}