# options to filter out, and why
--all-warnings				alias for -Wall
--extra-warnings			alias for -Wextra
-Wabi-tag				c++
-Wabi=					c++
-Wabi					this is now a no-op
-Waggregate-return			obsolescent
-Waliasing				fortran
-Walign-commons				fortran
-Waligned-new=[none|global|all]		c++
-Walloca				we like alloca in small doses
-Walloca-larger-than=<number>		FIXME: choose something sane?
-Walloc-size-larger-than=		handled specially by gl_MANYWARN_ALL_GCC
-Walloc-zero				Gnulib fixes this problem
-Wampersand				fortran
-Wargument-mismatch			fortran
-Warray-bounds				covered by -Warray-bounds=
-Warray-bounds=<0,2>			handled specially by gl_MANYWARN_ALL_GCC
-Warray-temporaries			fortran
-Wassign-intercept			objc/objc++
-Wc++-compat				only useful for code meant to be compiled by a C++ compiler
-Wc++0x-compat				c++
-Wc++11-compat				c++
-Wc++14-compat				c++
-Wc++17-compat				c++
-Wc++1z-compat				c++
-Wc-binding-type			fortran
-Wc90-c99-compat			c compatibility
-Wc99-c11-compat			c compatibility
-Wcast-qual				FIXME maybe? too much noise; encourages bad changes
-Wcatch-value				c++
-Wcatch-value=<0,3>			c++
-Wcharacter-truncation			fortran
-Wchkp					deprecated
-Wclass-memaccess			c++
-Wcompare-reals				fortran
-Wconditionally-supported		c++ and objc++
-Wconversion				FIXME maybe? too much noise; encourages bad changes
-Wconversion-extra			fortran
-Wconversion-null			c++ and objc++
-Wctor-dtor-privacy			c++
-Wdeclaration-after-statement		FIXME: do not want.  others may
-Wdelete-incomplete			c++ and objc++
-Wdelete-non-virtual-dtor		c++
-Wdo-subscript				fortran
-Weffc++				c++
-Werror-implicit-function-declaration	deprecated
-Wextra-semi				c++
-Wfloat-conversion			FIXME maybe? borderline.  some will want this
-Wfloat-equal				FIXME maybe? borderline.  some will want this
-Wformat				covered by -Wformat=2
-Wformat=<0,2>				gcc --help=warnings artifact
-Wformat-overflow<0,2>			gcc --help=warnings artifact
-Wformat-overflow=<0,2>			handled specially by gl_MANYWARN_ALL_GCC
-Wformat-truncation			covered by -Wformat-truncation=2
-Wformat-truncation=<0,2>		handled specially by gl_MANYWARN_ALL_GCC
-Wframe-larger-than=<number>		FIXME: choose something sane?
-Wfunction-elimination			fortran
-Wimplicit-fallthrough			covered by -Wimplicit-fallthrough=2
-Wimplicit-fallthrough=<0,5>		handled specially by gl_MANYWARN_ALL_GCC
-Wimplicit-interface			fortran
-Wimplicit-procedure			fortran
-Winherited-variadic-ctor		c++
-Winteger-division			fortran
-Wintrinsic-shadow			fortran
-Wintrinsics-std			fortran
-Winvalid-offsetof			c++ and objc++
-Wjump-misses-init			only useful for code meant to be compiled by a C++ compiler
-Wlarger-than-				gcc --help=warnings artifact
-Wlarger-than=<number>			FIXME: choose something sane?
-Wline-truncation			fortran
-Wliteral-suffix			c++ and objc++
-Wlong-long				obsolescent
-Wlto-type-mismatch			c++ and objc++
-Wmissing-format-attribute		obsolescent
-Wmissing-noreturn			obsolescent
-Wmultiple-inheritance			c++ and objc++
-Wnamespaces				c++
-Wnoexcept				c++
-Wnoexcept-type				c++
-Wnon-template-friend			c++
-Wnon-virtual-dtor			c++
-Wnormalized				covered by -Wnormalized=
-Wnormalized=[none|id|nfc|nfkc]		handled specially by gl_MANYWARN_ALL_GCC
-Wold-style-cast			c++ and objc++
-Woverloaded-virtual			c++
-Woverride-init-side-effects		c++ and objc++
-Wpadded				FIXME maybe?  warns about "stabil" member in /usr/include/bits/timex.h
-Wpedantic				FIXME: too strict?
-Wplacement-new				c++
-Wplacement-new=<0,2>			c++
-Wpmf-conversions			c++ and objc++
-Wproperty-assign-default		objc++
-Wprotocol				objc++
-Wreal-q-constant			fortran
-Wrealloc-lhs				fortran
-Wrealloc-lhs-all			fortran
-Wredundant-decls			FIXME maybe? many _gl_cxxalias_dummy FPs
-Wregister				c++ and objc++
-Wreorder				c++ and objc++
-Wselector				objc and objc++
-Wshadow-ivar				objc
-Wshadow=compatible-local		covered by -Wshadow
-Wshadow-compatible-local		covered by -Wshadow
-Wshadow=global				covered by -Wshadow
-Wshadow=local				covered by -Wshadow
-Wshadow-local				covered by -Wshadow
-Wshift-overflow			covered by -Wshift-overflow=2
-Wshift-overflow=<0,2>			gcc --help=warnings artifact
-Wsign-compare				FIXME maybe? borderline.  some will want this
-Wsign-conversion			FIXME maybe? borderline.  some will want this
-Wsign-promo				c++ and objc++
-Wsized-deallocation			c++ and objc++
-Wstack-usage=<number>			FIXME: choose something sane?
-Wstrict-aliasing=<0,3>			FIXME: choose something sane?
-Wstrict-null-sentinel			c++ and objc++
-Wstrict-overflow=<0,5>			FIXME: choose something sane?
-Wstrict-selector-match			objc and objc++
-Wstringop-overflow			covered by -Wstringop-overflow=
-Wstringop-overflow=<0,4>		handled specially by gl_MANYWARN_ALL_GCC
-Wsubobject-linkage			c++ and objc++
-Wsuggest-override			c++ and objc++
-Wsurprising				fortran
-Wswitch-default			https://lists.gnu.org/r/bug-gnulib/2018-05/msg00179.html
-Wswitch-enum				FIXME maybe? borderline.  some will want this
-Wsynth					deprecated
-Wtabs					fortran
-Wtarget-lifetime			fortran
-Wtemplates				c++ and objc++
-Wterminate				c++ and objc++
-Wtraditional				obsolescent
-Wtraditional-conversion		obsolescent
-Wundeclared-selector			objc and objc++
-Wundef					FIXME maybe? too many false positives
-Wundefined-do-loop			fortran
-Wunderflow				fortran
-Wunreachable-code			obsolescent no-op
-Wunsuffixed-float-constants		triggers warning in gnulib's timespec.h
-Wunused-const-variable			covered by -Wunusec-const-variable=2
-Wunused-const-variable=<0,2>		gcc --help=warnings artifact
-Wunused-dummy-argument			fortran
-Wuse-without-only			fortran
-Wuseless-cast				c++ and objc++
-Wvirtual-inheritance			c++
-Wvirtual-move-assign			c++
-Wvla-larger-than=<number>		handled specially by gl_MANYWARN_ALL_GCC
-Wzero-as-null-pointer-constant		c++ and objc++
-Wzerotrip				fortran
-frequire-return-statement		go
