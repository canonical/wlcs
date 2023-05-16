#!/bin/bash

set -e

LP_CREDS="$1"
if [ -z "${LP_CREDS}" ]; then
  echo "ERROR: pass the Launchpad credentials file as argument" >&2
  exit 2
fi

if [ -z "${RELEASE}" ]; then
  echo "ERROR: RELEASE environment variable needs to be set to the" >&2
  echo "  target Ubuntu version." >&2
  exit 1
fi

GIT_BRANCH=${GITHUB_REF-$( git rev-parse --abbrev-ref HEAD )}
# determine the patch release
if ! [[ "${GIT_BRANCH}" =~ ^(refs/(heads|tags)/)?(main|(release/|v)([0-9\.]+))$ ]]; then
  echo "ERROR: This script should only run on main or release tags" >&2
  echo "  or branches." >&2
  exit 3
fi

source <( python <<EOF
import os
from launchpadlib.launchpad import Launchpad

lp = Launchpad.login_anonymously("mir-ci",
                                 "production",
                                 version="devel")

ubuntu = lp.distributions["ubuntu"]
series = ubuntu.getSeries(name_or_version=os.environ['RELEASE'])

print("UBUNTU_SERIES={}".format(series.name))
print("UBUNTU_VERSION={}".format(series.version))
EOF
)

if [ -z "${UBUNTU_SERIES}" -o -z "${UBUNTU_VERSION}" ]; then
  echo "ERROR: \"${RELEASE}\" was not recognized as a valid Ubuntu series" >&2
  exit 2
fi

GIT_REVISION=$( git rev-parse --short HEAD )

if [[ "${GIT_BRANCH}" =~ ^(refs/(heads|tags)/)?(release/|v)([0-9\.]+)$ ]]; then
  # we're on a release branch
  TARGET_PPA=ppa:mir-team/rc
  WLCS_SERIES=${BASH_REMATCH[4]}
  if [[ "$( git describe --tags --exact-match )" =~ ^v([0-9\.]+)$ ]]; then
    # this is a final release, use the tag version
    WLCS_VERSION=${BASH_REMATCH[1]}
  else
    # find the last tagged patch version
    PATCH_VERSION=$( git describe --abbrev=0 --match "v${WLCS_SERIES}*" \
                     2> /dev/null | sed 's/^v//')
    if [ -z "${PATCH_VERSION}" ]; then
      # start with patch version 0
      WLCS_VERSION=${WLCS_SERIES}.0
    else
      # increment the patch version
      WLCS_VERSION=$( echo ${PATCH_VERSION} | perl -pe 's/^((\d+\.)*)(\d+)$/$1.($3+1)/e' )
    fi

    # use the number of commits since main
    GIT_COMMITS=$( git rev-list --count origin/main..HEAD )
    WLCS_VERSION=${WLCS_VERSION}~rc${GIT_COMMITS}.git${GIT_REVISION}
  fi
else
  # look for a release tag within parents 2..n
  PARENT=2
  while git rev-parse HEAD^${PARENT} >/dev/null 2>&1; do
    if [[ "$( git describe --exact-match HEAD^${PARENT} )" =~ ^v([0-9\.]+)$ ]]; then
      # copy packages from ppa:mir-team/rc to ppa:mir-team/release
      RELEASE_VERSION=${BASH_REMATCH[1]}-0ubuntu${UBUNTU_VERSION}
      echo "Copying wlcs_${RELEASE_VERSION} from ppa:mir-team/rc to ppa:mir-team/release…"
      python - ${RELEASE_VERSION} <<EOF
import os
import sys

from launchpadlib.credentials import (RequestTokenAuthorizationEngine,
                                      UnencryptedFileCredentialStore)
from launchpadlib.launchpad import Launchpad

try:
  lp = Launchpad.login_with(
    "mir-ci",
    "production",
    version="devel",
    authorization_engine=RequestTokenAuthorizationEngine("production",
                                                         "mir-ci"),
    credential_store=UnencryptedFileCredentialStore(
      os.path.expanduser("${LP_CREDS}")
    )
  )
except NotImplementedError:
  raise RuntimeError("Invalid credentials.")

ubuntu = lp.distributions["ubuntu"]
series = ubuntu.getSeries(name_or_version=os.environ['RELEASE'])

mir_team = lp.people["mir-team"]
rc_ppa = mir_team.getPPAByName(name="rc")
release_ppa = mir_team.getPPAByName(name="release")

release_ppa.copyPackage(source_name="wlcs",
                        version=sys.argv[1],
                        from_archive=rc_ppa,
                        from_series=series.name,
                        from_pocket="Release",
                        to_series=series.name,
                        to_pocket="Release",
                        include_binaries=True)

EOF
      break
    fi
    PARENT=$(( ${PARENT} + 1 ))
  done

  # upload to dev PPA
  TARGET_PPA=ppa:mir-team/dev
  GIT_VERSION=$( git describe | sed 's/^v//' )
  WLCS_VERSION=${GIT_VERSION/-/+dev}
fi

PPA_VERSION=${WLCS_VERSION}-0ubuntu${UBUNTU_VERSION}

echo "Setting version to:"
echo "  ${PPA_VERSION}"

debchange \
  --newversion ${PPA_VERSION} \
  --force-bad-version \
  "Automatic build of revision ${GIT_REVISION}"

debchange \
  --release \
  --distribution ${UBUNTU_SERIES} \
  --force-distribution \
  "" # required for debchange to not open an editor

git archive --format=tar --prefix=wlcs-${WLCS_VERSION}/ HEAD \
  | xz -9 > ../wlcs_${WLCS_VERSION}.orig.tar.xz

dpkg-buildpackage \
    -I".git" \
    -I"build" \
    -i"^.git|^build" \
    -d -S

dput ${TARGET_PPA} ../wlcs_${PPA_VERSION}_source.changes
