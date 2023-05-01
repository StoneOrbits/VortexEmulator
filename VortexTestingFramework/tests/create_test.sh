#!/bin/bash

VORTEX="$(pwd)/../vortex"
OUTPUT_FILE="recorded_input.txt"

# Color definitions
RED="$(tput setaf 1)"
GREEN="$(tput setaf 2)"
YELLOW="$(tput setaf 3)"
WHITE="$(tput setaf 7)"
NC="$(tput sgr0)" # No Color

REPOS=(
  "core"
  "gloves"
  "orbit"
  "handle"
  "finger"
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

# a helper function to insert w10 and w100 between characters in the input string
function insert_w10_w100() {
  local input_string="$1"
  local output_string=""
  local length=${#input_string}
  local last_index=$((length - 1))

  for (( i=0; i<$length; i++ )); do
      char=${input_string:$i:1}
      
      if [[ $char =~ [0-9] ]]; then
          output_string+="$char"
      else
          if [ $i -ne 0 ]; then
              if [ $i -eq $last_index ]; then
                  output_string+="w100"
              else
                  output_string+="w10"
              fi
          fi
          output_string+="$char"
      fi
  done

  echo "$output_string"
}

# select the target repo to create a test for
if [ -z $1 ]; then
  TARGETREPO=$(select_repo)
else
  TARGETREPO=$1
fi

mkdir $TARGETREPO

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

# =====================================================================
#  repeat everything below here if they say yes to making another test
while true; do

  # Run the Vortex program
  $VORTEX --color --in-place --record
  
  # Check if the output file exists and read the result from it
  if [ ! -f "$OUTPUT_FILE" ]; then
    echo "Output file not found"
  	exit
  fi
  
  RESULT=$(cat "$OUTPUT_FILE")
  echo -n "${YELLOW}Use Result [${WHITE}$RESULT${YELLOW}]? (Y/n): ${WHITE}"
  
  read -e CONFIRM
  if [[ $CONFIRM == [nN] || $CONFIRM == [nN][oO] ]]; then
    exit
  fi
  
  NEW_INPUT=$(insert_w10_w100 "$RESULT")
  
  echo -e "\n${WHITE}================================================================================${NC}"
  echo -e "Processed Input: ${WHITE}$NEW_INPUT${NC}"
  
  # Prompt for test name and description
  echo -en "${YELLOW}Enter the name of the test:${WHITE} "
  read -e TEST_NAME
  echo -en "${YELLOW}Enter the description:${WHITE} "
  read -e DESCRIPTION
  
  # replace spaces with underscores
  TEST_NAME="${TEST_NAME// /_}"

  # cd to test dir
  cd $TARGETREPO
  
  # Create the test file with an incremented prefix number
  PREFIX=0
  MAX_PREFIX=0
  for file in [0-9][0-9][0-9][0-9]*.test; do
    PREFIX_NUM="${file%%_*}"
    if [[ "$PREFIX_NUM" =~ ^[0-9]+$ ]]; then
      if ((10#$PREFIX_NUM > 10#$MAX_PREFIX)); then
        MAX_PREFIX=$PREFIX_NUM
      fi
    fi
  done
  NEXT_PREFIX=$((10#$MAX_PREFIX + 1))
  TEST_FILE="$(printf "%04d" $NEXT_PREFIX)_${TEST_NAME}.test"
  
  # Write the test information to the test file
  echo "Input=${NEW_INPUT}" > "$TEST_FILE"
  echo "Brief=${DESCRIPTION}" >> "$TEST_FILE"
  echo "--------------------------------------------------------------------------------" >> "$TEST_FILE"
  
  # generate the history for the test and append it to the test file
  echo "${NEW_INPUT}" | $VORTEX --no-timestep >> "$TEST_FILE"

  # cd back
  cd -
  
  # done
  echo "Test file created: ${TEST_FILE}"
  
  # run again?
  echo -n "${YELLOW}Create another test? (Y/n): ${WHITE}"
  read -e CONFIRM
  if [[ $CONFIRM == [nN] || $CONFIRM == [nN][oO] ]]; then
    exit
  fi
  
# end while true
done
