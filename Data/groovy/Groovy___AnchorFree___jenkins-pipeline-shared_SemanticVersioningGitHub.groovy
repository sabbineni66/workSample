#!/usr/bin/env groovy
package com.anchorfree;

// https://semver.org

def call() {

    withCredentials([string(credentialsId: 'credentials_github_auth_token', variable: 'GITHUB_AUTH_TOKEN')]) {
        sh '''
set -eu

set +e
DESCRIBE=$(git describe --tags --long)
DESCRIBE_EXIT_CODE=$?

TAG_POINT_AT=$(git tag --points-at HEAD | sort --version-sort | tail -n 1)
set -e

if [ -n "${TAG_POINT_AT}" ]; then
    VERSION_OLD_FULL_RAW=${TAG_POINT_AT}
elif [ ${DESCRIBE_EXIT_CODE} -ne 0 ] || [ -z "${DESCRIBE}" ]; then
    VERSION_OLD_FULL_RAW="v0.0.0"
else
    VERSION_OLD_FULL_RAW=$(echo ${DESCRIBE} | awk '{split($0,a,"-"); print a[1]}')
fi

VERSION_OLD_FULL_CLEAN=$(echo ${VERSION_OLD_FULL_RAW} | tr -d v)
COMMITS_COUNT_NEW=$(echo ${DESCRIBE} | awk '{split($0,a,"-"); print a[2]}')
COMMIT_SHA_NEW=$(echo ${DESCRIBE} | awk '{split($0,a,"-"); print a[3]}')

VERSION_OLD_MAJOR=$(echo ${VERSION_OLD_FULL_CLEAN} | awk -F . '{print $1}' )
VERSION_OLD_MINOR=$(echo ${VERSION_OLD_FULL_CLEAN} | awk -F . '{print $2}' )
VERSION_OLD_PATCH=$(echo ${VERSION_OLD_FULL_CLEAN} | awk -F . '{print $3}' )

VERSION_NEW_MAJOR=${VERSION_OLD_MAJOR}
VERSION_NEW_MINOR=$((${VERSION_OLD_MINOR}+1))
VERSION_NEW_PATCH=0

VERSION_NEW_FULL_RAW="v${VERSION_NEW_MAJOR}.${VERSION_NEW_MINOR}.${VERSION_NEW_PATCH}"

TOKEN=${GITHUB_AUTH_TOKEN}
COMMIT=$(git rev-parse HEAD)
DATA='{
  "tag_name": "'${VERSION_NEW_FULL_RAW}'",
  "target_commitish": "'${COMMIT}'",
  "name": "'${VERSION_NEW_FULL_RAW}'",
  "body": "Semantic Versioning: Autoincrement",
  "draft": false,
  "prerelease": false
}'

GITHUB_ORG=$(echo $JOB_NAME | awk -F / '{ print $1}')
GITHUB_REPO=$(echo $JOB_NAME | awk -F / '{ print $2}')

# https://developer.github.com/v3/auth/#authenticating-for-saml-sso
# https://developer.github.com/v3/repos/releases/#create-a-release
curl -v --stderr response.txt -X POST -d "${DATA}" -H  "Authorization: token $TOKEN" https://api.github.com/repos/${GITHUB_ORG}/${GITHUB_REPO}/releases
grep "< Status: 201 Created" response.txt || cat response.txt
'''
    }

}

