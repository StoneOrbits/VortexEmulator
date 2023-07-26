#!/bin/bash

VALGRIND="valgrind --quiet --leak-check=full --show-leak-kinds=all"
VORTEX="../vortex"

VALIDATE=0

for arg in "$@"
do
    if [ "$arg" == "-v" ]; then
        VALIDATE=1
    fi
done

REPOS=(
  "core"
  "gloves"
  "orbit"
  "handle"
  "duo"
  "duo_basicpattern"
)

# Function to display colored text
colored() {
  printf "%s%s%s" "${!1}" "${2}" "${NC}"
}

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

# select the target repo to create a test for
TARGETREPO=$(select_repo)

mkdir -p $TARGETREPO

echo -e -n "\e[33mBuilding Vortex...\e[0m"
make -C ../ &> /dev/null
if [ $? -ne 0 ]; then
  echo -e "\e[31mFailed to build Vortex!\e[0m"
  exit
fi
if [ ! -x "$VORTEX" ]; then
  echo -e "\e[31mCould not find Vortex!\e[0m"
  exit
fi
echo -e "\e[32mSuccess\e[0m"

function record_tests() {
  PROJECT=$1

	FILES=

  rm -rf tmp/$PROJECT
  mkdir -p tmp/$PROJECT
	
	# Iterate through the test files
	for file in "$PROJECT"/*.test; do
	  # Check if the file exists
	  if [ -e "$file" ]; then
	    NUMFILES=$((NUMFILES + 1))
			FILES="${FILES} $file"	
	  fi
	done

  NUMFILES="$(echo $FILES | wc -w)"

	if [ $NUMFILES -eq 0 ]; then
		echo -e "\e[31mNo tests found in $PROJECT folder\e[0m"
		exit
	fi

  echo -e "\e[33m== [\e[31mRECORDING \e[97m$NUMFILES INTEGRATION TESTS\e[33m] ==\e[0m"

  for FILE in $FILES; do
    INPUT="$(grep "Input=" $FILE | cut -d= -f2)"
    BRIEF="$(grep "Brief=" $FILE | cut -d= -f2)"
    ARGS="$(grep "Args=" $FILE | cut -d= -f2)"
    echo -e -n "\e[31mRecording \e[33m[\e[97m$BRIEF\e[33m] \e[33m[\e[97m$ARGS\e[33m]...\e[0m"
    TEMP_FILE="tmp/${FILE}.out"
    # Append the output of the $VORTEX command to the temp file
    # NOTE: When recording the tests we don't use valgrind because
    #       the valgrind output should be clean anyway. But when running
    #       the test valgrind is used with --leak-check=full --show-leak-kinds=all
    echo "Input=${INPUT}" > "$TEMP_FILE"
    echo "Brief=${BRIEF}" >> "$TEMP_FILE"
    echo "Args=${ARGS}" >> "$TEMP_FILE"
    echo "--------------------------------------------------------------------------------" >> "$TEMP_FILE"
    $VORTEX $ARGS --no-timestep --hex <<< $INPUT >> $TEMP_FILE
    # Replace the original file with the modified temp file
    mv $TEMP_FILE $FILE
    echo -e "\e[96mOK\e[0m"
    # print out colorful if in verbose
    if [ "$VALIDATE" -eq 1 ]; then
      $VORTEX $ARGS --no-timestep --color <<< $INPUT
      echo -e "\e[31mRecorded \e[33m[\e[97m$BRIEF\e[33m] \e[33m[\e[97m$ARGS\e[33m]\e[0m"
      echo -en "${YELLOW}Is this correct? (Y/n):${WHITE} "
      read -e CONFIRM
      if [[ $CONFIRM == [nN] || $CONFIRM == [nN][oO] ]]; then
        exit
      fi
    fi
  done

  #rm -rf tmp/$PROJECT
}

record_tests $TARGETREPO
