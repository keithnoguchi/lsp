/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./server", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[6];
		int		want;
	} *t, tests[] = {
		{
			.name		= "-h option",
			.argv		= {target, "-h", NULL},
			.want		= 0,
		},
		{
			.name		= "1sec timeout",
			.argv		= {target, "-t", "1", NULL},
			.want		= 0,
		},
		{
			.name		= "100 listening backlog",
			.argv		= {target, "-b", "100", "-t", "1", NULL},
			.want		= 0,
		},
		{}, /* sentry */
	};
	int ret = -1;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			abort();
		} else if (pid == 0) {
			/* child */
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				exit(EXIT_FAILURE);
			}
			/* not reachable */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			abort();
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: unexpected child signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			break;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: unexpected child not terminated\n",
				t->name);
			break;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected exit status\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			break;
		}
		ret = 0;
	}
	if (target)
		free(target);
	if (ret)
		return 1;
	return 0;
}