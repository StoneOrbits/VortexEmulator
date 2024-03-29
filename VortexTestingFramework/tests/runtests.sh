#!/bin/bash

VALGRIND=
VORTEX="../vortex"
DIFF="diff"

# select the target repo to create a test for
TARGETREPO=
VERBOSE=0
AUDIT=0
TODO=

REPOS=(
  "core"
  "gloves"
  "orbit"
  "handle"
  "duo"
  "duo_basicpattern"
)

for arg in "$@"
do
  if [ "$arg" == "-v" ]; then
    VALGRIND=
    VERBOSE=1
  fi
  if [ "$arg" == "-f" ]; then
    VALGRIND="valgrind --quiet --leak-check=full --show-leak-kinds=all"
  fi
  if [ "$arg" == "-a" ]; then
    AUDIT=1
    VERBOSE=1
    VALGRIND=
  fi
  if [[ $arg =~ ^-t=([0-9]*)$ ]]; then
    TODO="${BASH_REMATCH[1]}"
  fi
  for repo in "${REPOS[@]}"; do
    if [ "--${repo}" == "$arg" ]; then
      echo "Repo = $repo"
      TARGETREPO="$repo"
      break
    fi
  done
done

select_repo() {
  local original_PS3=$PS3
  local repo

  PS3='Please choose a repository: '

  select repo in "${REPOS[@]}"; do
    if [ -n "$repo" ]; then
      break
    fi
  done

  PS3=$original_PS3

  echo $repo
}

if [ -z "$TARGETREPO" ]; then
  TARGETREPO=$(select_repo)
fi

function run_tests() {
  PROJECT=$1

  ALLSUCCES=1

	# Initialize a counter
	NUMFILES=0
	FILES=
	
	# Iterate through the test files
	for file in "$PROJECT"/*.test; do
	  # Check if the file exists
	  if [ -e "$file" ]; then
	    NUMFILES=$((NUMFILES + 1))
			FILES="${FILES} $file"	
	  fi
	done

	if [ $NUMFILES -eq 0 ]; then
		echo -e "\e[31mNo tests found in $PROJECT folder\e[0m"
		exit
	fi

  echo -e "\e[33m== [\e[97mRUNNING $NUMFILES $PROJECT INTEGRATION TESTS\e[33m] ==\e[0m"

	# clear tmp folder
	rm -rf tmp/$PROJECT
	mkdir -p tmp/$PROJECT

  for FILE in $FILES; do
    INPUT="$(grep "Input=" $FILE | cut -d= -f2)"
    BRIEF="$(grep "Brief=" $FILE | cut -d= -f2)"
    ARGS="$(grep "Args=" $FILE | cut -d= -f2)"
    TESTNUM="$(echo $FILE | cut -d_ -f1 | cut -d/ -f2)"
    TESTNUM=$((10#$TESTNUM))
    if [ "$TODO" != "" ]; then
      if [ $TODO -ne $TESTNUM ]; then
        continue
      fi
    fi
    echo -e -n "\e[33mTesting $PROJECT $TESTNUM/$NUMFILES [\e[97m$BRIEF\e[33m] "
    if [ "$ARGS" != "" ]; then
      echo -e -n "[\e[97m$ARGS\e[33m] "
    fi
    echo -e -n "... \e[0m"
    DIVIDER=$(grep -n -- "--------------------------------------------------------------------------------" $FILE | cut -f1 -d:)
    EXPECTED="tmp/${FILE}.expected"
    OUTPUT="tmp/${FILE}.output"
    DIFFOUT="tmp/${FILE}.diff"
    tail -n +$(($DIVIDER + 1)) "$FILE" &> $EXPECTED
    # run again?
    if [ $AUDIT -eq 1 ]; then
      echo -n "${YELLOW}Begin test? (Y/n): ${WHITE}"
      read -e CONFIRM
      if [[ $CONFIRM == [nN] || $CONFIRM == [nN][oO] ]]; then
        break
      fi
      echo ""
      echo "-----------------------------"
      echo "Input: $INPUT"
      echo "Brief: $BRIEF"
      echo "Args: $ARGS"
      echo "Test: $TESTNUM"
      echo "-----------------------------"
    fi
    $VALGRIND $VORTEX $ARGS --no-timestep --hex <<< $INPUT &> $OUTPUT
    $DIFF --brief $EXPECTED $OUTPUT &> $DIFFOUT
    RESULT=$?
    if [ $VERBOSE -eq 1 ]; then
      $VORTEX $ARGS --no-timestep --color <<< $INPUT
    fi
    if [ $RESULT -eq 0 ]; then
      echo -e "\e[32mSUCCESS\e[0m"
    else
      echo -e "\e[31mFAILURE\e[0m"
      ALLSUCCES=0
      if [ "$VERBOSE" -eq 1 ]; then
        break
      fi
    fi
  done

  # check if all test succeeded
  if [ $ALLSUCCES -eq 1 ]; then
    echo -e "\e[33m== [\e[32mSUCCESS ALL TESTS PASSED\e[33m] ==\e[0m"
    # if so clear the tmp folder
    rm -rf tmp/$PROJECT
  else
    if [ "$VERBOSE" -eq 1 ]; then
      # otherwise cat the last diff
      $DIFF $EXPECTED $OUTPUT
    else
      echo -e "\e[31m== FAILURE ==\e[0m"
      exit 1
    fi
  fi
}

echo -e -n "\e[33mBuilding Vortex...\e[0m"
make -C ../ &> /dev/null
if [ $? -ne 0 ]; then
  echo -e "\e[31mFailed to build Vortex!\e[0m"
  exit 1
fi
if [ ! -x "$VORTEX" ]; then
  echo -e "\e[31mCould not find Vortex!\e[0m"
  exit 1
fi
echo -e "\e[32mSuccess\e[0m"

# select repo and run tests with it
run_tests $TARGETREPO
