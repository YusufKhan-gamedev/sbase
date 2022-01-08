/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define SYSLOG_NAMES

#include <syslog.h>
#include <unistd.h>

#include "util.h"
#define	INTERNAL_NOPRI 0x10
#define	INTERNAL_MARK (LOG_NFACILITIES<<3)
typedef struct {
	char *c_name;
	int c_val;
} CODE;
#define prioritynames ((CODE *)(const CODE []){ \
	{ "alert", LOG_ALERT }, { "crit", LOG_CRIT }, { "debug", LOG_DEBUG }, \
	{ "emerg", LOG_EMERG }, { "err", LOG_ERR }, { "error", LOG_ERR }, \
	{ "info", LOG_INFO }, { "none", INTERNAL_NOPRI }, \
	{ "notice", LOG_NOTICE }, { "panic", LOG_EMERG }, \
	{ "warn", LOG_WARNING }, { "warning", LOG_WARNING }, { 0, -1 } })
#define facilitynames ((CODE *)(const CODE []){ \
	{ "auth", LOG_AUTH }, { "authpriv", LOG_AUTHPRIV }, \
	{ "cron", LOG_CRON }, { "daemon", LOG_DAEMON }, { "ftp", LOG_FTP }, \
	{ "kern", LOG_KERN }, { "lpr", LOG_LPR }, { "mail", LOG_MAIL }, \
	{ "mark", INTERNAL_MARK }, { "news", LOG_NEWS }, \
	{ "security", LOG_AUTH }, { "syslog", LOG_SYSLOG }, \
	{ "user", LOG_USER }, { "uucp", LOG_UUCP }, \
	{ "local0", LOG_LOCAL0 }, { "local1", LOG_LOCAL1 }, \
	{ "local2", LOG_LOCAL2 }, { "local3", LOG_LOCAL3 }, \
	{ "local4", LOG_LOCAL4 }, { "local5", LOG_LOCAL5 }, \
	{ "local6", LOG_LOCAL6 }, { "local7", LOG_LOCAL7 }, { 0, -1 } })

static int
decodetable(CODE *table, char *name)
{
	CODE *c;

	for (c = table; c->c_name; c++)
		if (!strcasecmp(name, c->c_name))
			return c->c_val;
	eprintf("invalid priority name: %s\n", name);

	return -1; /* not reached */
}

static int
decodepri(char *pri)
{
	char *lev, *fac = pri;

	if (!(lev = strchr(pri, '.')))
		eprintf("invalid priority name: %s\n", pri);
	*lev++ = '\0';
	if (!*lev)
		eprintf("invalid priority name: %s\n", pri);

	return (decodetable(facilitynames, fac) & LOG_FACMASK) |
	       (decodetable(prioritynames, lev) & LOG_PRIMASK);
}

static void
usage(void)
{
	eprintf("usage: %s [-is] [-p priority] [-t tag] [message ...]\n", argv0);
}

int
main(int argc, char *argv[])
{
	size_t sz;
	int logflags = 0, priority = LOG_NOTICE, i;
	char *buf = NULL, *tag = NULL;

	ARGBEGIN {
	case 'i':
		logflags |= LOG_PID;
		break;
	case 'p':
		priority = decodepri(EARGF(usage()));
		break;
	case 's':
		logflags |= LOG_PERROR;
		break;
	case 't':
		tag = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	openlog(tag ? tag : getlogin(), logflags, 0);

	if (!argc) {
		while (getline(&buf, &sz, stdin) > 0)
			syslog(priority, "%s", buf);
	} else {
		for (i = 0, sz = 0; i < argc; i++)
			sz += strlen(argv[i]);
		sz += argc;
		buf = ecalloc(1, sz);
		for (i = 0; i < argc; i++) {
			estrlcat(buf, argv[i], sz);
			if (i + 1 < argc)
				estrlcat(buf, " ", sz);
		}
		syslog(priority, "%s", buf);
	}

	closelog();

	return fshut(stdin, "<stdin>");
}
