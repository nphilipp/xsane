#ifdef HAVE_SIGPROCMASK
# define SIGACTION	sigaction
#else

/* Just enough backwards compatibility that we get by in the backends
   without making handstands.  */
# ifdef sigset_t
#  undef sigset_t
# endif
# ifdef sigemptyset
#  undef sigemptyset
# endif
# ifdef sigfillset
#  undef sigfillset
# endif
# ifdef sigaddset
#  undef sigaddset
# endif
# ifdef sigdelset
#  undef sigdelset
# endif
# ifdef sigprocmask
#  undef sigprocmask
# endif
# ifdef SIG_BLOCK
#  undef SIG_BLOCK
# endif
# ifdef SIG_UNBLOCK
#  undef SIG_UNBLOCK
# endif
# ifdef SIG_SETMASK
#  undef SIG_SETMASK
# endif

# define sigset_t		int
# define sigemptyset(set)	do { *(set) = 0; } while (0)
# define sigfillset(set)	do { *(set) = ~0; } while (0)
# define sigaddset(set,signal)	do { *(set) |= sigmask (signal); } while (0)
# define sigdelset(set,signal)	do { *(set) &= ~sigmask (signal); } while (0)
# define sigaction(sig,new,old)	sigvec (sig,new,old)

  /* Note: it's not safe to just declare our own "struct sigaction" since
     some systems (e.g., some versions of OpenStep) declare that structure,
     but do not implement sigprocmask().  Hard to believe, aint it?  */
# define SIGACTION		sigvec
# define SIG_BLOCK	1
# define SIG_UNBLOCK	2
# define SIG_SETMASK	3
#endif /* !HAVE_SIGPROCMASK */

