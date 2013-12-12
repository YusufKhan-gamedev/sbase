/* See LICENSE file for copyright and license details. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "text.h"
#include "util.h"

static int linecmp(const char **, const char **);

static bool rflag = false;
static bool uflag = false;
static bool nflag = false;

static struct linebuf linebuf = EMPTY_LINEBUF;

static void
usage(void)
{
	eprintf("usage: %s [-nru] [file...]\n", argv0);
}

int
main(int argc, char *argv[])
{
	long i;
	FILE *fp;

	ARGBEGIN {
	case 'n':
		nflag = true;
		break;
	case 'r':
		rflag = true;
		break;
	case 'u':
		uflag = true;
		break;
	default:
		usage();
	} ARGEND;

	if(argc == 0) {
		getlines(stdin, &linebuf);
	} else for(; argc > 0; argc--, argv++) {
		if(!(fp = fopen(argv[0], "r"))) {
			weprintf("fopen %s:", argv[0]);
			continue;
		}
		getlines(fp, &linebuf);
		fclose(fp);
	}
	qsort(linebuf.lines, linebuf.nlines, sizeof *linebuf.lines,
			(int (*)(const void *, const void *))linecmp);

	for(i = 0; i < linebuf.nlines; i++) {
		if(!uflag || i == 0 || strcmp(linebuf.lines[i],
					linebuf.lines[i-1]) != 0) {
			fputs(linebuf.lines[i], stdout);
		}
	}

	return EXIT_SUCCESS;
}

int
linecmp(const char **a, const char **b)
{
	if (nflag) {
		if (rflag)
			return strtoul(*b, 0, 10) - strtoul(*a, 0, 10);
		else
			return strtoul(*a, 0, 10) - strtoul(*b, 0, 10);
	}
	return strcmp(*a, *b) * (rflag ? -1 : +1);
}

