dnl This file should be included by spice-common user from
dnl configure.ac to have all required checks in order to use spice-common
m4_define(spice_common_dir,m4_bpatsubst(__file__,[/m4/common\.m4],[]))dnl
dnl Automatically include m4 files in m4 directory
AC_CONFIG_MACRO_DIRS(spice_common_dir[/m4])dnl
SPICE_COMMON(spice_common_dir)
