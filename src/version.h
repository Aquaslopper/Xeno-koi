#ifndef XENO_KOI_VERSION_H
#define XENO_KOI_VERSION_H

/* The build's version string, in one place.
   The title screen prints it bottom-right, and the release page has to carry the same
   number - if the two disagree, a bug report cannot be matched to a build. Bump this in
   the same commit that tags a release.
   A CI job that already knows the tag can override it:
       -DXENO_KOI_VERSION='"V26"' */
#ifndef XENO_KOI_VERSION
#define XENO_KOI_VERSION "V25"
#endif

#endif
