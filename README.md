# gstreamer
GStreamer sample programs, samples of necessary use-cases.

## Build
`make` creates `multi-src` and `multi-sink`.

`multi-src` mixes three `videotestsrc` elements into a single `autovideosink` using `videomixer`.

`multi-src` demuxes three videos in one .mkv file using `matroskademux` and plays them in three different `autovideosink` elements.
