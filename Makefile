#
# $Id$
#
# To make a distribution 'tar' image, do
#	make -f Makefile.mid tar
#

all:
	@echo "" ;\
	 echo "   You must do a ./Configure before you run make" ;\
	 echo ""

tar:
	@echo "Did you remember to do a make cleanall" ?
	make -f Makefile.mid tar
clean:
	-for d in * ;do \
	 if [ -d $$d -a -f $$d/Makefile ]; then \
		(echo $$d ; cd $$d ; make -f Makefile clean) ;\
	 else \
	  if [ -d $$d -a -f $$d/Makefile.mid ]; then \
		(echo $$d ; cd $$d ; make -f Makefile.mid clean; ) ;\
		rm -f */*.o */a.out */core ;\
		echo "MIGHT HAVE TO DELETE BINARY TARGETS MANUALLY" ;\
	  fi ;\
	 fi ;\
	 done

## cleanup all Makefiles readfy for distribution
# Cleanup everything ready to send out for distribution
cleanall:  clean
	rm -f config.cache Makefile.bak */Makefile.bak
	cp Makefile.dist Makefile
	@-for i in */Makefile.mid */*/Makefile.mid; do \
	  rm -f `echo $$i | sed 's/\.mid//'` ;\
	done
