vtk_add_test_cxx(vtksz3CxxTests tests
  NO_DATA NO_VALID NO_OUTPUT
  TestIncludeSZ3.cxx
  TestCompressSZ3.cxx
  TestSZ3DualVersion.cxx)
vtk_test_cxx_executable(vtksz3CxxTests tests)
