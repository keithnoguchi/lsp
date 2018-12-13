/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <linux/limits.h>

/* _daemon create a daemonized process and returns its PID */
int _daemon(void)
{
	pid_t pid;
	int i;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid)
		exit(EXIT_SUCCESS);

	/* make it session leader/process group leader */
	if (setsid() == -1) {
		perror("setsid");
		return -1;
	}
	if (chdir("/") == -1) {
		perror("chdir");
		return -1;
	}
	/* close all the open files */
	for (i = 0; i < NR_OPEN; i++)
		close(i);
	/* stdin */
	if (open("/dev/null", O_RDWR) == -1) {
		perror("open");
		return -1;
	}
	/* stdout */
	if (dup2(0, 1) == -1) {
		perror("dup2");
		return -1;
	}
	/* stderr */
	if (dup2(0, 2) == -1) {
		perror("dup2");
		return -1;
	}
	return pid;
}

int main(int argc, char *argv[])
{
	pid_t got, want;
	
	got = _daemon();
	if (got == -1) {
		perror("_daemon");
		return 1;
	}
	want = getsid(0);
	if (want == -1) {
		perror("getsid");
		return 1;
	}
	if (got != want) {
		fprintf(stderr, "unexpected session ID:\n- want: %d\n-  got: %d\n",
			want, got);
		return 1;
	}
	return 0;
}
