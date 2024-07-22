project('irn', ['cpp'],
    default_options : [
      'cpp_std=c++17',
      'buildtype=debugoptimized',
      'optimization=2',
      'b_lto=false',
      ]
    )
add_project_arguments(['-std=c++17'], language : 'cuda')


if get_option('build_tests').enabled()
  gtest = subproject('gtest', required: true)
  gtest_dep = gtest.get_variable('gtest_dep')
  gtest_main_dep = gtest.get_variable('gtest_main_dep')
endif

set_warnings = [ '-Wall', '-Werror=return-type', '-Wno-reorder', '-Wno-unused_function', '-Wno-unused-function',
  '-Wno-unused-variable', '-Wno-unused-but-set-variable', '-Wno-deprecated-declarations', ]
add_project_arguments(set_warnings, language : 'cpp')

base_inc = include_directories('./src/')

base_dep = declare_dependency(
  include_directories: base_inc
  )

cborCodec_dep = declare_dependency(
  include_directories: [base_inc]
  )


if get_option('build_tests').enabled()

  if false
    executable('testParser',
      files('oldTest/testParser.cc'),
      dependencies: [base_dep, cborCodec_dep])

    executable('testEncoder',
      files('oldTest/testEncoder.cc'),
      dependencies: [base_dep, cborCodec_dep])
  endif

  executable('tests',
    files(
      'test/test_encoder.cc',
      'test/test_parser.cc',
      'test/test_big_json.cc',
      ),
    dependencies: [gtest_main_dep, cborCodec_dep])

endif
