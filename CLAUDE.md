# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

VTK uses CMake with out-of-source builds. Common workflow:

```bash
# Configure (use Ninja for faster builds)
mkdir build && cd build
ccmake -GNinja ../path/to/vtk/source

# Build
cmake --build .

# Run all tests
ctest -j$(nproc)

# Run a specific test
ctest -VV -R TestClassName

# Useful ctest flags
ctest -R TestNameSubstring    # Filter by name
ctest -E TestNameSubstring    # Exclude by name
ctest -I start,stop,step      # Run subset of tests
ctest -j N                    # Parallel execution
```

Key CMake options:
- `VTK_BUILD_TESTING=ON` - Enable tests
- `VTK_WRAP_PYTHON=ON` - Enable Python wrapping
- `VTK_USE_MPI=ON` - Enable MPI support
- `VTK_BUILD_ALL_MODULES=ON` - Build all modules
- `VTK_MODULE_ENABLE_<module>=YES|NO|WANT|DONT_WANT` - Control individual modules

## Architecture Overview

VTK is organized into a modular system where each module:
- Is defined in a `vtk.module` file declaring NAME, DEPENDS, PRIVATE_DEPENDS, OPTIONAL_DEPENDS
- Lives in its own directory with CMakeLists.txt and Testing subdirectory
- Follows the naming convention `VTK::<ModuleName>` (e.g., `VTK::CommonCore`)

### Core Directory Structure

| Directory | Purpose |
|-----------|---------|
| Common/ | Core classes: data model, execution model, math, transforms |
| Filters/ | Data processing algorithms |
| IO/ | File readers/writers for various formats |
| Rendering/ | Visualization and rendering backends |
| Interaction/ | User interaction (widgets, picking) |
| ThirdParty/ | Vendored dependencies (can use internal or external) |
| Wrapping/ | Python/Java wrapper generation |

### Key Base Classes

- `vtkObject` - Reference-counted base class with type info
- `vtkAlgorithm` - Base for all pipeline algorithms
- `vtkDataSet` / `vtkDataObject` - Data representation hierarchy
- `vtkMapper` / `vtkActor` - Rendering primitives

### Module Dependencies

Modules declare dependencies in `vtk.module`:
```
NAME
  VTK::ModuleName
DEPENDS
  VTK::RequiredModule
PRIVATE_DEPENDS
  VTK::ImplementationOnlyModule
OPTIONAL_DEPENDS
  VTK::OptionalFeature
TEST_DEPENDS
  VTK::TestingCore
```

## Coding Conventions

### Required Patterns

- All C++ code must be valid C++17
- Use Allman brace style (braces on new line)
- Two-space indentation, no tabs
- 100-character line limit preferred, 80 recommended
- Class names start with `vtk` prefix (e.g., `vtkMyClass`)
- Use `this->` for member access
- Protected constructors/destructors for vtkObject subclasses
- One public class per header file

### VTK-Specific Macros

```cpp
vtkTypeMacro(vtkMyClass, vtkBaseClass);    // Type info
vtkSetMacro(PropertyName, Type);           // Setter
vtkGetMacro(PropertyName, Type);           // Getter
vtkNew<vtkClassName> obj;                  // Preferred for stack-like allocation
```

### Namespace Requirements

```cpp
VTK_ABI_NAMESPACE_BEGIN
class VTKMODULE_EXPORT vtkMyClass : public vtkObject
{
  // class definition
};
VTK_ABI_NAMESPACE_END
```

### Documentation Style

```cpp
/**
 * @class vtkClassName
 * @brief One line description
 *
 * Longer description of class.
 */
```

### File Naming

- Class `vtkFoo` lives in `vtkFoo.h` and `vtkFoo.cxx`
- Test for `vtkClassName` is named `TestClassName.cxx` with entry point `TestClassName(int, char*[])`

## Testing

Tests live in `<Module>/Testing/Cxx/` (or `/Python/`). Test files are registered in `<Module>/Testing/Cxx/CMakeLists.txt`.

For tests using baseline images or data:
1. Add file names to module's `Testing/CMakeLists.txt`
2. Drop data files in `Testing/Data/`
3. Drop baselines in `<Module>/Testing/Data/Baselines/`
4. Build to convert files to `.sha512` references
5. Commit the `.sha512` files

## Contributing Workflow

1. Fork on GitLab (gitlab.kitware.com/vtk/vtk)
2. Run `./Utilities/SetupForDevelopment.sh` after cloning
3. Create topic branch
4. Make changes, add tests
5. Push with `git gitlab-push`
6. Create Merge Request
7. Robot may suggest `Do: reformat` for style fixes
8. Add changelog in `Documentation/release/dev/` for new features
9. CI must pass and need `+2` review before merge

## Formatting

VTK uses clang-format 16+ with Mozilla-based style. The GitLab CI robot can auto-fix formatting when you comment `Do: reformat` on an MR.
