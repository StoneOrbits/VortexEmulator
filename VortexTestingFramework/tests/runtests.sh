#!/bin/bash

#VALGRIND="valgrind --quiet --leak-check=full --show-leak-kinds=all"
VALGRIND=""
VORTEX="../vortex"
DIFF="diff"

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

rm -rf tmp
mkdir tmp

ALLSUCCES=1

FILES=*.test
NUMFILES="$(echo $FILES | wc -w)"

echo -e "\e[33m== [\e[97mRUNNING $NUMFILES INTEGRATION TESTS\e[33m] ==\e[0m"

for FILE in $FILES; do
  INPUT="$(grep "Input=" $FILE | cut -d= -f2)"
  BRIEF="$(grep "Brief=" $FILE | cut -d= -f2)"
  TESTNUM="$(echo $FILE | cut -d_ -f1)"
  echo -e -n "\e[33mTesting $TESTNUM [\e[97m$BRIEF\e[33m]... \e[0m"
  DIVIDER=$(grep -n -- "--------------------------------------------------------------------------------" $FILE | cut -f1 -d:)
  EXPECTED="tmp/${FILE}.expected"
  OUTPUT="tmp/${FILE}.output"
  DIFFOUT="tmp/${FILE}.diff"
  tail -n +$(($DIVIDER + 1)) "$FILE" &> $EXPECTED
  $VALGRIND $VORTEX -t <<< $INPUT &> $OUTPUT
  $DIFF --brief $EXPECTED $OUTPUT &> $DIFFOUT
  if [ $? -eq 0 ]; then
    echo -e "\e[32mSUCCESS\e[0m"
  else
    echo -e "\e[31mFAILURE\e[0m"
    ALLSUCCES=0
    break
  fi
done

# check if all test succeeded
if [ $ALLSUCCES -eq 1 ]; then
  echo -e "\e[33m== [\e[32mSUCCESS ALL TESTS PASSED\e[33m] ==\e[0m"
  # if so clear the tmp folder
  rm -rf tmp
else
  # otherwise cat the last diff
  $DIFF $EXPECTED $OUTPUT
fi
