#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>

static const char *charset;

#include <locale.h>
#include <libintl.h>
#include <langinfo.h>
#define locale_charset() nl_langinfo(CODESET)

/* if any standard file descriptor is missing open it to /dev/null */
void sanitize_stdfds(void)
{
	int fd = open("/dev/null", O_RDWR, 0);
	while (fd != -1 && fd < 2)
		fd = dup(fd);
	if (fd == -1)
        fprintf(stderr,"open /dev/null or dup failed\n");
	if (fd > 2)
		close(fd);
}

static void init_gettext_charset(const char *domain)
{
	/*
	   This trick arranges for messages to be emitted in the user's
	   requested encoding, but avoids setting LC_CTYPE from the
	   environment for the whole program.

	   This primarily done to avoid a bug in vsnprintf in the GNU C
	   Library [1]. which triggered a "your vsnprintf is broken" error
	   on Git's own repository when inspecting v0.99.6~1 under a UTF-8
	   locale.

	   That commit contains a ISO-8859-1 encoded author name, which
	   the locale aware vsnprintf(3) won't interpolate in the format
	   argument, due to mismatch between the data encoding and the
	   locale.

	   Even if it wasn't for that bug we wouldn't want to use LC_CTYPE at
	   this point, because it'd require auditing all the code that uses C
	   functions whose semantics are modified by LC_CTYPE.

	   But only setting LC_MESSAGES as we do creates a problem, since
	   we declare the encoding of our PO files[2] the gettext
	   implementation will try to recode it to the user's locale, but
	   without LC_CTYPE it'll emit something like this on 'git init'
	   under the Icelandic locale:

	       Bj? til t?ma Git lind ? /hlagh/.git/

	   Gettext knows about the encoding of our PO file, but we haven't
	   told it about the user's encoding, so all the non-US-ASCII
	   characters get encoded to question marks.

	   But we're in luck! We can set LC_CTYPE from the environment
	   only while we call nl_langinfo and
	   bind_textdomain_codeset. That suffices to tell gettext what
	   encoding it should emit in, so it'll now say:

	       Bjó til tóma Git lind í /hlagh/.git/

	   And the equivalent ISO-8859-1 string will be emitted under a
	   ISO-8859-1 locale.

	   With this change way we get the advantages of setting LC_CTYPE
	   (talk to the user in his language/encoding), without the major
	   drawbacks (changed semantics for C functions we rely on).

	   However foreign functions using other message catalogs that
	   aren't using our neat trick will still have a problem, e.g. if
	   we have to call perror(3):

	   #include <stdio.h>
	   #include <locale.h>
	   #include <errno.h>

	   int main(void)
	   {
		   setlocale(LC_MESSAGES, "");
		   setlocale(LC_CTYPE, "C");
		   errno = ENODEV;
		   perror("test");
		   return 0;
	   }

	   Running that will give you a message with question marks:

	   $ LANGUAGE= LANG=de_DE.utf8 ./test
	   test: Kein passendes Ger?t gefunden

	   The vsnprintf bug has been fixed since glibc 2.17.

	   Then we could simply set LC_CTYPE from the environment, which would
	   make things like the external perror(3) messages work.

	   See t/t0203-gettext-setlocale-sanity.sh's "gettext.c" tests for
	   regression tests.

	   1. http://sourceware.org/bugzilla/show_bug.cgi?id=6530
	   2. E.g. "Content-Type: text/plain; charset=UTF-8\n" in po/is.po
	*/
	setlocale(LC_CTYPE, "");
	charset = locale_charset();
	bind_textdomain_codeset(domain, charset);
	// /* the string is taken from v0.99.6~1 */
	// if (test_vsnprintf("%.*s", 13, "David_K\345gedal") < 0)
	// 	setlocale(LC_CTYPE, "C");
}

void git_setup_gettext(void)
{
	const char *podir = getenv("GIT_TEXTDOMAINDIR");

	if (!podir)
		podir = "./po";
	bindtextdomain("git", podir);
	setlocale(LC_MESSAGES, "");
	setlocale(LC_TIME, "");
	init_gettext_charset("git");
	textdomain("git");
}


/*
 * Many parts of Git have subprograms communicate via pipe, expect the
 * upstream of a pipe to die with SIGPIPE when the downstream of a
 * pipe does not need to read all that is written.  Some third-party
 * programs that ignore or block SIGPIPE for their own reason forget
 * to restore SIGPIPE handling to the default before spawning Git and
 * break this carefully orchestrated machinery.
 *
 * Restore the way SIGPIPE is handled to default, which is what we
 * expect.
 */
static void restore_sigpipe_to_default(void)
{
	sigset_t unblock;

	sigemptyset(&unblock);
	sigaddset(&unblock, SIGPIPE);
	sigprocmask(SIG_UNBLOCK, &unblock, NULL);
	signal(SIGPIPE, SIG_DFL);
}

int main(int argc, const char **argv)
{
	/*
	 * Always open file descriptors 0/1/2 to avoid clobbering files
	 * in die().  It also avoids messing up when the pipes are dup'ed
	 * onto stdin/stdout/stderr in the child processes we spawn.
	 */
	sanitize_stdfds();
	// 载入语言包
	git_setup_gettext();

	restore_sigpipe_to_default();
    printf("stop here\n");

	// return cmd_main(argc, argv);
}