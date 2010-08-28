#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ber.h>
#include <autober.h>

static const struct autober_tag *do_find_tag(const struct autober_tag *tags,
					unsigned int n, gber_tag_t id)
{
	const struct autober_tag *t;

	for(t = tags; n; ) {
		unsigned int i;
		int cmp;

		i = n / 2U;
		cmp = id - t[i].ab_tag;
		if ( cmp < 0 ) {
			n = i;
		}else if ( cmp > 0 ) {
			t = t + (i + 1U);
			n = n - (i + 1U);
		}else
			return t + i;
	}

	return NULL;
}

const struct autober_tag *autober_find_tag(const struct autober_tag *tags,
					unsigned int n, gber_tag_t id)
{
	return do_find_tag(tags, n, id);
}

int autober_constraints(const struct autober_tag *tags,
				struct autober_constraint *cons,
				unsigned int num_tags,
				const uint8_t *ptr, size_t len)
{
	const uint8_t *end = ptr + len;
	unsigned int i;

	memset(cons, 0, sizeof(*cons) * num_tags);

	while(ptr < end) {
		const struct autober_tag *atag;
		unsigned int idx;
		struct gber_tag tag;

		ptr = ber_decode_tag(&tag, ptr, end - ptr);
		atag = do_find_tag(tags, num_tags, tag.ber_tag);
		if ( NULL == atag ) {
			ptr += tag.ber_len;
			continue;
		}

		idx = atag - tags;
		if ( atag->ab_flags & AUTOBER_TEMPLATE ) {
			if ( cons[idx].count && 
				!(atag->ab_flags & AUTOBER_SEQUENCE) ) {
				fprintf(stderr, "%s not expecting sequence\n",
					atag->ab_label);
				return 0;
			}
			cons[idx].count++;
			//printf("field[%u] template: %s: count = %u\n",
			//	idx, atag->ab_label, cons[idx].count);
		}else{
			if ( cons[idx].count ) {
				fprintf(stderr, "%s multiply defined\n",
					atag->ab_label);
				return 0;
			}
			cons[idx].count++;
			//printf("field[%u] fixed: %s: len = %u\n",
			//	idx, atag->ab_label, tag.ber_len);
		}
		cons[idx].len = tag.ber_len;

		if ( atag->ab_flags & AUTOBER_CHECK_SIZE ) {
			if ( cons[idx].len < atag->ab_size[0] ||
				cons[idx].len > atag->ab_size[1] ) {
				fprintf(stderr, "%s violates size constraint\n",
					atag->ab_label);
				fprintf(stderr, "%u < %u < %u\n",
					atag->ab_size[0],
					cons->len,
					atag->ab_size[1]);
				return 0;
			}
		}

		ptr += tag.ber_len;
	}

	for(i = 0; i < num_tags; i++) {
		if ( (tags[i].ab_flags & !(AUTOBER_OPTIONAL|AUTOBER_UNION))
			 && cons[i].count == 0 ) {
			fprintf(stderr, "Mandatory tag: %s: missing\n",
				tags[i].ab_label);
			return 0;
		}
	}

	return 1;
}

int autober_u8(uint8_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt)
{
	memcpy(out, ptr, tag->ber_len);
	if ( cnt )
		*cnt = tag->ber_len;
	return 1;
}

int autober_u16(uint16_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt)
{
	const uint8_t *end;
	unsigned int i;

	if ( tag->ber_len & 1 )
		return 0;

	for(i = 0, end = ptr + tag->ber_len;
			ptr < end; ptr += sizeof(*out), i++) {
		*out = (ptr[0] << 8) | ptr[1];
	}

	if ( cnt )
		*cnt = i;
	return 1;
}

int autober_u32(uint32_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt)
{
	const uint8_t *end;
	unsigned int i;

	if ( tag->ber_len & 3 )
		return 0;

	for(i = 0, end = ptr + tag->ber_len;
			ptr < end; ptr += sizeof(*out), i++) {
		*out = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] << 8) | ptr[3];
	}

	if ( cnt )
		*cnt = i;
	return 1;
}

int autober_u64(uint64_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt)
{
	const uint8_t *end;
	unsigned int i;

	if ( tag->ber_len & 7 )
		return 0;

	for(i = 0, end = ptr + tag->ber_len;
			ptr < end; ptr += sizeof(*out), i++) {
		*out =  ((uint64_t)ptr[0] << 56ULL) |
			((uint64_t)ptr[1] << 48ULL) |
			((uint64_t)ptr[2] << 40ULL) |
			((uint64_t)ptr[3] << 32ULL) |
			((uint64_t)ptr[4] << 24ULL) |
			((uint64_t)ptr[5] << 16ULL) |
			((uint64_t)ptr[6] << 8ULL)  |
			(uint64_t)ptr[7];
	}

	if ( cnt )
		*cnt = i;
	return 1;
}

int autober_octet(uint8_t *out, struct gber_tag *tag, const uint8_t *ptr,
		unsigned int *cnt)
{
	memcpy(out, ptr, tag->ber_len);
	if ( cnt )
		*cnt = tag->ber_len;
	return 1;
}

int autober_blob(struct autober_blob *out, struct gber_tag *tag,
			const uint8_t *ptr)
{
	out->ptr = malloc(tag->ber_len);
	if ( NULL == out->ptr )
		return 0;
	out->len = tag->ber_len;
	memcpy(out->ptr, ptr, tag->ber_len);
	return 1;
}

