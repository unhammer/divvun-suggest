SWIG_INTERFACE = libdivvun.i

EXTRA_DIST = libdivvun.i std_unique_ptr.i setup.py

BUILT_SOURCES = libdivvun_wrap.cpp libdivvun.py
CLEANFILES = $(BUILT_SOURCES)

all-local: $(BUILT_SOURCES)
	@./mac-libreoffice-workaround

%_wrap.cpp %.py: %.i setup.py std_unique_ptr.i
	$(SWIG) -c++ -python -I$(top_srcdir)/src/lib -o $*_wrap.cpp -outdir . $(srcdir)/$*.i
	CPPFLAGS="$(CPPFLAGS)" CXXFLAGS="$(CXXFLAGS)" LDFLAGS="-Wl,-rpath,${prefix}/lib $(LDFLAGS)" $(PYTHON) setup.py build
# build_ext doesn't copy the .py to the build dir
# rpath in LDFLAGS so users don't have to set LD_LIBRARY_PATH

install-exec-local:
	$(PYTHON) setup.py install --prefix=$(DESTDIR)$(prefix)

uninstall-local:
	rm -f $(DESTDIR)$(libdir)/python*/site-packages/_libdivvun.*.so
	rm -f $(DESTDIR)$(libdir)/python*/site-packages/libdivvun.py
	rm -f $(DESTDIR)$(libdir)/python*/site-packages/__pycache__/libdivvun.*.pyc
	rm -f $(DESTDIR)$(libdir)/python*/site-packages/python_$(PACKAGE)-*.egg-info

clean-local:
	rm -rf build __pycache__

