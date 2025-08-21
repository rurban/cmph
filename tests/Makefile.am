TESTS = $(check_PROGRAMS) cmph_tests cmph_compile_tests
check_PROGRAMS = graph_tests select_tests compressed_seq_tests compressed_rank_tests cmph_benchmark_test
noinst_PROGRAMS = packed_mphf_tests mphf_tests
EXTRA_DIST = cmph_tests

AM_CPPFLAGS = -I$(top_srcdir)/src

cmph_tests: mphf_tests
graph_tests_SOURCES = graph_tests.c
graph_tests_LDADD = ../src/libcmph.la

packed_mphf_tests_SOURCES = packed_mphf_tests.c
packed_mphf_tests_LDADD = ../src/libcmph.la

mphf_tests_SOURCES = mphf_tests.c
mphf_tests_LDADD = ../src/libcmph.la

select_tests_SOURCES = select_tests.c
select_tests_LDADD = ../src/libcmph.la

compressed_seq_tests_SOURCES = compressed_seq_tests.c
compressed_seq_tests_LDADD = ../src/libcmph.la

compressed_rank_tests_SOURCES = compressed_rank_tests.c
compressed_rank_tests_LDADD = ../src/libcmph.la

cmph_benchmark_test_SOURCES = cmph_benchmark_test.c
cmph_benchmark_test_LDADD = ../src/libcmph.la

CLEANFILES = test-suite.log _words _words.mph _words100 _words100.mph TAGS \
  cmph_compile_run.o cmph_compile_run
