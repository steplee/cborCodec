project('irn', ['cpp'],
    default_options : [
      'cpp_std=c++17',
      'buildtype=debugoptimized',
      'optimization=2',
      'b_lto=false',
      ]
    )
add_project_arguments(['-std=c++17'], language : 'cuda')


set_warnings = [ '-Wall', '-Werror=return-type', '-Wno-reorder', '-Wno-unused_function', '-Wno-unused-function',
  '-Wno-unused-variable', '-Wno-unused-but-set-variable', '-Wno-deprecated-declarations', ]
add_project_arguments(set_warnings, language : 'cpp')

base_inc = include_directories('./src/')

base_dep = declare_dependency(
  include_directories: base_inc
  )


executable('testParser',
  files('test/testParser.cc'),
  dependencies: [base_dep])

executable('testEncoder',
  files('test/testEncoder.cc'),
  dependencies: [base_dep])
