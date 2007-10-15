/* Old Liquid Tracker "NO" module loader for xmp
 * Copyright (C) 2007 Claudio Matsuoka
 *
 * $Id: no_load.c,v 1.10 2007-10-15 23:37:24 cmatsuoka Exp $
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "period.h"

/* Nir Oren's Liquid Tracker old "NO" format. I have only one NO module,
 * Moti Radomski's "Time after time" from ftp.modland.com.
 */


static int no_test (FILE *, char *);
static int no_load (struct xmp_mod_context *, FILE *);

struct xmp_loader_info no_loader = {
	"LIQ",
	"Liquid Tracker (old)",
	no_test,
	no_load
};

static int no_test(FILE *f, char *t)
{
	if (read32b(f) != 0x4e4f0000)		/* NO 0x00 0x00 */
		return -1;

	read_title(f, t, read8(f));

	return 0;
}


static uint8 fx[] = {
	FX_ARPEGGIO,
	0,
	FX_BREAK,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};


static int no_load(struct xmp_mod_context *m, FILE *f)
{
	struct xxm_event *event;
	int i, j, k;
	int nsize;

	LOAD_INIT();

	read32b(f);			/* NO 0x00 0x00 */

	strcpy(m->type, "NO (old Liquid Tracker)");

	nsize = read8(f);
	for (i = 0; i < nsize; i++) {
		uint8 x = read8(f);
		if (i < XMP_DEF_NAMESIZE)
			m->name[i] = x;
	}

	read16l(f);
	read16l(f);
	read16l(f);
	read16l(f);
	read8(f);
	m->xxh->pat = read8(f);
	read8(f);
	m->xxh->chn = read8(f);
	m->xxh->trk = m->xxh->pat * m->xxh->chn;
	read8(f);
	read16l(f);
	read16l(f);
	read8(f);
	m->xxh->ins = m->xxh->smp = 63;

	for (i = 0; i < 256; i++) {
		uint8 x = read8(f);
		if (x == 0xff)
			break;
		m->xxo[i] = x;
	}
	fseek(f, 255 - i, SEEK_CUR);
	m->xxh->len = i;

	MODULE_INFO();

	INSTRUMENT_INIT();
	reportv(1, "     Instrument name         SLen SBeg SEnd L Vol C2spd\n");

	/* Read instrument names */
	for (i = 0; i < m->xxh->ins; i++) {
		int hasname, c2spd;

		m->xxi[i] = calloc(sizeof(struct xxm_instrument), 1);

		nsize = read8(f);
		hasname = 0;
		for (j = 0; j < nsize; j++) {
			uint8 x = read8(f);
			if (x != 0x20)
				hasname = 1;
			if (j < 32)
				m->xxih[i].name[j] = x;
		}
		if (!hasname)
			m->xxih[i].name[0] = 0;

		read32l(f);
		read32l(f);
		m->xxi[i][0].vol = read8(f);
		c2spd = read16l(f);
		m->xxs[i].len = read16l(f);
		m->xxs[i].lps = read16l(f);
		m->xxs[i].lpe = read16l(f);
		read32l(f);
		read16l(f);

		m->xxih[i].nsm = !!(m->xxs[i].len);
		m->xxs[i].lps = 0;
		m->xxs[i].lpe = 0;
		m->xxs[i].flg = m->xxs[i].lpe > 0 ? WAVE_LOOPING : 0;
		m->xxi[i][0].fin = 0;
		m->xxi[i][0].pan = 0x80;
		m->xxi[i][0].sid = i;

		if (V(1) && (strlen((char*)m->xxih[i].name) || (m->xxs[i].len > 1))) {
			report("[%2X] %-22.22s  %04x %04x %04x %c V%02x %5d\n",
				i, m->xxih[i].name,
				m->xxs[i].len, m->xxs[i].lps, m->xxs[i].lpe,
				m->xxs[i].flg & WAVE_LOOPING ? 'L' : ' ',
				m->xxi[i][0].vol, c2spd);
		}

		c2spd = 8363 * c2spd / 8448;
		c2spd_to_note(c2spd, &m->xxi[i][0].xpo, &m->xxi[i][0].fin);
	}

	PATTERN_INIT();

	/* Read and convert patterns */
	reportv(0, "Stored patterns: %d ", m->xxh->pat);

	for (i = 0; i < m->xxh->pat; i++) {
//printf("%d  %x\n", i, ftell(f));
		PATTERN_ALLOC(i);
		m->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < m->xxp[i]->rows; j++) {
			for (k = 0; k < m->xxh->chn; k++) {
				uint32 x, note, ins, vol, fxt, fxp;

				event = &EVENT (i, k, j);

				x = read32l(f);
				note = x & 0x0000003f;
				ins = (x & 0x00001fc0) >> 6;
				vol = (x & 0x000fe000) >> 13;
				fxt = (x & 0x00f00000) >> 20;
				fxp = (x & 0xff000000) >> 24;

				if (note != 0x3f)
					event->note = 24 + note;
				if (ins != 0x7f)
					event->ins = 1 + ins;
				if (vol != 0x7f)
					event->vol = vol;
				if (fxt != 0x0f) {
					event->fxt = fx[fxt];
					event->fxp = fxp;
				}
			}
		}
		reportv(0, ".");
	}
	reportv(0, "\n");

	/* Read samples */
	reportv(0, "Stored samples : %d ", m->xxh->smp);
	for (i = 0; i < m->xxh->ins; i++) {
		if (m->xxs[i].len == 0)
			continue;
		xmp_drv_loadpatch(f, m->xxi[i][0].sid, xmp_ctl->c4rate,
				XMP_SMP_UNS, &m->xxs[m->xxi[i][0].sid], NULL);
		reportv(0, ".");
	}
	reportv(0, "\n");

	return 0;
}
