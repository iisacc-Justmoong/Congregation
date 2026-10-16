# File creation ownership in Congregation

Profile processing, painting caches/staging and application storage directories delegate creation to provider devices; document and drawing formats remain domain-owned.

The common [ownership contract](../../../SDK/iiFileProvider/docs/FILE_OWNERSHIP.md) defines stream, staging, control-file and
installed-package rules. Source regression tests remain with this module; the
workspace creation audit also checks these boundaries. Build/deployment and target
runtime validation are separate evidence.

Changed boundaries:


- `src/App/main.cpp`
- `src/App/models/painting/drawingsurfaceitem.cpp`
- `src/App/models/painting/drawingsurfaceitem.h`
- `src/App/models/preferences/applicationpreferences.cpp`
- `src/App/models/profile/profileimageprocessor.cpp`
- `src/App/models/profile/profileimageprocessor.h`
