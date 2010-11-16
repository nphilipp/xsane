#############################################################
# when compiling on Win32 we have to add the option
# -fnative-struct for gcc2 or
# --mms-bitfields for gcc3
# parts from glib configure.in

dnl AC_GCC_MSNATIVE_STRUCT
dnl test if gcc supports option for MSnative struct
dnl
AC_DEFUN(AC_GCC_MSNATIVE_STRUCT,
[dnl
dnl Add ms native struct gcc option to CFLAGS if available
dnl
dnl  if test x"$glib_native_win32" = xyes; then
 AC_CHECK_HEADERS(windows.h,
 [
    if test x"$GCC" = xyes; then
      msnative_struct=''
      AC_MSG_CHECKING([how to get MSVC-compatible struct packing])
      if test -z "$ac_cv_prog_CC"; then
        our_gcc="$CC"
      else
        our_gcc="$ac_cv_prog_CC"
      fi
      case `$our_gcc --version | sed -e 's,\..*,.,' -e q` in
        2.)
          if $our_gcc -v --help 2>/dev/null | grep fnative-struct >/dev/null; then
            msnative_struct='-fnative-struct'
          fi
          ;;
        *)
         if $our_gcc -v --help 2>/dev/null | grep ms-bitfields >/dev/null; then
           msnative_struct='-mms-bitfields'
         fi
         ;;
      esac
      if test x"$msnative_struct" = x ; then
        AC_MSG_RESULT([no way])
        AC_MSG_WARN([produced libraries might be incompatible with MSVC-compiled code])
      else
        CFLAGS="$CFLAGS $msnative_struct"
        AC_MSG_RESULT([${msnative_struct}])
      fi
    fi
dnl  fi
 ])
])

