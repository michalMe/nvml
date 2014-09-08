/*
 * Copyright (c) 2014, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * log_walker.c -- unit test to verify pool's write-protection in debug mode
 *
 * usage: log_walker file
 *
 */

#include <sys/param.h>
#include "unittest.h"

/*
 * do_append -- call pmemlog_append() & print result
 */
void
do_append(PMEMlog *plp)
{
	const char *str[6] = {
		"1st append string\n",
		"2nd append string\n",
		"3rd append string\n",
		"4th append string\n",
		"5th append string\n",
		"6th append string\n"
	};

	for (int i = 0; i < 6; ++i) {
		int rv = pmemlog_append(plp, str[i], strlen(str[i]));
		switch (rv) {
		case 0:
			OUT("append   str[%i] %s", i, str[i]);
			break;
		case -1:
			OUT("!append   str[%i] %s", i, str[i]);
			break;
		default:
			OUT("!append: wrong return value");
			break;
		}
	}
}

/*
 * try_to_store -- try to store to the buffer 'buf'
 *
 * It is a walker function for pmemlog_walk
 */
int
try_to_store(const void *buf, size_t len, void *arg)
{
	memset((void *)buf, 0, len);
	return 0;
}

/*
 * do_walk -- call pmemlog_walk() & print result
 */
void
do_walk(PMEMlog *plp)
{
	pmemlog_walk(plp, 0, try_to_store, NULL);
	OUT("walk all at once");
}

sigjmp_buf Jmp;

/*
 * signal_handler -- called on SIGSEGV
 */
void
signal_handler(int sig)
{
	OUT("signal: %s", strsignal(sig));

	siglongjmp(Jmp, 1);
}

int
main(int argc, char *argv[])
{
	PMEMlog *plp;

	START(argc, argv, "log_walker");

	if (argc != 2)
		FATAL("usage: %s file-name", argv[0]);

	int fd = OPEN(argv[1], O_RDWR);

	/* pre-allocate 2MB of persistent memory */
	errno = posix_fallocate(fd, (off_t)0, (size_t)(2 * 1024 * 1024));
	if (errno != 0)
		FATAL("!posix_fallocate");

	if ((plp = pmemlog_map(fd)) == NULL)
		FATAL("!pmemlog_map: %s", argv[1]);

	close(fd);

	/* append some data */
	do_append(plp);

	/* arrange to catch SEGV */
	struct sigvec v = { 0 };
	v.sv_handler = signal_handler;
	SIGVEC(SIGSEGV, &v, NULL);

	if (!sigsetjmp(Jmp, 1)) {
		do_walk(plp);
	}

	pmemlog_unmap(plp);

	DONE(NULL);
}
