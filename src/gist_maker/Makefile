include ../../Makefile.incl
PROJECT = gist_maker
PROJECTFLAGS = -DDEBUG=${DEBUG_LEVEL} -DI18N_ENABLED -DSCRIPT_DETECTION_ENABLED -DPROFANITY_CHECK_ENABLED -DKEYPHRASE_ENABLED -DNAMED_ENTITIES_ENABLED -DKEYWORDS_ENABLED -DINTENT_ENABLED -DSENTIMENT_ENABLED -DTEXT_CLASSIFICATION_ENABLED -DLANG_ENABLED -DLOCATION_ENABLED
ARCHIVE = ar -r

INCLUDE_DIR = -I. -I/usr/include/c++/4.4/ -I../../include/utfcpp -I../../extern/curl_request_maker -I../../extern/inagist_api -I../../extern/twitter_api -I../utils -I../script_detector -I../ngrams_generator -I../corpus_manager

LIBS = -l${PROJECT} -lngramsgenerator -lscriptdetector -lutils -ltwitterapi -linagistapi -lcurlrequestmaker -ljsonparser -lcurl

LIB = ${LIB_DIR}/lib${PROJECT}.so

BINS = ${BIN_DIR}/t_gist_maker

OBJS = gist_maker.o gist_maker_config.o

all: ${LIB} ${BINS}
	@echo "build done!"

clean:
	@echo "cleaning libraries ..."
	@rm *.o ${BINS} ${LIB}
	@echo "done!"

${LIB}: ${OBJS}
	@echo "creating library file ..."
	@${ARCHIVE} ${LIB} ${OBJS}
	@ranlib ${LIB} # this creates an index table
	@echo "done!"

.cc.o:
	@echo "compiling ..." $< "..."
	@${CXX} ${CXXFLAGS} ${PROJECTFLAGS} -shared -fPIC -c ${INCLUDE_DIR} $<

${BIN_DIR}/t_gist_maker: t_gist_maker.o ${LIB}
	@echo "creating binary:" $@
	@${CXX} ${CXXFLAGS} ${PROJECTFLAGS} $< ${LINK_DIR} ${LIBS} -o ${BIN_DIR}/t_gist_maker
	@echo "done!"

# DO NOT DELETE THIS LINE
