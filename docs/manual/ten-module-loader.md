# <a name="6">6 - Ten Module Loader</a>
While not an inherent part of the Ten language, the Ten Module Loader
[libtml](https://github.com/ten-lang/libtml) implements module loading
functionality as an extension to the core Ten runtime.  The loader
supports both library and project modules, and can load script files,
native dynamic libraries, and data files.

## <a name="6.1">6.1 - Project Modules</a>
Project modules are those loaded from the current project and not
from an external library.  These are loaded with the module type
`pro`, so something like:

    def proMod: require"pro:path/to/module"

The module path is given relative to the script file from which
it's being loaded, and can't include `.` or `..`.  Forward slash
`/` should be used as the path delimiter, regardless of platform
and the file extension should be omitted.

## <a name="6.2">6.2 - Library Modules</a>
Library modules are those loaded from an external library.  These
use the `lib` module type.

    def libMod: require"lib:someLib#0.1.2/someMod"

The module path for libraries is a bit more complex than for
project modules.  The first component specifies the library
name, in this case `someLib`.  Following this is an optional
version number (marked by a `#`) specifying one to three semantic
version components; the loader will use the highest version
available with the first components matching those provided. And
following the version number is an optional module path, relative
to the library directory.  If no path is specified then a module
of the same name as the library at the librarie's root is used.

Ten will search for modules in all known library directories, which
should have a tree structure something like:

    libs
        + library1
            - 0-1-0
               ...
            - 0-1-1
               ...
            - 1-0-0
               ...
        + library2
            - 0-0-0
               ...


Library directories contain a child directory for each library, within
which is a version directory for each available version.  The library
files go in the version directories.  Library directories will typically
have paths like `**/lib/ten` or `**/lib64/ten` on Unix systems.


## <a name="6.3">6.3 - Script Modules</a>
Script modules are normal Ten scripts with extension `.ten`, which export
components to the importer by putting them in an `export` record, provided
by the loader.  So something like:

    def export.fun1: [ ... ]
      ...

    def export.var1: ...

## <a name="6.4">6.4 - Native Modules</a>
Native modules are dynamic libraries (extension `.so` on Unix) that
define a function:

    void
    tml_export( ten_State* ten, ten_Var* export );

The loader will pass a Ten instance and an `export` variable initialized
with an empty record, and which this function should fill with the
module's exported values.

## <a name="6.5">6.5 - Data Modules</a>
Data modules are just loaded in full as a string.  They can have file
extension `.txt`, `.dat`, or `LANG.str`; where `LANG` is the three [letter
language code](https://www.loc.gov/standards/iso639-2/php/code_list.php)
for some language.  The language specific data modules will only be loaded
if their language code matches the language of the running system.

[Next: Ten CLI](ten-cli.md)
