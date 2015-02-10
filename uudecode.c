/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "text.h"
#include "util.h"

static void uudecodeb64(FILE *, FILE *);
static void uudecode(FILE *, FILE *);
static void parseheader(FILE *, const char *, char **, mode_t *, char **);
static FILE *parsefile(const char *);

static void
usage(void)
{
	eprintf("usage: %s [-m] [-o output] [file]\n", argv0);
}

static int mflag = 0;
static int oflag = 0;

int
main(int argc, char *argv[])
{
	FILE *fp = NULL, *nfp = NULL;
	char *fname, *header;
	const char *ifname;
	mode_t mode = 0;
	void (*d) (FILE *, FILE *) = NULL;
	char *ofname = NULL;

	ARGBEGIN {
	case 'm':
		mflag = 1; /* accepted but unused (autodetect file type) */
		break;
	case 'o':
		oflag = 1;
		ofname = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND;

	if (argc > 1)
		usage();

	if (argc == 0) {
		fp = stdin;
		ifname = "<stdin>";
	} else {
		if (!(fp = fopen(argv[0], "r")))
			eprintf("fopen %s:", argv[0]);
		ifname = argv[0];
	}

	parseheader(fp, ifname, &header, &mode, &fname);

	if (!strncmp(header, "begin", sizeof("begin")))
		d = uudecode;
	else if (!strncmp(header, "begin-base64", sizeof("begin-base64")))
		d = uudecodeb64;
	else
		eprintf("unknown header %s:", header);

	if (oflag)
		fname = ofname;
	if (!(nfp = parsefile(fname)))
		eprintf("fopen %s:", fname);

	d(fp, nfp);

	if (nfp != stdout && chmod(fname, mode) < 0)
		eprintf("chmod %s:", fname);
	if (fp)
		fclose(fp);
	if (nfp)
		fclose(nfp);

	return 0;
}

static FILE *
parsefile(const char *fname)
{
	struct stat st;
	int ret;

	if (strcmp(fname, "/dev/stdout") == 0 || strcmp(fname, "-") == 0)
		return stdout;
	ret = lstat(fname, &st);
	/* if it is a new file, try to open it */
	if (ret < 0 && errno == ENOENT)
		goto tropen;
	if (ret < 0) {
		weprintf("lstat %s:", fname);
		return NULL;
	}
	if (!S_ISREG(st.st_mode)) {
		weprintf("for safety uudecode operates only on regular files and /dev/stdout\n");
		return NULL;
	}
tropen:
	return fopen(fname,"w");
}

static void
parseheader(FILE *fp, const char *s, char **header, mode_t *mode,
	    char **fname)
{
	char bufs[PATH_MAX + 18]; /* len header + mode + maxname */
	char *p, *q;
	size_t n;

	if (!fgets(bufs, sizeof(bufs), fp))
		if (ferror(fp))
			eprintf("%s: read error:", s);
	if (bufs[0] == '\0' || feof(fp))
		eprintf("empty or nil header string\n");
	if (!(p = strchr(bufs, '\n')))
		eprintf("header string too long or non-newline terminated file\n");
	p = bufs;
	if (!(q = strchr(p, ' ')))
		eprintf("malformed mode string in header\n");
	*header = bufs;
	*q++ = '\0';
	p = q;
	/* now header should be null terminated, q points to mode */
	if (!(q = strchr(p, ' ')))
		eprintf("malformed mode string in header\n");
	*q++ = '\0';
	/* now mode should be null terminated, q points to fname */
	*mode = parsemode(p, *mode, 0);
	n = strlen(q);
	while (n > 0 && (q[n - 1] == '\n' || q[n - 1] == '\r'))
		q[--n] = '\0';
	if (n > 0)
		*fname = q;
}
static const char b64dt[] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,0,-1,-1,-1,0,1,2,3,4,5,6,
	7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
	49,50,51,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

static void
uudecodeb64(FILE *fp, FILE *outfp)
{
	char bufb[60], *pb;
	char out[45], *po;
	size_t n;
	int b = 0, e, t = 0;
	unsigned char b24[3] = {0, 0, 0};

	while ((n = fread(bufb, 1, sizeof(bufb), fp))) {
		for (pb = bufb, po = out; pb < bufb + n; pb++) {
			if (*pb == '=') {
				if (b == 0 || t) {
					/* footer size is ==== is 4 */
					if (++t < 4)
						continue;
					else
						return;
				} else if (b == 1) {
					eprintf("unexpected \"=\" appeared.");
				} else if (b == 2) {
					*po++ = b24[0];
					fwrite(out, 1, (po - out), outfp);
				} else if (b == 3) {
					*po++ = b24[0];
					*po++ = b24[1];
					fwrite(out, 1, (po - out), outfp);
				}
			}
			if ((e = b64dt[(int)*pb]) == -1) {
				eprintf("invalid byte \"%c\"", *pb);
			} else if (e == -2) /* whitespace */
				continue;
			switch (b) {
				case 0: b24[0] |= e << 2; break;
				case 1: b24[0] |= (e >> 4) & 0x3;
				        b24[1] |= (e & 0xf) << 4; break;
				case 2: b24[1] |= (e >> 2) & 0xf;
				        b24[2] |= (e & 0x3) << 6; break;
				case 3: b24[2] |= e; break;
			}
			if (++b == 4) {
				*po++ = b24[0];
				*po++ = b24[1];
				*po++ = b24[2];
				b24[0] = b24[1] = b24[2] = 0;
				b = 0;
			}
		}
		fwrite(out, 1, (po - out), outfp);
	}
	eprintf("invalid uudecode footer \"====\" not found\n");
}

static void
uudecode(FILE *fp, FILE *outfp)
{
	char *bufb = NULL, *p;
	size_t n = 0;
	ssize_t len;
	int ch, i;

#define DEC(c)  (((c) - ' ') & 077) /* single character decode */
#define IS_DEC(c) ( (((c) - ' ') >= 0) && (((c) - ' ') <= 077 + 1) )
#define OUT_OF_RANGE(c) eprintf("character %c out of range: [%d-%d]", (c), 1 + ' ', 077 + ' ' + 1)

	while ((len = getline(&bufb, &n, fp)) != -1) {
		p = bufb;
		/* trim newlines */
		if (len && bufb[len - 1] != '\n')
			bufb[len - 1] = '\0';
		else
			eprintf("no newline found, aborting\n");
		/* check for last line */
		if ((i = DEC(*p)) <= 0)
			break;
		for (++p; i > 0; p += 4, i -= 3) {
			if (i >= 3) {
				if (!(IS_DEC(*p) && IS_DEC(*(p + 1)) &&
				      IS_DEC(*(p + 2)) && IS_DEC(*(p + 3))))
					OUT_OF_RANGE(*p);

				ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
				putc(ch, outfp);
				ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
				putc(ch, outfp);
				ch = DEC(p[2]) << 6 | DEC(p[3]);
				putc(ch, outfp);
			} else {
				if (i >= 1) {
					if (!(IS_DEC(*p) && IS_DEC(*(p + 1))))
						OUT_OF_RANGE(*p);

					ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
					putc(ch, outfp);
				}
				if (i >= 2) {
					if (!(IS_DEC(*(p + 1)) &&
					      IS_DEC(*(p + 2))))
						OUT_OF_RANGE(*p);

					ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
					putc(ch, outfp);
				}
			}
		}
		if (ferror(fp))
			eprintf("read error:");
	}
	/* check for end or fail */
	len = getline(&bufb, &n, fp);
	if (len < 3 || strncmp(bufb, "end", 3) != 0 || bufb[3] != '\n')
		eprintf("invalid uudecode footer \"end\" not found\n");
	free(bufb);
}
