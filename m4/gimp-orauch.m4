######################################################################
# Configure paths for The GIMP 
# Oliver Rauch 2000-12-28, Changes 2004-04-20
# Parts from the original gimp.m4
# with extensions that this version can handle gimp-1.0.x

dnl AM_PATH_GIMP_ORAUCH([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for GIMP, and define GIMP_CFLAGS and GIMP_LIBS
dnl
AC_DEFUN(AM_PATH_GIMP_ORAUCH,
[dnl 
dnl Get the cflags and libraries from the gimp-config script
dnl
  AC_ARG_WITH(gimp-prefix,[  --with-gimp-prefix=PFX   Prefix where GIMP is installed (optional)],
              gimp_config_prefix="$withval", gimp_config_prefix="")
  AC_ARG_WITH(gimp-exec-prefix,[  --with-gimp-exec-prefix=PFX Exec prefix where GIMP is installed (optional)],
              gimp_config_exec_prefix="$withval", gimp_config_exec_prefix="")
  AC_ARG_ENABLE(gimptest, [  --disable-gimptest      do not try to compile and run a test GIMP program], , enable_gimptest=yes)

  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"
  ac_save_GTK_CFLAGS="$GTK_CFLAGS"
  ac_save_GTK_LIBS="$GTK_LIBS"

  if test x$gimp_config_exec_prefix != x ; then
     gimp_config_args="$gimp_config_args --exec-prefix=$gimp_config_exec_prefix"
     if test x${GIMP_CONFIG+set} != xset ; then
        GIMP_CONFIG=$gimp_config_exec_prefix/bin/gimp-config
     fi
  fi

  if test x$gimp_config_prefix != x ; then
     gimp_config_args="$gimp_config_args --prefix=$gimp_config_prefix"
     if test x${GIMP_CONFIG+set} != xset ; then
        GIMP_CONFIG=$gimp_config_prefix/bin/gimp-config
     fi
  fi

  AC_PATH_PROG(GIMP_CONFIG, gimp-config, no)
  if test "$GIMP_CONFIG" = "no" ; then
    if test x$gimp_config_exec_prefix != x ; then
       gimp_config_args="$gimp_config_args --exec-prefix=$gimp_config_exec_prefix"
       if test x${GIMP_TOOL+set} != xset ; then
          GIMP_TOOL=$gimp_config_exec_prefix/bin/gimptool
       fi
    fi
    if test x$gimp_config_prefix != x ; then
       gimp_config_args="$gimp_config_args --prefix=$gimp_config_prefix"
       if test x${GIMP_TOOL+set} != xset ; then
          GIMP_TOOL=$gimp_config_prefix/bin/gimptool
       fi
    fi
    AC_PATH_PROG(GIMP_TOOL, gimptool, no)
    GIMP_CONFIG=$GIMP_TOOL
  fi

  min_gimp_version=ifelse([$1], ,1.0.0,$1)
  no_gimp=""

  if test "$GIMP_CONFIG" = "no" ; then
dnl we do not have gimp-config (gimp-1.0.x does not have gimp-config)
dnl so we have to use the GTK_* things for testing for gimp.h and gimpfeatures.h
    ac_save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="${CPPFLAGS} ${GTK_CFLAGS}"
    AC_CHECK_HEADERS(libgimp/gimp.h, GIMP_LIBS="-lgimp", no_gimp=yes)
    AC_CHECK_HEADERS(libgimp/gimpfeatures.h)
    CPPFLAGS="$ac_save_CPPFLAGS"
    CFLAGS="${CFLAGS} ${GTK_CFLAGS}"
    LIBS="${LIBS} ${GTK_LIBS} ${GIMP_LIBS}"

    if test "x$no_gimp" = x ; then
      dnl *** we have found libgimp/gimp.h ***
      AC_MSG_CHECKING(GIMP compilation)
      gimp_config_major_version=-1
      gimp_config_minor_version=0
      gimp_config_micro_version=0
    fi
  else
dnl Ok, we have gimp-config and so we do not need the GTK_* things because they are
dnl included in the output of gimp-config
    GTK_CFLAGS=""
    GTK_LIBS=""
    GIMP_CFLAGS=`$GIMP_CONFIG $gimp_config_args --cflags`
    GIMP_LIBS=`$GIMP_CONFIG $gimp_config_args --libs`" -lgimp"
    gimp_config_major_version=`$GIMP_CONFIG $gimp_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gimp_config_minor_version=`$GIMP_CONFIG $gimp_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gimp_config_micro_version=`$GIMP_CONFIG $gimp_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
      CFLAGS="${CFLAGS} ${GIMP_CFLAGS}"
      LIBS="${LIBS} ${GIMP_LIBS}"
      AC_MSG_CHECKING(for GIMP - version >= $min_gimp_version)
  fi

dnl
dnl Now check if the installed GIMP is sufficiently new. (Also sanity
dnl checks the results of gimp-config to some extent
dnl
  if test "x$no_gimp" = x ; then
    if test "$enable_gimptest" = "yes" ; then
      rm -f conf.gimptest
      AC_TRY_RUN([
#include <libgimp/gimp.h>
#include <stdio.h>

#define GIMP_TEST_CHECK_VERSION(major, minor, micro) \
    ($gimp_config_major_version > (major) || \
     ($gimp_config_major_version == (major) && $gimp_config_minor_version > (minor)) || \
     ($gimp_config_major_version == (major) && $gimp_config_minor_version == (minor) && \
      $gimp_config_micro_version >= (micro)))                                

#if !GIMP_TEST_CHECK_VERSION(1,1,25)
#  define GimpPlugInInfo GPlugInInfo /* do test with gimp interface version 1.0 */
#endif

GimpPlugInInfo PLUG_IN_INFO =
{
  NULL, NULL, NULL, NULL
};


int 
main ()
{
  int major, minor, micro;

  system ("touch conf.gimptest");

  if (sscanf("$min_gimp_version", "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gimp_version");
     exit(1);
   }

  if ( ($gimp_config_major_version != -1) &&
       ((gimp_major_version != $gimp_config_major_version) ||
       (gimp_minor_version != $gimp_config_minor_version) ||
       (gimp_micro_version != $gimp_config_micro_version)) )
  {
      printf("\n*** 'gimp-config --version' returned %d.%d.%d, but GIMP (%d.%d.%d)\n", 
             $gimp_config_major_version, $gimp_config_minor_version, $gimp_config_micro_version,
             gimp_major_version, gimp_minor_version, gimp_micro_version);
      printf ("*** was found! If gimp-config was correct, then it is best\n");
      printf ("*** to remove the old version of GIMP. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gimp-config was wrong, set the environment variable GIMP_CONFIG\n");
      printf("*** to point to the correct copy of gimp-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
  } 
  else
  {
      if ((gimp_major_version > major) ||
        ((gimp_major_version == major) && (gimp_minor_version > minor)) ||
        ((gimp_major_version == major) && (gimp_minor_version == minor) && (gimp_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GIMP (%d.%d.%d) was found.\n",
               gimp_major_version, gimp_minor_version, gimp_micro_version);
        printf("*** You need a version of GIMP newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GIMP is always available from ftp://ftp.gimp.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gimp-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GIMP, but you can also set the GIMP_CONFIG environment to point to the\n");
        printf("*** correct copy of gimp-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
  }
  return 1;
}
], , no_gimp=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
     fi
  fi

  if test "x$no_gimp" = x ; then
     dnl *** gimp test succeeded or not tested ***
     if test "$enable_gimptest" = "yes" ; then
       AC_MSG_RESULT(yes)
     else
       AC_MSG_RESULT(not tested)
     fi
     if test "$GIMP_CONFIG" != "no" ; then
       ac_save_CPPFLAGS="$CPPFLAGS"
       CPPFLAGS="${CPPFLAGS} ${GIMP_CFLAGS}"
       AC_CHECK_HEADERS(libgimp/gimp.h)
       AC_CHECK_HEADERS(libgimp/gimpfeatures.h)
       CPPFLAGS="$ac_save_CPPFLAGS"
     fi
     CFLAGS="$ac_save_CFLAGS"
     LIBS="$ac_save_LIBS"
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
       if test -f conf.gimptest ; then
        :
       else
        echo ""
        echo "*** Could not run GIMP test program, checking why..."
        CFLAGS="$CFLAGS $GIMP_CFLAGS"
        LIBS="$LIBS $GIMP_LIBS"
        AC_TRY_LINK([
#include <libgimp/gimp.h>
#include <stdio.h>
#define GIMP_TEST_CHECK_VERSION(major, minor, micro) \
    ($gimp_config_major_version > (major) || \
     ($gimp_config_major_version == (major) && $gimp_config_minor_version > (minor)) || \
     ($gimp_config_major_version == (major) && $gimp_config_minor_version == (minor) && \
      $gimp_config_micro_version >= (micro)))                                

#if !GIMP_TEST_CHECK_VERSION(1,1,25)
#  define GimpPlugInInfo GPlugInInfo /* do test with gimp interface version 1.0 */
#endif
GimpPlugInInfo PLUG_IN_INFO = { NULL, NULL, NULL, NULL };
],    [ return ((gimp_major_version) || (gimp_minor_version) || (gimp_micro_version)); ],
      [ echo "*** The test program compiled, but did not run. This usually means"
        echo "*** that the run-time linker is not finding GIMP or finding the wrong"
        echo "*** version of GIMP. If it is not finding GIMP, you'll need to set your"
        echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
        echo "*** to the installed location  Also, make sure you have run ldconfig if that"
        echo "*** is required on your system"
        echo "***"
        echo "*** If you have an old version installed, it is best to remove it, although"
        echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
        echo "***"
        echo "***" ])
        CFLAGS="$ac_save_CFLAGS"
        LIBS="$ac_save_LIBS"
     fi

     dnl *** ok, gimp does not work, so we have to use the gtk_* things again ***
     GIMP_CFLAGS=""
     GIMP_LIBS=""
     GTK_CFLAGS="$ac_save_GTK_CFLAGS"
     GTK_LIBS="$ac_save_GTK_LIBS"
     CFLAGS="${CFLAGS} ${GTK_CFLAGS}"
     LIBS="${LIBS} ${GTK_LIBS}"
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GIMP_CFLAGS)
  AC_SUBST(GIMP_LIBS)
  rm -f conf.gimptest
])

