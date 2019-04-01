/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct client {
	char			buf[LINE_MAX];
	int			sd;
	const struct process	*p;
};

static struct process {
	struct client		client[1];		/* single client */
	const char		*prompt;
	int			port;
	socklen_t		salen;
	struct sockaddr_storage	ss;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.client[0].sd	= -1,
	.progname	= NULL,
	.prompt		= "client",
	.port		= 9999,
	.salen		= -1,
	.ss.ss_family	= AF_UNSPEC,
	.opts		= "h",
	.lopts		= {
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s] <server IP address>\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'h':
			fprintf(s, "\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int is_print_prompt(const struct process *restrict p)
{
	if (!isatty(STDOUT_FILENO))
	    return 0;
	return p->prompt[0] != '\0';
}

static void prompt(const struct process *restrict p)
{
	if (is_print_prompt(p))
		printf("%s> ", p->prompt);
}

static char *fetch(struct client *ctx)
{
	char *cmdline;
	prompt(ctx->p);
	cmdline = fgets(ctx->buf, sizeof(ctx->buf), stdin);
	if (cmdline == NULL)
		return NULL;
	cmdline[strlen(cmdline)-1] = '\0'; /* drop newline */
	return cmdline;
}

static int connect_server(struct client *ctx, const char *cmdline)
{
	const struct process *const p = ctx->p;
	struct sockaddr *sa = (struct sockaddr *)&p->ss;
	int ret, sd = -1;

	sd = socket(sa->sa_family, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		return -1;
	}
	ret = connect(sd, sa, p->salen);
	if (ret == -1) {
		perror("connect");
		goto err;
	}
	ctx->sd = sd;
	return 0;
err:
	if (sd != -1)
		if (close(sd))
			perror("close");
	return -1;
}

static ssize_t send_command(struct client *ctx, const char *cmdline)
{
	ssize_t rem;
	char *buf;

	rem = strlen(cmdline)+1; /* null */
	buf = (char *)cmdline;
	while (rem > 0) {
		ssize_t len = send(ctx->sd, buf, rem, 0);
		if (len == -1) {
			perror("send");
			return -1;
		}
		rem -= len;
		buf += len;
	}
	return rem;
}

static int handle(struct client *ctx, const char *cmdline)
{
	ssize_t len;
	int ret;

	if (!strncasecmp(cmdline, "quit", strlen(cmdline)))
		return 0;
	ret = connect_server(ctx, cmdline);
	if (ret == -1)
		return -1;
	len = send_command(ctx, cmdline);
	if (len == -1)
		goto out;
out:
	if (close(ctx->sd))
		perror("close");
	ctx->sd = -1;
	return 1;
}

static int init_address(struct process *p, const char *addr)
{
	unsigned short port = p->port;
	struct sockaddr_in *sin4;
	char *colon;
	int ret;

	colon = strrchr(addr, ':');
	if (colon != NULL) {
		int val = strtol(colon, NULL, 10);
		if (val > 0 && val <= USHRT_MAX) {
			port = val;
			*colon = '\0';
		}
	}
	sin4 = (struct sockaddr_in *)&p->ss;
	ret = inet_pton(AF_INET, addr, &sin4->sin_addr);
	if (ret == 1) {
		p->salen = sizeof(struct sockaddr_in);
		sin4->sin_family = AF_INET;
		sin4->sin_port = htons(port);
		return 0;
	}
	/* No IPv6 support yet */
	return -1;
}

static int init(struct process *p, char *addr)
{
	struct client *c = p->client;
	int ret;

	ret = init_address(p, addr);
	if (ret == -1)
		return -1;
	c->p = p;
	return 0;
}

static void term(const struct process *restrict p)
{
	return;
}

int main(int argc, char *const argv[])
{
	struct process *const p = &process;
	struct client *ctx;
	const char *cmd;
	int o, ret;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		switch (o) {
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	}
	if (optind >= argc)
		usage(p, stderr, EXIT_FAILURE);
	ret = init(p, argv[optind]);
	if (ret == -1)
		return 1;
	ctx = p->client;
	while ((cmd = fetch(ctx)))
		if ((ret = handle(ctx, cmd)) <= 0)
			break;
	term(p);
	if (ret)
		return 1;
	return 0;
}
