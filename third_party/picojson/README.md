# picojson

Vendored `picojson.h` from https://github.com/kazuho/picojson/tree/v1.3.0
(tag v1.3.0). BSD license and copyright are retained in the header.
`src/npno_release.hpp` enables the upstream int64 option to preserve large integer
metadata. Used only by the control-side NPNO release updater, never the audio callback.
